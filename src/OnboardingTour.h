#pragma once
#include <QPointer>
#include <QWidget>
class QFrame;
class QLabel;
class QPushButton;

class OnboardingTour final : public QWidget {
    Q_OBJECT
public:
    explicit OnboardingTour(QWidget *parent);
    void start();
    int step() const { return m_step; }
    QRect spotlightRect() const { return m_spotlight; }
signals:
    void pageRequested(int page);
    void finished(bool completed);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
private:
    void showStep(int step);
    void advance();
    void positionBubble();
    int m_step=0;
    bool m_pressed=false;
    QPointer<QWidget> m_target;
    QRect m_spotlight;
    QFrame *m_bubble;
    QLabel *m_progress,*m_title,*m_body;
    QPushButton *m_previous,*m_next;
};
