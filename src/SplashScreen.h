#pragma once

#include <QPointer>
#include <QWidget>

class QVariantAnimation;

// The original web opening, painted as vectors over the already prepared window.
class SplashScreen final : public QWidget {
    Q_OBJECT
public:
    static SplashScreen *present(QWidget *window);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    explicit SplashScreen(QWidget *window);
    QVariantAnimation *m_animation;
    QPointer<QWidget> m_previousFocus;
    int m_elapsed = 0;
};
