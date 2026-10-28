#include "AchievementCelebration.h"
#include "AchievementArt.h"
#include "AppTheme.h"
#include "OnboardingDialog.h"
#include "SplashScreen.h"
#include <QApplication>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSqlQuery>
#include <QVariant>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace {
class ChestnutSeal final : public QWidget {
public:
    ChestnutSeal(const Achievement &badge, QWidget *parent, bool hero):QWidget(parent) {
        const auto definitions=AchievementRepository::definitions();
        for(int i=0;i<definitions.size();++i) if(definitions[i].key==badge.key) m_stamp=i;
        setFixedSize(hero?156:48,hero?142:48);
        setAccessibleName(badge.name+QStringLiteral("成就栗子"));
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.translate(width()/2.0,height()/2.0);p.scale(m_reveal,m_reveal);p.translate(-width()/2.0,-height()/2.0);
        const auto accent=AppTheme::color(this,"accent");
        if(width()>100) {
            auto halo=accent;halo.setAlpha(45);
            p.setBrush(Qt::NoBrush);p.setPen(QPen(halo,1));p.drawEllipse(QRectF(22,15,112,112));
            p.setPen(QPen(accent,1.4,Qt::SolidLine,Qt::RoundCap));
            for(const QPointF point:{QPointF(15,40),QPointF(138,93)}) {
                p.drawLine(point-QPointF(4,0),point+QPointF(4,0));
                p.drawLine(point-QPointF(0,4),point+QPointF(0,4));
            }
            p.setBrush(accent);p.setPen(Qt::NoPen);p.drawEllipse(QPointF(132,30),2,2);
        }
        const qreal size=width()>100?96:48;
        p.translate((width()-size)/2,(height()-size)/2);p.scale(size/64,size/64);
        AchievementArt::chestnut(p,m_stamp,true,accent,AppTheme::color(this,"inset"),AppTheme::color(this,"card"));
    }
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if(width()<100 || parentWidget()->window()->parentWidget()->property("reducedMotion").toBool()) return;
        auto *animation=new QVariantAnimation(this);
        animation->setDuration(480);animation->setStartValue(.86);animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::OutBack);
        connect(animation,&QVariantAnimation::valueChanged,this,[this](const QVariant &value) {m_reveal=value.toReal();update();});
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
private:
    int m_stamp=0;
    qreal m_reveal=1;
};

QLabel *text(const QString &value, QWidget *parent, const char *name, int size,
             const QString &role="ink")
{
    auto *label=new QLabel(value,parent);label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);label->setWordWrap(true);label->setAlignment(Qt::AlignCenter);
    const qreal scale=parent->window()->property("fontScale").toInt()/100.0;
    label->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;background:transparent;")
                         .arg(AppTheme::color(parent,role).name()).arg(qRound(size*qMax(1.0,scale))));
    return label;
}
}

AchievementCelebration::AchievementCelebration(const QList<Achievement> &badges,QWidget *parent)
    :QDialog(parent,Qt::Dialog|Qt::FramelessWindowHint),m_card(new QFrame(this))
{
    Q_ASSERT(!badges.isEmpty());
    setObjectName("achievementCelebration");setWindowTitle(QStringLiteral("收藏了新栗子"));
    setProperty("kuraDialog",true); // This card owns its shade, layout and close controls.
    setProperty("fontScale",parent->property("fontScale"));
    setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    m_card->setObjectName("achievementCelebrationCard");
    m_card->setStyleSheet(QStringLiteral("QFrame#achievementCelebrationCard {background:%1;border:1px solid %2;border-radius:22px;}")
                         .arg(AppTheme::color(parent,"card").name(),AppTheme::color(parent,"border").name()));
    auto *layout=new QVBoxLayout(m_card);layout->setContentsMargins(30,16,30,26);layout->setSpacing(12);
    auto *top=new QHBoxLayout;
    top->addStretch();
    auto *close=new QPushButton(QStringLiteral("×"),m_card);close->setObjectName("closeAchievementButton");
    close->setAccessibleName(QStringLiteral("关闭成就弹窗"));close->setToolTip(QStringLiteral("关闭 · Esc"));
    close->setFixedSize(30,30);close->setFlat(true);close->setCursor(Qt::PointingHandCursor);
    close->setStyleSheet("padding:0;min-height:0;font-size:22px;background:transparent;border:none;");
    top->addWidget(close);layout->addLayout(top);
    connect(close,&QPushButton::clicked,this,&QDialog::reject);

    auto *scroll=new QScrollArea(m_card);scroll->setObjectName("achievementCelebrationScroll");
    scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget {background:transparent;border:none;}");
    auto *body=new QWidget(scroll);auto *content=new QVBoxLayout(body);
    content->setContentsMargins(6,0,6,8);content->setSpacing(12);
    content->addWidget(text(badges.size()==1?QStringLiteral("收藏了一颗新栗子"):QStringLiteral("这次，收获了 %1 颗栗子").arg(badges.size()),this,"achievementCelebrationHeading",13,"accent"));
    content->addWidget(new ChestnutSeal(badges.first(),body,true),0,Qt::AlignHCenter);
    auto *title=text(badges.first().name,this,"achievementCelebrationTitle",26);
    title->setStyleSheet(title->styleSheet()+"font-weight:500;");content->addWidget(title);
    auto *message=text(QStringLiteral("“%1”").arg(badges.first().message),this,"achievementCelebrationMessage",18,"secondary");
    message->setStyleSheet(message->styleSheet()+"font-family:'LXGW WenKai Lite';");content->addWidget(message);
    content->addWidget(text(badges.first().condition,this,"achievementCelebrationCondition",12,"secondary"));
    for(int i=1;i<badges.size();++i) {
        auto *row=new QFrame(body);auto *line=new QHBoxLayout(row);line->setContentsMargins(4,10,4,0);line->setSpacing(12);
        line->addWidget(new ChestnutSeal(badges[i],row,false));
        auto *words=new QVBoxLayout;
        auto *name=text(badges[i].name,this,"achievementExtraTitle",15);name->setAlignment(Qt::AlignLeft);
        auto *note=text(badges[i].message,this,"achievementExtraMessage",12,"secondary");note->setAlignment(Qt::AlignLeft);
        words->addWidget(name);words->addWidget(note);line->addLayout(words,1);content->addWidget(row);
    }
    scroll->setWidget(body);layout->addWidget(scroll,1);
    auto *keep=new QPushButton(badges.size()==1?QStringLiteral("收好这颗栗子"):QStringLiteral("把栗子都收好"),m_card);
    keep->setObjectName("keepAchievementButton");keep->setProperty("primary",true);
    keep->setCursor(Qt::PointingHandCursor);keep->setDefault(true);layout->addWidget(keep);
    auto *collection=new QPushButton(QStringLiteral("去栗子收藏室看看  →"),m_card);
    collection->setObjectName("viewAchievementCollectionButton");collection->setFlat(true);
    collection->setCursor(Qt::PointingHandCursor);layout->addWidget(collection);
    connect(keep,&QPushButton::clicked,this,&QDialog::accept);
    connect(collection,&QPushButton::clicked,this,[this] { accept();emit collectionRequested(); });
    parent->installEventFilter(this);
    fit();keep->setFocus();
}

void AchievementCelebration::fit()
{
    auto *owner=parentWidget();
    setGeometry(QRect(owner->mapToGlobal(QPoint()),owner->size()));
    const qreal scale=qMax(1.0,property("fontScale").toInt()/100.0);
    const int w=qMin(width()-32,qRound(440*scale));
    const int h=qMin(height()-32,qRound(548*scale));
    m_card->setGeometry((width()-w)/2,(height()-h)/2,w,h);
}
bool AchievementCelebration::eventFilter(QObject *object,QEvent *event)
{
    if(object==parentWidget() && (event->type()==QEvent::Resize || event->type()==QEvent::Move)) fit();
    return QDialog::eventFilter(object,event);
}
void AchievementCelebration::paintEvent(QPaintEvent *)
{
    QPainter p(this);p.fillRect(rect(),QColor(25,22,18,88));
}
void AchievementCelebration::mousePressEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton && !m_card->geometry().contains(event->position().toPoint())) reject();
    else QDialog::mousePressEvent(event);
}
void AchievementCelebration::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);fit();
    if(parentWidget()->property("reducedMotion").toBool()) return;
    auto *effect=new QGraphicsOpacityEffect(m_card);m_card->setGraphicsEffect(effect);
    auto *animation=new QPropertyAnimation(effect,"opacity",effect);
    animation->setDuration(300);animation->setStartValue(0.0);animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);animation->start(QAbstractAnimation::DeleteWhenStopped);
}

AchievementNotifier::AchievementNotifier(DatabaseManager &database,QWidget *owner)
    :QObject(owner),m_database(database),m_owner(owner)
{
    setObjectName("achievementNotifier");resetBaseline();
    m_timer.setInterval(600);
    connect(&m_timer,&QTimer::timeout,this,&AchievementNotifier::checkNow);m_timer.start();
}
bool AchievementNotifier::readRevision(qint64 &changes,qint64 &version)
{
    if(!m_database.database().isOpen()) return false;
    QSqlQuery query(m_database.database());
    if(!query.exec("SELECT total_changes()") || !query.next()) return false;
    changes=query.value(0).toLongLong();query.finish();
    if(!query.exec("PRAGMA data_version") || !query.next()) return false;
    version=query.value(0).toLongLong();return true;
}
bool AchievementNotifier::resetBaseline()
{
    m_pending.clear();if(m_dialog) m_dialog->reject();
    m_known.clear();m_initialized=false;
    QList<Achievement> loaded;
    if(!AchievementRepository(m_database).load(loaded)) return false;
    for(const auto &badge:loaded) if(badge.unlocked()) m_known.insert(badge.key);
    m_initialized=readRevision(m_changes,m_version);m_day=QDate::currentDate();return m_initialized;
}
void AchievementNotifier::checkNow()
{
    if(!m_initialized) { resetBaseline();return; }
    qint64 changes,version;
    if(!readRevision(changes,version)) return;
    if(changes!=m_changes || version!=m_version || m_day!=QDate::currentDate()) {
        QList<Achievement> loaded;
        if(!AchievementRepository(m_database).load(loaded)) return; // Retry; keep the last good baseline.
        for(const auto &badge:loaded) if(badge.unlocked() && !m_known.contains(badge.key)) {
            m_known.insert(badge.key);m_pending.append(badge);
        }
        readRevision(m_changes,m_version);m_day=QDate::currentDate();
    }
    presentPending();
}
void AchievementNotifier::presentPending()
{
    if(m_pending.isEmpty() || m_dialog || !m_owner->isVisible() || m_owner->isMinimized()) return;
    if(QApplication::activeModalWidget() || QApplication::activePopupWidget()) return;
    if(auto *splash=m_owner->findChild<SplashScreen *>();splash && splash->isVisible()) return;
    if(auto *guide=m_owner->findChild<OnboardingDialog *>();guide && guide->isVisible()) return;
    m_dialog=new AchievementCelebration(m_pending,m_owner);m_pending.clear();
    connect(m_dialog,&AchievementCelebration::collectionRequested,this,&AchievementNotifier::collectionRequested);
    m_dialog->open();
}
