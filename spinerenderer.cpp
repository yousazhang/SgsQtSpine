#include "spinerenderer.h"
#include "spineviewport.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QMatrix4x4>

SpineRenderer::SpineRenderer(const QVariantList& configs) {
    for (auto c : configs) {
        QVariantMap m = c.toMap();

        Unit u;
        u.name = m.value("name").toString();
        const QString atlasPath = m.value("atlas").toString();
        const QString skelPath  = m.value("skel").toString();

        if (u.name.isEmpty() || atlasPath.isEmpty() || skelPath.isEmpty()) {
            qWarning() << "[SpineRenderer] invalid unit config:" << m;
            continue;
        }

        u.atlas = spAtlas_createFromFile(atlasPath.toUtf8().constData(), nullptr);
        if (!u.atlas) {
            qWarning() << "[SpineRenderer] failed to load atlas:" << atlasPath;
            continue;
        }

        u.binary = spSkeletonBinary_create(u.atlas);
        u.binary->scale = m.value("scale", 1.0).toFloat();

        QFileInfo fi(skelPath);
        if (!fi.exists() || fi.size() <= 0) {
            qWarning() << "[SpineRenderer] missing/empty skel:" << skelPath;
            continue;
        }

        QFile f(skelPath);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "[SpineRenderer] cannot open skel:" << skelPath;
            continue;
        }
        QByteArray bytes = f.readAll();
        f.close();

        u.data = spSkeletonBinary_readSkeletonData(
            u.binary,
            reinterpret_cast<const unsigned char*>(bytes.constData()),
            bytes.size()
        );
        if (!u.data) {
            qWarning() << "[SpineRenderer] failed to parse skel:" << skelPath
                       << "error:" << (u.binary && u.binary->error ? u.binary->error : "unknown");
            continue;
        }

        u.skeleton = spSkeleton_create(u.data);
        spSkeleton_setToSetupPose(u.skeleton);
        spSkeleton_updateWorldTransform(u.skeleton);

        u.stateData = spAnimationStateData_create(u.data);
        u.state = spAnimationState_create(u.stateData);

        u.x = m.value("x", 0.0).toFloat();
        u.y = m.value("y", 0.0).toFloat();
        //u.pma = m.value("pma", false).toBool();
        u.skeleton->x = u.x;
        u.skeleton->y = u.y;

        const QString defAnim = m.value("defaultAnim").toString();
        if (!defAnim.isEmpty()) {
            QByteArray a = defAnim.toUtf8();
            spAnimation* anim = spSkeletonData_findAnimation(u.data, a.constData());
            if (anim) spAnimationState_setAnimation(u.state, 0, anim, 1);
        }

        m_units.push_back(u);
        qWarning() << "[SpineRenderer] loaded unit:" << u.name;
    }
}

SpineRenderer::~SpineRenderer() {
    if (m_clipper) { spSkeletonClipping_dispose(m_clipper); m_clipper = nullptr; }
    for (auto &u : m_units) {
        if (u.state) spAnimationState_dispose(u.state);
        if (u.stateData) spAnimationStateData_dispose(u.stateData);
        if (u.skeleton) spSkeleton_dispose(u.skeleton);
        if (u.data) spSkeletonData_dispose(u.data);
        if (u.binary) spSkeletonBinary_dispose(u.binary);
        if (u.atlas) spAtlas_dispose(u.atlas);
    }
}

void SpineRenderer::synchronize(QQuickFramebufferObject* item) {
    auto viewport = static_cast<SpineViewport*>(item);
    m_pendingCommands = viewport->takeCommands();
}

QOpenGLFramebufferObject* SpineRenderer::createFramebufferObject(const QSize& size) {
    m_fboSize = size;
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    return new QOpenGLFramebufferObject(size, format);
}

void SpineRenderer::initGlIfNeeded() {
    if (m_glInited) return;

    initializeOpenGLFunctions();

    m_clipper = spSkeletonClipping_create();

    const char* vs = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        layout(location=2) in vec4 aColor;
        uniform mat4 uMvp;
        out vec2 vUV;
        out vec4 vColor;
        void main(){
            gl_Position = uMvp * vec4(aPos, 0.0, 1.0);
            vUV = aUV;
            vColor = aColor;
        }
    )";

    const char* fs = R"(
        #version 330 core
        in vec2 vUV;
        in vec4 vColor;
        uniform sampler2D uTex;
        out vec4 FragColor;
        void main(){
            vec4 tex = texture(uTex, vUV);
            vec4 c = tex * vColor;
            c.rgb *= c.a; // premultiply for Qt Quick composition
            FragColor = c;
        }
    )";

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vs);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    if (!m_program.link()) qWarning() << "Shader link failed:" << m_program.log();

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_ibo.create();
    m_ibo.bind();
    m_ibo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_program.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, r));
    m_program.release();

    m_ibo.release();
    m_vbo.release();
    m_vao.release();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_glInited = true;
}

void SpineRenderer::setBlend(int blendMode, bool premultipliedAlpha) {
    glEnable(GL_BLEND);
    if (premultipliedAlpha) {
        switch (blendMode) {
        case SP_BLEND_MODE_NORMAL:   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); break;
        case SP_BLEND_MODE_ADDITIVE: glBlendFunc(GL_ONE, GL_ONE); break;
        case SP_BLEND_MODE_MULTIPLY: glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA); break;
        case SP_BLEND_MODE_SCREEN:   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); break;
        default:                     glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); break;
        }
    } else {
        switch (blendMode) {
        case SP_BLEND_MODE_NORMAL:   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
        case SP_BLEND_MODE_ADDITIVE: glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
        case SP_BLEND_MODE_MULTIPLY: glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA); break;
        case SP_BLEND_MODE_SCREEN:   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); break;
        default:                     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
        }
    }
}

SpineRenderer::Unit* SpineRenderer::findUnit(const QString& name) {
    for (auto &u : m_units) if (u.name == name) return &u;
    return nullptr;
}

void SpineRenderer::executeTrigger(const QVariantList& commands) {
    for (auto cmd : commands) {
        QVariantMap m = cmd.toMap();
        const QString unitName = m.value("unit").toString();
        auto* u = findUnit(unitName);
        if (!u) continue;

        const QStringList queue = m.value("queue").toStringList();
        const int track = m.value("track", 0).toInt();
        const bool loopLast = m.value("loopLast", true).toBool();

        for (int i = 0; i < queue.size(); ++i) {
            const bool isLast = (i == queue.size() - 1);
            const bool loop = isLast ? loopLast : false;

            QByteArray a = queue[i].toUtf8();
            spAnimation* anim = spSkeletonData_findAnimation(u->data, a.constData());
            if (!anim) continue;

            if (i == 0) spAnimationState_setAnimation(u->state, track, anim, loop ? 1 : 0);
            else spAnimationState_addAnimation(u->state, track, anim, loop ? 1 : 0, 0);
        }
    }
}

void SpineRenderer::drawUnit(Unit& u)
{
    const bool premultipliedAlpha = false;
    if (!m_clipper) return;

    // Start with no clipping.
    // Spine 3.6 recommends calling clipEnd2 at the end of the skeleton render.
    for (int i = 0; i < u.skeleton->slotsCount; ++i) {
        spSlot* slot = u.skeleton->drawOrder[i];
        if (!slot) continue;

        spAttachment* attachment = slot->attachment;

        // Handle clipping start
        if (attachment && attachment->type == SP_ATTACHMENT_CLIPPING) {
            spClippingAttachment* clip = (spClippingAttachment*)attachment;
            spSkeletonClipping_clipStart(m_clipper, slot, clip);
            // clipStart slot itself does not draw.
            spSkeletonClipping_clipEnd(m_clipper, slot);
            continue;
        }

        if (!attachment) {
            spSkeletonClipping_clipEnd(m_clipper, slot);
            continue;
        }

        float r = u.skeleton->color.r * slot->color.r;
        float g = u.skeleton->color.g * slot->color.g;
        float b = u.skeleton->color.b * slot->color.b;
        float a = u.skeleton->color.a * slot->color.a;
        spAtlasRegion* region = nullptr;

        // Build base (unclipped) geometry buffers in float arrays, then optionally clip.
        std::vector<float> baseXY;
        std::vector<float> baseUV;
        std::vector<unsigned short> baseIdx;

        if (attachment->type == SP_ATTACHMENT_REGION) {
            spRegionAttachment* att = (spRegionAttachment*)attachment;

            float world[8];
            spRegionAttachment_computeWorldVertices(att, slot->bone, world, 0, 2);
            const float* uvs = att->uvs;

            baseXY.assign(world, world + 8);
            baseUV.assign(uvs, uvs + 8);
            baseIdx = {0,1,2, 2,3,0};

            region = (spAtlasRegion*)att->rendererObject;
        } else if (attachment->type == SP_ATTACHMENT_MESH) {
            spMeshAttachment* att = (spMeshAttachment*)attachment;

            int worldLen = att->super.worldVerticesLength; // floats count (x,y, x,y, ...)
            baseXY.resize((size_t)worldLen);
            spVertexAttachment_computeWorldVertices(&att->super, slot, 0, worldLen, baseXY.data(), 0, 2);

            int uvLen = worldLen;
            baseUV.resize((size_t)uvLen);
            // UVs are stored as floats [u,v,...] same length as world vertices
            for (int k = 0; k < uvLen; ++k) baseUV[(size_t)k] = att->uvs[k];

            baseIdx.resize((size_t)att->trianglesCount);
            for (int t = 0; t < att->trianglesCount; ++t) baseIdx[(size_t)t] = (unsigned short)att->triangles[t];

            region = (spAtlasRegion*)att->rendererObject;
        } else {
            spSkeletonClipping_clipEnd(m_clipper, slot);
            continue;
        }

        if (!region || !region->page || !region->page->rendererObject) {
            spSkeletonClipping_clipEnd(m_clipper, slot);
            continue;
        }
        // Qt Quick composites FBO as premultiplied alpha; shader premultiplies output.
        setBlend((int)slot->data->blendMode, true);

        // Apply clipping if active
        const bool clipping = spSkeletonClipping_isClipping(m_clipper) != 0;

        const float* finalXY = nullptr;
        const float* finalUV = nullptr;
        const unsigned short* finalIdx = nullptr;
        int finalVertFloats = 0;
        int finalIdxCount = 0;

        if (clipping) {
            // clipTriangles writes to m_clipper->clippedVertices/UVs/Triangles
            spSkeletonClipping_clipTriangles(
                m_clipper,
                baseXY.data(), (int)baseXY.size(),
                baseIdx.data(), (int)baseIdx.size(),
                baseUV.data(),
                2 // stride in vertices array (x,y)
            );

            finalXY = m_clipper->clippedVertices->items;
            finalUV = m_clipper->clippedUVs->items;
            finalIdx = m_clipper->clippedTriangles->items;
            finalVertFloats = m_clipper->clippedVertices->size; // floats count
            finalIdxCount = m_clipper->clippedTriangles->size;  // indices count
        } else {
            finalXY = baseXY.data();
            finalUV = baseUV.data();
            finalIdx = baseIdx.data();
            finalVertFloats = (int)baseXY.size();
            finalIdxCount = (int)baseIdx.size();
        }

        if (!finalXY || !finalUV || !finalIdx || finalVertFloats < 8 || finalIdxCount < 3) {
            spSkeletonClipping_clipEnd(m_clipper, slot);
            continue;
        }

        int vertCount = finalVertFloats / 2;

        m_verts.resize((size_t)vertCount);
        for (int v = 0; v < vertCount; ++v) {
            m_verts[(size_t)v] = Vtx{
                finalXY[v*2+0], finalXY[v*2+1],
                finalUV[v*2+0], finalUV[v*2+1],
                r,g,b,a
            };
        }

        m_indices.resize((size_t)finalIdxCount);
        for (int ii = 0; ii < finalIdxCount; ++ii) m_indices[(size_t)ii] = finalIdx[ii];

        auto* tex = static_cast<QOpenGLTexture*>(region->page->rendererObject);
        if (!tex) {
            spSkeletonClipping_clipEnd(m_clipper, slot);
            continue;
        }

        m_vbo.bind();
        m_vbo.allocate(m_verts.data(), int(m_verts.size() * sizeof(Vtx)));
        m_ibo.bind();
        m_ibo.allocate(m_indices.data(), int(m_indices.size() * sizeof(unsigned short)));

        glActiveTexture(GL_TEXTURE0);
        tex->bind();

        glDrawElements(GL_TRIANGLES, int(m_indices.size()), GL_UNSIGNED_SHORT, nullptr);

        tex->release();
        m_ibo.release();
        m_vbo.release();

        // This ends clipping when the clip's end slot is reached.
        spSkeletonClipping_clipEnd(m_clipper, slot);
    }

    spSkeletonClipping_clipEnd2(m_clipper);
}

void SpineRenderer::render() {
    initGlIfNeeded();

    if (!m_pendingCommands.isEmpty()) {
        executeTrigger(m_pendingCommands);
        m_pendingCommands.clear();
    }

    glViewport(0, 0, m_fboSize.width(), m_fboSize.height());
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Compute real delta time (Qt Quick may render at 60/120/144Hz)
    if (!m_timer.isValid()) {
        m_timer.start();
        m_lastNs = m_timer.nsecsElapsed();
    }
    const qint64 nowNs = m_timer.nsecsElapsed();
    float dt = float(nowNs - m_lastNs) / 1e9f;
    m_lastNs = nowNs;
    if (dt < 0.f) dt = 0.f;
    if (dt > 0.05f) dt = 0.05f; // clamp (tab switch / debugging)

    for (auto &u : m_units) {
        if (!u.state || !u.skeleton) continue;
        spAnimationState_update(u.state, dt);
        spAnimationState_apply(u.state, u.skeleton);
        spSkeleton_updateWorldTransform(u.skeleton);
    }

    QMatrix4x4 mvp;
    float w = float(m_fboSize.width());
    float h = float(m_fboSize.height());
    mvp.ortho(-w*0.5f, w*0.5f, -h*0.5f, h*0.5f, -1.f, 1.f);

    m_program.bind();
    m_program.setUniformValue("uMvp", mvp);
    m_program.setUniformValue("uTex", 0);
    m_vao.bind();

    for (auto &u : m_units) {
        if (u.skeleton) drawUnit(u);
    }

    m_vao.release();
    m_program.release();
}
