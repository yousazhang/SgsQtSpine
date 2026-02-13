#include <QImage>
#include <QOpenGLTexture>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <QDebug>

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
#include <spine/extension.h>
}

extern "C" char* _spUtil_readFile(const char* path, int* length) {
    QFile f(QString::fromUtf8(path));
    if (!f.exists()) f.setFileName("assets/" + QString::fromUtf8(path));
    if (!f.open(QIODevice::ReadOnly)) {
        if (length) *length = 0;
        return nullptr;
    }
    QByteArray data = f.readAll();
    f.close();

    char* buf = (char*)malloc((size_t)data.size() + 1);
    if (!buf) {
        if (length) *length = 0;
        return nullptr;
    }
    memcpy(buf, data.constData(), (size_t)data.size());
    buf[data.size()] = '\0';
    if (length) *length = data.size();
    return buf;
}

extern "C" void _spAtlasPage_createTexture(spAtlasPage* page, const char* path) {
    QImage img(QString::fromUtf8(path));
    if (img.isNull()) img = QImage("assets/" + QString::fromUtf8(path));

    if (img.isNull()) {
        qWarning() << "[atlas] FAILED texture:" << path;
        img = QImage(2, 2, QImage::Format_RGBA8888);
        img.fill(Qt::magenta);
    }

    img = img.convertToFormat(QImage::Format_RGBA8888);

    auto* tex = new QOpenGLTexture(img);
    tex->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    tex->setWrapMode(QOpenGLTexture::ClampToEdge);

    page->rendererObject = tex;
    page->width = tex->width();
    page->height = tex->height();
}

extern "C" void _spAtlasPage_disposeTexture(spAtlasPage* page) {
    delete static_cast<QOpenGLTexture*>(page->rendererObject);
    page->rendererObject = nullptr;
}
