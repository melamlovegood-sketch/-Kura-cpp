#include "DialogUi.h"
#include "AppTheme.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
class Header final:public QWidget {
public:
    explicit Header(QDialog *dialog,QWidget *parent):QWidget(parent),m_dialog(dialog) {
        setObjectName("appDialogHeader"); auto *row=new QHBoxLayout(this); row->setContentsMargins(0,0,0,0); row->setSpacing(16);
        auto *title=new QLabel(dialog->windowTitle(),this); title->setObjectName("appDialogTitle"); title->setWordWrap(true);
        row->addWidget(title,1); auto *close=new QToolButton(this); close->setObjectName("appDialogCloseButton"); close->setText(QStringLiteral("×")); close->setAccessibleName(QStringLiteral("关闭弹窗")); close->setToolTip(QStringLiteral("关闭 · Esc")); close->setFixedSize(32,32); close->setCursor(Qt::PointingHandCursor); row->addWidget(close,0,Qt::AlignTop);
        close->setFocusPolicy(Qt::NoFocus);
        connect(close,&QToolButton::clicked,dialog,&QDialog::reject);
        connect(dialog,&QWidget::windowTitleChanged,title,&QLabel::setText);
    }
protected:
    void mousePressEvent(QMouseEvent *event) override { if(event->button()==Qt::LeftButton) { m_offset=event->globalPosition().toPoint()-m_dialog->pos(); m_dragging=true; } }
    void mouseMoveEvent(QMouseEvent *event) override { if(m_dragging && event->buttons().testFlag(Qt::LeftButton)) m_dialog->move(event->globalPosition().toPoint()-m_offset); }
    void mouseReleaseEvent(QMouseEvent *) override { m_dragging=false; }
private:
    QDialog *m_dialog; QPoint m_offset; bool m_dragging=false;
};
class Shade final:public QWidget {
public:
    explicit Shade(QWidget *parent):QWidget(parent) { setObjectName("appDialogShade"); setAttribute(Qt::WA_StyledBackground,false); setFocusPolicy(Qt::NoFocus); }
protected:
    void paintEvent(QPaintEvent *) override { QPainter painter(this); painter.fillRect(rect(),QColor(25,22,18,72)); }
};
bool actionRow(QLayoutItem *item)
{
    if (auto *widget=item->widget()) return qobject_cast<QDialogButtonBox *>(widget) || qobject_cast<QPushButton *>(widget);
    if (auto *layout=item->layout()) { bool hasButton=false; for(int i=0;i<layout->count();++i) { auto *child=layout->itemAt(i); if(child->spacerItem()) continue; if(!actionRow(child)) return false; hasButton=true; } return hasButton; }
    return false;
}
class Binding final:public QObject {
public:
    Binding(QDialog *dialog,QScrollArea *scroll,QWidget *body,QWidget *background=nullptr):QObject(dialog),m_dialog(dialog),m_background(background),m_scroll(scroll),m_body(body),m_preferredWidth(dialog->width()),m_preferredHeight(dialog->height()) {
        if (auto *parent=dialog->parentWidget()) { m_owner=parent->window(); m_shade=new Shade(m_owner); m_shade->hide(); m_owner->installEventFilter(this); }
        dialog->installEventFilter(this);
    }
    ~Binding() override { delete m_shade; }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_dialog && event->type()==QEvent::Resize) {
            if(m_background) m_background->setGeometry(m_dialog->rect());
            roundWindow();
        }
        if (object==m_owner && event->type()==QEvent::Resize && m_shade) m_shade->setGeometry(m_owner->rect());
        if (object==m_dialog && event->type()==QEvent::Show) {
            if(m_shade) { m_shade->setGeometry(m_owner->rect()); m_shade->show(); m_shade->raise(); }
            fit();
            roundWindow();
        }
        if(object==m_dialog && event->type()==QEvent::Hide && m_shade) m_shade->hide();
        return false;
    }
    void fit() {
        if(m_dialog->property("dialogPreserveGeometry").toBool()) return;
        auto *screen=m_dialog->screen(); const QRect available=screen?screen->availableGeometry():QRect(0,0,1280,800);
        QRect area=m_owner?QRect(m_owner->mapToGlobal(QPoint()),m_owner->size()).intersected(available):available;
        if(area.width()<320 || area.height()<280) area=available;
        const int maxWidth=qMax(280,area.width()-40),maxHeight=qMax(240,area.height()-40);
        m_dialog->setMinimumSize(0,0); m_dialog->setMaximumSize(maxWidth,maxHeight);
        if(m_body && !m_scroll) {
            m_dialog->resize(qMin(maxWidth,m_preferredWidth),qMin(maxHeight,m_preferredHeight));
        } else if(m_scroll && m_body) {
            m_body->layout()->activate();
            const int desired=qBound(440,qMax(m_preferredWidth,m_body->sizeHint().width()+80),1100);
            const int width=qMin(maxWidth,desired);
            const int contentWidth=qMax(180,width-90);
            for(auto *row:m_body->findChildren<QBoxLayout *>()) {
                if(row->direction()==QBoxLayout::LeftToRight && row->minimumSize().width()>contentWidth) {
                    row->setProperty("dialogWrapped",true); row->setDirection(QBoxLayout::TopToBottom);
                }
            }
            m_body->layout()->setSizeConstraint(QLayout::SetNoConstraint);
            m_body->setMinimumWidth(0);
            m_scroll->setMinimumHeight(0);
            m_scroll->setMaximumHeight(qMax(100,maxHeight-150));
            m_dialog->resize(width,qMin(maxHeight,m_body->sizeHint().height()+155));
        } else m_dialog->adjustSize();
        const QPoint center=area.center()-QPoint(m_dialog->width()/2,m_dialog->height()/2);
        m_dialog->move(center);
    }
    void roundWindow() {
        QPainterPath shape; shape.addRoundedRect(QRectF(m_dialog->rect()),22,22);
        m_dialog->setMask(QRegion(shape.toFillPolygon().toPolygon()));
    }
private:
    QDialog *m_dialog; QPointer<QWidget> m_owner,m_background; QPointer<Shade> m_shade;
    QScrollArea *m_scroll; QWidget *m_body; int m_preferredWidth,m_preferredHeight;
};
void styleControls(QDialog *dialog)
{
    for(auto *spin:dialog->findChildren<QAbstractSpinBox *>()) spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    for(auto *form:dialog->findChildren<QFormLayout *>()) {
        form->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter); form->setHorizontalSpacing(18); form->setVerticalSpacing(14);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    }
    for(auto *button:dialog->findChildren<QPushButton *>()) {
        button->setCursor(Qt::PointingHandCursor);
        if(button->objectName().startsWith("save") || button->objectName().startsWith("commit") || button->objectName()=="confirmPurchaseButton") button->setProperty("primary",true);
    }
}
void prepare(QDialog *dialog)
{
    if(!dialog->layout() || dialog->property("kuraDialog").toBool() || qobject_cast<QFileDialog *>(dialog)) return;
    bool themed=false;
    for(auto *widget=static_cast<QWidget *>(dialog);widget;widget=widget->parentWidget())
        if(widget->property("theme").isValid()) { themed=true; break; }
    if(!themed) return;
    dialog->setProperty("kuraDialog",true);
    // A native rounded region avoids opaque black margins on Windows when a
    // dialog is polished after its platform window has already been created.
    dialog->setAttribute(Qt::WA_TranslucentBackground,false);
    dialog->setWindowFlag(Qt::FramelessWindowHint,true);
    if(dialog->objectName()=="billingTransactionDetailsDialog") {
        dialog->setProperty("dialogPreserveGeometry",true);
        new Binding(dialog,nullptr,nullptr); return;
    }
    styleControls(dialog);
    if(auto *message=qobject_cast<QMessageBox *>(dialog)) {
        // Keep QMessageBox's own grid and result semantics intact.
        auto *grid=qobject_cast<QGridLayout *>(message->layout()); if(!grid) return;
        struct Cell { QLayoutItem *item; int row,column,rowSpan,columnSpan; };
        QList<Cell> cells; while(grid->count()) { int r,c,rs,cs; grid->getItemPosition(0,&r,&c,&rs,&cs); cells.append({grid->takeAt(0),r,c,rs,cs}); }
        auto *header=new Header(dialog,dialog); grid->addWidget(header,0,0,1,3);
        for(const auto &cell:cells) grid->addItem(cell.item,cell.row+1,cell.column,cell.rowSpan,cell.columnSpan);
        grid->setContentsMargins(26,22,26,24); grid->setVerticalSpacing(18);
        if(auto *icon=message->findChild<QLabel *>("qt_msgboxex_icon_label")) icon->hide();
        if(auto *text=message->findChild<QLabel *>("qt_msgbox_label")) { text->setWordWrap(true); text->setTextInteractionFlags(Qt::TextSelectableByMouse); text->setMinimumWidth(300); text->setMaximumWidth(540); }
        for(auto *button:message->buttons()) {
            const auto role=message->buttonRole(button);
            if(role==QMessageBox::YesRole || role==QMessageBox::AcceptRole) button->setProperty("primary",true);
            if(role==QMessageBox::NoRole || role==QMessageBox::RejectRole) button->setText(QStringLiteral("取消"));
            else if(message->standardButton(button)==QMessageBox::Yes) {
                const auto title=message->windowTitle(); button->setText(title.contains(QStringLiteral("删除"))?QStringLiteral("确认删除"):title.contains(QStringLiteral("恢复"))?QStringLiteral("确认恢复"):title.contains(QStringLiteral("停用"))?QStringLiteral("停用"):title.contains(QStringLiteral("追踪"))?QStringLiteral("结束追踪"):QStringLiteral("确认"));
            } else if(message->standardButton(button)==QMessageBox::Ok) button->setText(QStringLiteral("知道了"));
            if(message->windowTitle().contains(QStringLiteral("删除")) && role==QMessageBox::YesRole) button->setProperty("destructive",true);
            button->style()->unpolish(button); button->style()->polish(button);
        }
        auto *background=new QFrame(message); background->setObjectName("appMessageCard"); background->setGeometry(message->rect()); background->lower(); background->show();
        new Binding(dialog,nullptr,nullptr,background); return;
    }
    auto *original=dialog->layout(); original->setContentsMargins(0,0,0,0);
    auto *body=new QWidget; body->setObjectName("appDialogBody"); body->setLayout(original);
    auto *outer=new QVBoxLayout(dialog); outer->setContentsMargins(0,0,0,0);
    auto *card=new QFrame(dialog); card->setObjectName("appDialogCard"); auto *column=new QVBoxLayout(card); column->setContentsMargins(26,24,26,24); column->setSpacing(20);
    auto *header=new Header(dialog,card); column->addWidget(header);
    QScrollArea *scroll=nullptr;
    if(dialog->property("dialogFixedViewport").toBool()) {
        body->setMinimumSize(0,0); body->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Ignored);
        original->setSizeConstraint(QLayout::SetNoConstraint); column->addWidget(body,1);
    }
    else {
        scroll=new QScrollArea(card); scroll->setObjectName("appDialogScroll"); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setWidget(body);
        column->addWidget(scroll,1);
    }
    // Lift trailing actions out of the scroll area. Validation and form state stay in place.
    auto *footer=new QVBoxLayout; footer->setContentsMargins(0,2,0,0); footer->setSpacing(10);
    QList<QLayoutItem *> actions;
    while(original->count() && actionRow(original->itemAt(original->count()-1))) actions.prepend(original->takeAt(original->count()-1));
    for(auto *item:actions) {
        if(auto *widget=item->widget()) { footer->addWidget(widget); delete item; }
        else if(auto *row=item->layout()) { row->setParent(nullptr); footer->addLayout(row); }
    }
    if(footer->count()) column->addLayout(footer); else delete footer;
    outer->addWidget(card);
    // Nested content cards can retain their own theme; the outer form stays neutral.
    for(auto *label:body->findChildren<QLabel *>(QString(),Qt::FindDirectChildrenOnly)) if(label->property("heading").toBool() && label->text()==dialog->windowTitle()) label->hide();
    auto *binding=new Binding(dialog,scroll,body); binding->fit();
}
class Filter final:public QObject {
public:
    explicit Filter(QObject *parent):QObject(parent) {}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::Polish) {
            if(auto *dialog=qobject_cast<QDialog *>(object)) prepare(dialog);
            if(auto *widget=qobject_cast<QWidget *>(object))
                if((qobject_cast<QComboBox *>(widget) || qobject_cast<QAbstractSpinBox *>(widget)) && widget->focusPolicy()==Qt::WheelFocus)
                    widget->setFocusPolicy(Qt::StrongFocus);
        }
        if(event->type()==QEvent::Wheel) {
            auto *field=qobject_cast<QWidget *>(object);
            // Include embedded editors, but never climb out of a popup into
            // the combo box that owns it: its option list must scroll normally.
            while(field && !qobject_cast<QComboBox *>(field) && !qobject_cast<QAbstractSpinBox *>(field)) {
                if(field->isWindow()) return false;
                field=field->parentWidget();
            }
            if(!field) return false;
            auto *wheel=static_cast<QWheelEvent *>(event);
            if(!field->isWindow()) for(auto *parent=field->parentWidget();parent;parent=parent->parentWidget()) {
                if(auto *scroll=qobject_cast<QAbstractScrollArea *>(parent)) {
                    auto *viewport=scroll->viewport();
                    QWheelEvent forwarded(viewport->mapFromGlobal(wheel->globalPosition()),wheel->globalPosition(),
                        wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),wheel->inverted(),wheel->source(),wheel->pointingDevice());
                    QApplication::sendEvent(viewport,&forwarded);
                    break;
                }
                if(parent->isWindow()) break;
            }
            // Consume the original even with no scroll area or at its edge.
            // Focus is deliberately irrelevant: a just-edited value is safe too.
            wheel->accept();
            return true;
        }
        return false;
    }
};
}
void DialogUi::install() { static QPointer<Filter> filter; if(!filter && qApp) { filter=new Filter(qApp); qApp->installEventFilter(filter); } }
QString DialogUi::styleSheet()
{
    return QStringLiteral(R"(
        QDialog[kuraDialog="true"] { background: @card; border: none; }
        QFrame#appMessageCard, QFrame#appDialogCard { background: @card; border: 1px solid @border; border-radius: 22px; }
        QWidget#appDialogBody, QWidget#appDialogHeader, QScrollArea#appDialogScroll, QScrollArea#appDialogScroll > QWidget > QWidget { background: transparent; }
        QLabel#appDialogTitle { font-size: 22px; font-weight: 500; color: @ink; }
        QToolButton#appDialogCloseButton { background: transparent; border: none; border-radius: 8px; padding: 0; font-size: 24px; color: @muted; }
        QToolButton#appDialogCloseButton:hover, QToolButton#appDialogCloseButton:focus { background: @inset; color: @ink; }
        QDialog[kuraDialog="true"] QLineEdit, QDialog[kuraDialog="true"] QAbstractSpinBox, QDialog[kuraDialog="true"] QComboBox { min-height: 28px; border-radius: 10px; padding: 8px 12px; }
        QDialog[kuraDialog="true"] QAbstractSpinBox::up-button, QDialog[kuraDialog="true"] QAbstractSpinBox::down-button { width: 0; height: 0; }
        QDialog[kuraDialog="true"] QPushButton { min-height: 25px; padding: 8px 17px; border-radius: 10px; }
        QDialogButtonBox { button-layout: 1; }
        QDialog[kuraDialog="true"] QPushButton[primary="true"] { background: @accent; color: @onAccent; border-color: @accent; }
        QDialog[kuraDialog="true"] QPushButton[primary="true"]:disabled { background: @inset; color: @muted; border-color: @border; }
        QPushButton[decisionChoice="true"]:checked { background: @inset; border-color: @accent; color: @ink; }
        QPushButton[decisionQuiet="true"] { background: transparent; border-color: transparent; color: @secondary; }
        QPushButton[decisionQuiet="true"]:hover { background: @inset; color: @ink; }
        QDialog[kuraDialog="true"] QPushButton[destructive="true"] { background: @danger; border-color: @danger; color: #ffffff; }
        QScrollArea[selectorScroll="true"], QScrollArea[selectorScroll="true"] > QWidget > QWidget { background: transparent; }
        QDialog[kuraDialog="true"] QLineEdit[fieldError="true"], QDialog[kuraDialog="true"] QAbstractSpinBox[fieldError="true"] { border-color: @danger; }
        QDialog[kuraDialog="true"] QScrollArea { background: transparent; }
        QLabel[dialogCaption="true"] { color: @muted; font-size: 12px; }
        QPushButton[coolingPreset="true"] { background: @inset; border-color: transparent; color: @secondary; padding: 5px 12px; }
        QPushButton[coolingPreset="true"]:checked { background: @accent; color: @onAccent; }
        QStackedWidget#billImportPages { background: transparent; }
        QFrame#importDropCard { background: @card; border: 1px dashed @border; border-radius: 20px; }
        QFrame#importOptionsPanel, QFrame#importBulkBar { background: @inset; border: none; border-radius: 14px; }
        QFrame#importFooter { background: transparent; border: none; border-top: 1px solid @border; }
        QLabel[importStep="true"] { color: @muted; padding: 6px 9px; font-size: 12px; border-radius: 9px; }
        QLabel[importStep="true"][activeStep="true"] { background: @inset; color: @ink; }
        QLabel[importTitle="true"] { font-size: 20px; font-weight: 500; }
        QLabel#importUploadMark, QLabel#importSuccessMark { color: @accent; font-size: 36px; padding: 4px; }
        QLabel#importFileNameLabel, QLabel#importFooterSummary { font-weight: 500; }
        QLabel#importFooterHint { font-size: 12px; }
        QPushButton[importQuiet="true"] { background: transparent; border: 1px solid transparent; color: @secondary; padding: 5px 9px; min-height: 22px; border-radius: 10px; }
        QPushButton[importQuiet="true"]:hover, QPushButton[importQuiet="true"]:focus { background: @inset; color: @ink; }
        QPushButton[importTab="true"]:checked { background: @inset; color: @ink; }
        QTableWidget#billImportPreviewTable { background: @card; border: 1px solid @border; border-radius: 14px; }
        QTableWidget#billImportPreviewTable::item { border: none; border-bottom: 1px solid @border; padding: 8px; }
        QDialog#billImportDialog QCheckBox::indicator { width: 17px; height: 17px; border: 1px solid @border; border-radius: 5px; background: @card; }
        QDialog#billImportDialog QCheckBox::indicator:checked, QDialog#billImportDialog QCheckBox::indicator:indeterminate { background: @accent; border-color: @accent; }
        QDialog#billImportDialog QPushButton[importQuiet="true"] { min-height: 20px; padding: 4px 8px; border-radius: 10px; }
        QDialog#billImportDialog QLineEdit#importSearchEdit { min-height: 20px; padding: 5px 10px; }
    )");
}
