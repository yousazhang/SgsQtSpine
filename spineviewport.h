#pragma once
#include <QQuickFramebufferObject>
#include <QVariantList>

class SpineViewport : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList units READ units WRITE setUnits)
public:
    SpineViewport();
    Renderer* createRenderer() const override;

    QVariantList units() const { return m_units; }
    void setUnits(const QVariantList& u) { m_units = u; update(); }

    Q_INVOKABLE void trigger(const QVariantList& commands);
    Q_INVOKABLE void tick() { update(); }
    QVariantList takeCommands();

private:
    QVariantList m_units;
    QVariantList m_pendingCommands;
};
