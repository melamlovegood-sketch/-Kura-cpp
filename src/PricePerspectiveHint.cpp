#include "PricePerspectiveHint.h"
#include "PricePerspective.h"
PricePerspectiveHint::PricePerspectiveHint(const QString &settingsPath,QWidget *parent):QLabel(parent),m_settingsPath(settingsPath) {
    setProperty("muted",true); setWordWrap(true); setTextFormat(Qt::PlainText); setMinimumWidth(0); hide();
}
void PricePerspectiveHint::setAmount(qint64 cents) { m_cents=cents; refresh(); }
void PricePerspectiveHint::refresh() {
    const auto result=PricePerspectiveStore::load(m_settingsPath).quote(m_cents);
    setText(result.primary+(result.secondary.isEmpty()?QString():"\n"+result.secondary)); setToolTip(result.basis); setVisible(!result.isEmpty());
    setAccessibleName(text()); updateGeometry();
}
