#include "spineviewport.h"
#include "spinerenderer.h"

SpineViewport::SpineViewport() { setMirrorVertically(true); }

QQuickFramebufferObject::Renderer* SpineViewport::createRenderer() const {
    return new SpineRenderer(m_units);
}

void SpineViewport::trigger(const QVariantList& commands) {
    m_pendingCommands = commands;
    update();
}

QVariantList SpineViewport::takeCommands() {
    QVariantList tmp = m_pendingCommands;
    m_pendingCommands.clear();
    return tmp;
}
