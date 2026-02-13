#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtQml>

#include "spineviewport.h"

int main(int argc, char *argv[])
{
    // QQuickFramebufferObject needs OpenGL backend on Qt6 (Windows default is often D3D)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);

    qmlRegisterType<SpineViewport>("MySpine", 1, 0, "SpineViewport");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}
