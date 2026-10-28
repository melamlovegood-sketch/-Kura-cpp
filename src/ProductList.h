#pragma once
#include "AppTheme.h"

#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <functional>

class ProductStateBadge:public QWidget {
public:
    explicit ProductStateBadge(QWidget *parent):QWidget(parent) { setObjectName("productStateBadge"); setFixedSize(36,36); setAttribute(Qt::WA_TransparentForMouseEvents); hide(); }
    void setState(bool cooling,bool closed) { m_cooling=cooling; m_closed=closed; setProperty("cooling",cooling); setAccessibleName(cooling?QStringLiteral("冷静中，暂不可购买"):closed?QStringLiteral("历史商品"):QStringLiteral("待决定")); show(); update(); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        auto background=AppTheme::color(this,"muted"); background.setAlpha(m_cooling?36:14);
        p.setPen(Qt::NoPen); p.setBrush(background); p.drawRoundedRect(rect(),10,10);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(AppTheme::color(this,m_cooling || m_closed?"muted":"ink"),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if (m_cooling) { p.drawEllipse(QRectF(10,10,16,16)); p.drawLine(QPointF(18,13),QPointF(18,18)); p.drawLine(QPointF(18,18),QPointF(22,20)); }
        else { p.drawRoundedRect(QRectF(10,14,16,13),2,2); p.drawArc(QRectF(14,9,8,10),0,180*16); }
    }
private:
    bool m_cooling=false,m_closed=false;
};

class ProductNameLabel:public QLabel {
public:
    using QLabel::QLabel;
    std::function<void()> activated;
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button()==Qt::LeftButton && activated) event->accept(); else QLabel::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button()==Qt::LeftButton && activated) { activated(); event->accept(); } else QLabel::mouseReleaseEvent(event);
    }
};

class ProductToggle : public QPushButton
{
public:
    explicit ProductToggle(QWidget *parent) : QPushButton(parent)
    {
        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }
    QSize sizeHint() const override
    {
        return layout() ? layout()->sizeHint().expandedTo(QSize(0, 88)) : QSize(0, 88);
    }
    QSize minimumSizeHint() const override { return QSize(0, 88); }
    int heightForWidth(int width) const override
    {
        return layout() ? qMax(88, layout()->totalHeightForWidth(width)) : 88;
    }
};

class ProductArrow : public QLabel
{
public:
    explicit ProductArrow(QWidget *parent) : QLabel(parent) { setFixedSize(12, 18); }
    void setExpanded(bool expanded) { m_expanded = expanded; update(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(palette().color(QPalette::WindowText), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPolygonF arrow;
        if (m_expanded) arrow << QPointF(3, 7) << QPointF(6, 10) << QPointF(9, 7);
        else arrow << QPointF(4, 5) << QPointF(7, 9) << QPointF(4, 13);
        painter.drawPolyline(arrow);
    }
private:
    bool m_expanded = false;
};

// The table retains the repository snapshot and selection model. Each visible
// row is a single disclosure, so secondary fields never become table columns.
class ProductList : public QTableWidget
{
public:
    void setProductDetailsHandler(std::function<void(int)> handler) { m_productDetails=std::move(handler); }
    void setRowActionsHandler(std::function<void(int,QHBoxLayout *)> handler) { m_rowActions=std::move(handler); }
    explicit ProductList(int columns, QWidget *parent = nullptr)
        : QTableWidget(0, columns, parent)
    {
        setProperty("productList", true);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setShowGrid(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        horizontalHeader()->hide();
        verticalHeader()->hide();
        horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int column = 1; column < columns; ++column) hideColumn(column);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        connect(this, &QTableWidget::itemSelectionChanged, this, [this] { updateExpansion(); });
    }

    void setDetailWidget(QWidget *details)
    {
        m_details = details;
        releaseDetails();
    }

    void releaseDetails()
    {
        if (m_details) {
            m_details->hide();
            m_details->setParent(this);
        }
    }

    void resetRows(int count)
    {
        releaseDetails();
        const QSignalBlocker blocker(this);
        setRowCount(0);
        setRowCount(count);
        for (int row = 0; row < count; ++row) {
            auto *body = new QWidget;
            body->setProperty("productRow", true);
            body->setProperty("firstProduct", row == 0);
            body->setProperty("lastProduct", row == count - 1);
            auto *column = new QVBoxLayout(body);
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(0);
            auto *toggle = new ProductToggle(body);
            toggle->setObjectName(QStringLiteral("productToggleButton"));
            toggle->setProperty("productToggle", true);
            toggle->setCursor(Qt::PointingHandCursor);
            toggle->setMinimumHeight(88);
            auto *header = new QHBoxLayout(toggle);
            header->setContentsMargins(24, 16, 24, 16);
            header->setSpacing(18);
            header->addWidget(new ProductStateBadge(toggle));
            auto *identity = new QVBoxLayout;
            identity->setSpacing(6);
            auto *name = new ProductNameLabel(toggle);
            name->setObjectName(QStringLiteral("productNameLabel"));
            name->setProperty("productName", true);
            name->setWordWrap(true);
            name->setTextFormat(Qt::PlainText);
            name->setMinimumWidth(0);
            identity->addWidget(name);
            auto *subtitle = new QLabel(toggle);
            subtitle->setObjectName(QStringLiteral("productStatusLabel"));
            subtitle->setProperty("muted", true);
            subtitle->setWordWrap(true);
            subtitle->setTextFormat(Qt::PlainText);
            identity->addWidget(subtitle);
            header->addLayout(identity, 1);
            auto *price = new QVBoxLayout;
            price->setSpacing(4);
            auto *amount = new QLabel(toggle);
            amount->setObjectName(QStringLiteral("productAmountLabel"));
            amount->setProperty("productAmount", true);
            amount->setAlignment(Qt::AlignRight);
            price->addWidget(amount);
            auto *note = new QLabel(toggle);
            note->setObjectName(QStringLiteral("productPriceNoteLabel"));
            note->setProperty("muted", true);
            note->setAlignment(Qt::AlignRight);
            price->addWidget(note);
            header->addLayout(price);
            if (m_rowActions) m_rowActions(row,header);
            auto *arrow = new ProductArrow(toggle);
            arrow->setObjectName(QStringLiteral("productArrowLabel"));
            arrow->setProperty("productArrow", true);
            arrow->setFixedWidth(12);
            arrow->setAlignment(Qt::AlignCenter);
            header->addWidget(arrow);
            for (auto *label : toggle->findChildren<QLabel *>())
                label->setAttribute(Qt::WA_TransparentForMouseEvents);
            if (m_productDetails) {
                name->setAttribute(Qt::WA_TransparentForMouseEvents,false);
                name->setCursor(Qt::PointingHandCursor);
                name->setToolTip(QStringLiteral("打开商品详情与 AI 讨论"));
                name->activated=[this,row] { m_productDetails(row); };
            }
            column->addWidget(toggle);
            setCellWidget(row, 0, body);
            connect(toggle, &QPushButton::clicked, this, [this, row] {
                if (currentRow() == row && item(row, 0) && item(row, 0)->isSelected()) {
                    clearSelection();
                    setCurrentCell(-1, -1);
                } else {
                    selectRow(row);
                }
            });
        }
    }

    void setSummary(int row, const QString &name, const QString &status,
                    const QString &amount, const QString &note)
    {
        auto *body = cellWidget(row, 0);
        if (!body) return;
        const QList<QPair<const char *, QString>> values = {
            {"productNameLabel", name}, {"productStatusLabel", status},
            {"productAmountLabel", amount}, {"productPriceNoteLabel", note}};
        for (const auto &value : values) {
            auto *label = body->findChild<QLabel *>(QLatin1String(value.first));
            if (label->text() != value.second) label->setText(value.second);
        }
        body->findChild<QPushButton *>()->setAccessibleName(name + QStringLiteral("，") + status
            + QStringLiteral("，") + amount + QStringLiteral("，展开或收起详情"));
    }

    void setCostHint(int row,const QString &text,const QString &basis) {
        auto *body=cellWidget(row,0); if (!body) return;
        auto *label=body->findChild<QLabel *>("productCostHintLabel");
        if (!label) {
            auto *note=body->findChild<QLabel *>("productPriceNoteLabel");
            label=new QLabel(note->parentWidget()); label->setObjectName("productCostHintLabel"); label->setProperty("muted",true);
            label->setTextFormat(Qt::PlainText); label->setWordWrap(true); label->setMaximumWidth(170); label->setAlignment(Qt::AlignRight);
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            // The price note belongs to the right-hand vertical layout.
            auto *header=qobject_cast<QHBoxLayout *>(note->parentWidget()->layout());
            for (int i=0;header && i<header->count();++i) if (auto *column=header->itemAt(i)->layout()) if (column->indexOf(note)>=0) { column->addWidget(label); break; }
        }
        label->setText(text); label->ensurePolished();
        label->setMinimumWidth(text.isEmpty()?0:qMin(170,label->fontMetrics().horizontalAdvance(text)));
        label->setToolTip(basis); label->setVisible(!text.isEmpty());
    }

    void setCoolingState(int row,bool cooling,bool closed=false) {
        if (auto *body=cellWidget(row,0)) static_cast<ProductStateBadge *>(body->findChild<QWidget *>("productStateBadge"))->setState(cooling,closed);
    }

    void updateExpansion()
    {
        const int selected = currentRow();
        const bool open = selected >= 0 && item(selected, 0) && item(selected, 0)->isSelected();
        auto *body = open ? cellWidget(selected, 0) : nullptr;
        if (m_details && m_details->parentWidget() != body) {
            releaseDetails();
            if (body) {
                body->layout()->addWidget(m_details);
                m_details->show();
            }
        }
        for (int row = 0; row < rowCount(); ++row) {
            if (auto *widget = cellWidget(row, 0))
                static_cast<ProductArrow *>(widget->findChild<QLabel *>(QStringLiteral("productArrowLabel")))
                    ->setExpanded(open && row == selected);
        }
        fitRows();
        // Labels and wrapped actions may need a layout pass after reparenting.
        QTimer::singleShot(0, this, [this] { fitRows(); });
    }

    void fitRows()
    {
        if (m_fitting) return;
        m_fitting = true;
        int total = frameWidth() * 2 + 2;
        const int width = qMax(1, viewport()->width());
        for (int row = 0; row < rowCount(); ++row) {
            auto *body = cellWidget(row, 0);
            if (!body) continue;
            auto *layout = body->layout();
            const int wanted = layout->hasHeightForWidth() ? layout->heightForWidth(width) : layout->sizeHint().height();
            const int height = qMax(101, wanted + 1);
            if (rowHeight(row) != height) setRowHeight(row, height);
            total += height;
        }
        if (height() != qMax(1, total)) {
            setFixedHeight(qMax(1, total));
            // QTabWidget caches its page hints. Invalidate the surrounding
            // layouts as well, or a newly expanded chart gets clipped.
            for (auto *page = parentWidget(); page && !qobject_cast<QAbstractScrollArea *>(page); page = page->parentWidget()) {
                if (page->layout()) page->layout()->invalidate();
                page->updateGeometry();
            }
        }
        m_fitting = false;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTableWidget::resizeEvent(event);
        fitRows();
    }

private:
    std::function<void(int)> m_productDetails;
    std::function<void(int,QHBoxLayout *)> m_rowActions;
    QWidget *m_details = nullptr;
    bool m_fitting = false;
};
