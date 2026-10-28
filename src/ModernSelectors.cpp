#include "ModernSelectors.h"
#include "AppTheme.h"
#include <QCalendarWidget>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QRegion>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextCharFormat>
#include <QVBoxLayout>

namespace {
class OptionPopup:public QFrame {
public:
    explicit OptionPopup(QWidget *parent):QFrame(parent,Qt::Popup) {}
    void watch(QWidget *widget) { widget->installEventFilter(this); }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()!=QEvent::KeyPress) return false;
        auto *key=static_cast<QKeyEvent *>(event);
        if(key->key()==Qt::Key_Escape) { close(); return true; }
        if(auto *button=qobject_cast<QPushButton *>(object)) if(key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter) { button->click(); return true; }
        if(key->key()!=Qt::Key_Up && key->key()!=Qt::Key_Down) return false;
        QList<QPushButton *> options; for(auto *button:findChildren<QPushButton *>()) if(button->isVisible() && button->isEnabled()) options.append(button);
        if(options.isEmpty()) return false;
        int index=options.indexOf(qobject_cast<QPushButton *>(object)); index=(index+(key->key()==Qt::Key_Down?1:-1)+options.size())%options.size(); options[index]->setFocus(); return true;
    }
};
void paintPicker(QWidget *widget,const QString &text)
{
    QPainter painter(widget); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(AppTheme::color(widget,widget->hasFocus()?"accent":"border"),1)); painter.setBrush(AppTheme::color(widget,"card")); painter.drawRoundedRect(QRectF(widget->rect()).adjusted(.5,.5,-.5,-.5),9,9);
    painter.setPen(AppTheme::color(widget,widget->isEnabled()?"secondary":"muted")); painter.setFont(widget->font()); painter.drawText(widget->rect().adjusted(12,0,-29,0),Qt::AlignVCenter|Qt::AlignLeft,painter.fontMetrics().elidedText(text,Qt::ElideRight,widget->width()-43));
    const qreal x=widget->width()-17,y=widget->height()/2.0; QPainterPath caret; caret.moveTo(x-3,y-1); caret.lineTo(x,y+2); caret.lineTo(x+3,y-1); painter.setPen(QPen(AppTheme::color(widget,"muted"),1.3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); painter.setBrush(Qt::NoBrush); painter.drawPath(caret);
}
QFrame *popupFrame(QWidget *anchor,const char *name)
{
    auto *frame=new OptionPopup(anchor); frame->setObjectName(QLatin1String(name)); frame->setProperty("billingPopup",true); frame->setAttribute(Qt::WA_DeleteOnClose); return frame;
}
}
void ModernSelectors::showPopup(QFrame *popup,QWidget *anchor)
{
    popup->ensurePolished(); popup->adjustSize(); const QRect bounds=anchor->screen()?anchor->screen()->availableGeometry():QRect(0,0,1280,800); QPoint point=anchor->mapToGlobal(QPoint(0,anchor->height()+7));
    if (point.x()+popup->width()>bounds.right()) point.setX(bounds.right()-popup->width()); if (point.y()+popup->height()>bounds.bottom()) point.setY(anchor->mapToGlobal(QPoint()).y()-popup->height()-7);
    point.setX(qMax(bounds.left(),point.x())); point.setY(qMax(bounds.top(),point.y())); popup->move(point);
    QPainterPath shape; shape.addRoundedRect(QRectF(popup->rect()),12,12); popup->setMask(QRegion(shape.toFillPolygon().toPolygon()));
    popup->show(); popup->setFocus();
}
MonthPicker::MonthPicker(const QDate &value,QWidget *parent):QDateEdit(value,parent)
{
    setDateRange(QDate(1,1,1),QDate(9999,12,31)); setDisplayFormat(QStringLiteral("yyyy 年 MM 月")); setButtonSymbols(QAbstractSpinBox::NoButtons); setReadOnly(true); lineEdit()->hide(); setCursor(Qt::PointingHandCursor); setMinimumHeight(35); connect(this,&QDateEdit::dateChanged,this,[this] { update(); });
}
QSize MonthPicker::sizeHint() const { return {145,36}; }
void MonthPicker::paintEvent(QPaintEvent *) { paintPicker(this,date().toString(QStringLiteral("yyyy 年 M 月"))); }
void MonthPicker::mousePressEvent(QMouseEvent *event) { if (event->button()==Qt::LeftButton && isEnabled()) openPicker(); else QDateEdit::mousePressEvent(event); }
void MonthPicker::keyPressEvent(QKeyEvent *event) { if (event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter || event->key()==Qt::Key_Space || event->key()==Qt::Key_Down) { openPicker(); event->accept(); } else QDateEdit::keyPressEvent(event); }
void MonthPicker::openPicker()
{
    if (!isEnabled() || findChild<QFrame *>("monthPickerPopup")) return; auto *popup=popupFrame(this,"monthPickerPopup"); popup->setFixedWidth(268); auto *layout=new QVBoxLayout(popup); layout->setContentsMargins(14,14,14,14); layout->setSpacing(12);
    auto *header=new QHBoxLayout; auto *previous=new QPushButton(QStringLiteral("‹"),popup); auto *next=new QPushButton(QStringLiteral("›"),popup); auto *year=new QSpinBox(popup); year->setObjectName("monthPickerYearEdit"); year->setRange(1,9999); year->setValue(date().year()); year->setButtonSymbols(QAbstractSpinBox::NoButtons); year->setSuffix(QStringLiteral(" 年")); year->setAlignment(Qt::AlignCenter); previous->setProperty("billingQuiet",true); next->setProperty("billingQuiet",true); previous->setFixedWidth(32); next->setFixedWidth(32); header->addWidget(previous); header->addWidget(year,1); header->addWidget(next); layout->addLayout(header);
    auto *grid=new QGridLayout; grid->setSpacing(6); for (int month=1;month<=12;++month) { auto *button=new QPushButton(QStringLiteral("%1 月").arg(month),popup); button->setObjectName(QStringLiteral("pickMonth%1").arg(month)); button->setProperty("billingChoice",true); button->setCheckable(true); button->setChecked(month==date().month()); button->setMinimumHeight(32); grid->addWidget(button,(month-1)/3,(month-1)%3); connect(button,&QPushButton::clicked,this,[this,popup,year,month] { setDate(QDate(year->value(),month,1)); popup->close(); }); } layout->addLayout(grid);
    connect(previous,&QPushButton::clicked,year,[year] { year->setValue(year->value()-1); }); connect(next,&QPushButton::clicked,year,[year] { year->setValue(year->value()+1); }); ModernSelectors::showPopup(popup,this);
}
DatePicker::DatePicker(const QDate &value,QWidget *parent):QDateEdit(value,parent)
{
    setDateRange(QDate(1,1,1),QDate(9999,12,31)); setDisplayFormat("yyyy-MM-dd"); setButtonSymbols(QAbstractSpinBox::NoButtons); setReadOnly(true); lineEdit()->hide(); setMinimumHeight(35); setCursor(Qt::PointingHandCursor); connect(this,&QDateEdit::dateChanged,this,[this] { update(); });
}
QSize DatePicker::sizeHint() const { return {140,36}; }
void DatePicker::paintEvent(QPaintEvent *) { paintPicker(this,date().toString("yyyy-MM-dd")); }
void DatePicker::mousePressEvent(QMouseEvent *event) { if (event->button()==Qt::LeftButton && isEnabled()) openPicker(); else QDateEdit::mousePressEvent(event); }
void DatePicker::keyPressEvent(QKeyEvent *event) { if (event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter || event->key()==Qt::Key_Space || event->key()==Qt::Key_Down) { openPicker(); event->accept(); } else QDateEdit::keyPressEvent(event); }
void DatePicker::openPicker()
{
    if(!isEnabled() || findChild<QFrame *>("datePickerPopup")) return;
    auto *popup=popupFrame(this,"datePickerPopup"); popup->setFixedWidth(322);
    auto *layout=new QVBoxLayout(popup); layout->setContentsMargins(12,12,12,12); layout->setSpacing(9);
    auto *header=new QHBoxLayout; auto *previous=new QPushButton(QStringLiteral("‹"),popup); auto *next=new QPushButton(QStringLiteral("›"),popup);
    auto *month=new MonthPicker(QDate(date().year(),date().month(),1),popup); month->setObjectName("dateCalendarMonthPicker");
    for(auto *button:{previous,next}) { button->setProperty("billingQuiet",true); button->setFixedWidth(32); }
    header->addWidget(previous); header->addWidget(month,1); header->addWidget(next); layout->addLayout(header);
    auto *calendar=new QCalendarWidget(popup); calendar->setObjectName("billingDateCalendar"); calendar->setDateRange(minimumDate(),maximumDate()); calendar->setSelectedDate(date()); calendar->setNavigationBarVisible(false); calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader); calendar->setFirstDayOfWeek(Qt::Monday);
    QTextCharFormat format; format.setForeground(AppTheme::color(this,"secondary")); calendar->setWeekdayTextFormat(Qt::Saturday,format); calendar->setWeekdayTextFormat(Qt::Sunday,format); layout->addWidget(calendar);
    connect(calendar,&QCalendarWidget::currentPageChanged,month,[month](int year,int value) { const QSignalBlocker blocker(month); month->setDate(QDate(year,value,1)); });
    connect(month,&QDateEdit::dateChanged,calendar,[calendar](const QDate &value) { calendar->setCurrentPage(value.year(),value.month()); });
    connect(previous,&QPushButton::clicked,calendar,&QCalendarWidget::showPreviousMonth); connect(next,&QPushButton::clicked,calendar,&QCalendarWidget::showNextMonth);
    const auto choose=[this,popup](const QDate &selected) { setDate(selected); popup->close(); };
    connect(calendar,&QCalendarWidget::clicked,this,choose); connect(calendar,&QCalendarWidget::activated,this,choose);
    auto *today=new QPushButton(QStringLiteral("回到今天"),popup); today->setObjectName("datePickerTodayButton"); today->setProperty("billingQuiet",true); layout->addWidget(today);
    connect(today,&QPushButton::clicked,this,[this,popup] { setDate(QDate::currentDate()); popup->close(); });
    ModernSelectors::showPopup(popup,this); calendar->setFocus();
}
ModernComboBox::ModernComboBox(QWidget *parent):QComboBox(parent) { setMinimumHeight(35); setCursor(Qt::PointingHandCursor); }
QSize ModernComboBox::sizeHint() const { const auto base=QComboBox::sizeHint(); return {qMax(135,base.width()),36}; }
void ModernComboBox::paintEvent(QPaintEvent *) { paintPicker(this,currentText()); }
void ModernComboBox::showPopup()
{
    if (!isEnabled() || count()==0 || m_popup) return;
    auto *popup=static_cast<OptionPopup *>(popupFrame(this,"modernComboPopup")); m_popup=popup;
    popup->setMinimumWidth(qMin(420,qMax(230,width()))); auto *layout=new QVBoxLayout(popup); layout->setContentsMargins(10,10,10,10); layout->setSpacing(8);
    QLineEdit *search=nullptr; if(count()>6) { search=new QLineEdit(popup); search->setObjectName("selectorSearchEdit"); search->setPlaceholderText(QStringLiteral("搜索选项…")); search->setClearButtonEnabled(true); layout->addWidget(search); popup->watch(search); }
    auto *scroll=new QScrollArea(popup); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setProperty("selectorScroll",true);
    auto *content=new QWidget; auto *grid=new QGridLayout(content); grid->setContentsMargins(0,0,0,0); grid->setSpacing(5); const int columns=count()>6?2:1;
    QList<QPushButton *> buttons; QPushButton *selected=nullptr;
    for(int i=0;i<count();++i) { auto *button=new QPushButton(itemText(i),content); button->setObjectName(QStringLiteral("chooseOption%1").arg(i)); button->setProperty("billingChoice",true); button->setCheckable(true); button->setChecked(i==currentIndex()); button->setMinimumHeight(30); button->setCursor(Qt::PointingHandCursor); grid->addWidget(button,i/columns,i%columns); buttons.append(button); popup->watch(button); if(i==currentIndex()) selected=button; connect(button,&QPushButton::clicked,this,[this,i] { setCurrentIndex(i); hidePopup(); emit textActivated(itemText(i)); emit activated(i); }); }
    scroll->setWidget(content); scroll->setMinimumHeight(qMin(300,((count()+columns-1)/columns)*40)); layout->addWidget(scroll);
    if(search) connect(search,&QLineEdit::textChanged,popup,[buttons](const QString &text) { for(auto *button:buttons) button->setVisible(button->text().contains(text,Qt::CaseInsensitive)); });
    ModernSelectors::showPopup(popup,this); if(search) search->setFocus(); else if(selected) selected->setFocus();
}
void ModernComboBox::hidePopup() { if (m_popup) { auto *popup=m_popup.data(); m_popup=nullptr; popup->close(); } QComboBox::hidePopup(); }
