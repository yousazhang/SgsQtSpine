#pragma once
#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QVariantList>
#include <QSize>
#include <QElapsedTimer>
#include <vector>

#ifdef slots
#  undef slots
#endif
#ifdef signals
#  undef signals
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif

extern "C" {
#include <spine/spine.h>
#include <spine/SkeletonClipping.h>
}

class SpineRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions {
public:
    explicit SpineRenderer(const QVariantList& configs);
    ~SpineRenderer() override;

    void render() override;
    void synchronize(QQuickFramebufferObject* item) override;
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;

private:
    struct Unit {
        QString name;
        spAtlas* atlas = nullptr;
        spSkeletonBinary* binary = nullptr;
        spSkeletonData* data = nullptr;
        spSkeleton* skeleton = nullptr;
        spAnimationStateData* stateData = nullptr;
        spAnimationState* state = nullptr;
        float x = 0.f;
        float y = 0.f;
};

    struct Vtx { float x,y,u,v,r,g,b,a; };

    std::vector<Unit> m_units;
    QVariantList m_pendingCommands;
    QSize m_fboSize;

    // timing for correct animation speed (vsync/variable refresh)
    QElapsedTimer m_timer;
    qint64 m_lastNs = 0;

    bool m_glInited = false;
    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_ibo{QOpenGLBuffer::IndexBuffer};
    QOpenGLVertexArrayObject m_vao;

    std::vector<Vtx> m_verts;
    std::vector<unsigned short> m_indices;

    spSkeletonClipping* m_clipper = nullptr;

    Unit* findUnit(const QString& name);
    void executeTrigger(const QVariantList& commands);

    void initGlIfNeeded();
    void setBlend(int blendMode, bool premultipliedAlpha);
    void drawUnit(Unit& u);
};
