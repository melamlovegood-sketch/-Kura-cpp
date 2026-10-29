#pragma once
#include "AppTheme.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QStyle>
#include <QStyleOptionViewItem>

// Draw selection marks natively, including on runtimes without the SVG plugin.
class BatchCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent *event) override {
        QCheckBox::paintEvent(event);
        if (checkState()==Qt::Unchecked) return;
        QStyleOptionButton option; initStyleOption(&option);
        const auto box=style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,this);
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(AppTheme::color(this,"accent"),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const auto point=[&](double x,double y) { return QPointF(box.left()+box.width()*x,box.top()+box.height()*y); };
        if (checkState()==Qt::PartiallyChecked) painter.drawLine(point(.25,.5),point(.75,.5));
        else { painter.drawLine(point(.25,.5),point(.44,.7)); painter.drawLine(point(.44,.7),point(.78,.3)); }
    }
};

// Selection belongs to the currently displayed snapshot, never hidden records.
class BatchSelectionBar : public QWidget {
public:
    BatchSelectionBar(QTableWidget *table, const QString &prefix, bool itemChecks = false)
        : QWidget(table->parentWidget()), m_table(table), m_itemChecks(itemChecks) {
        setObjectName(prefix+"SelectionBar");
        auto *row=new QHBoxLayout(this); row->setContentsMargins(0,0,0,0);
        m_all=new BatchCheckBox(QStringLiteral("全选当前列表"),this); m_all->setObjectName(prefix+"SelectAllCheckBox");
        m_count=new QLabel(this); m_count->setObjectName(prefix+"SelectionCountLabel");
        auto *clear=new QPushButton(QStringLiteral("取消选择"),this); clear->setObjectName(prefix+"ClearSelectionButton");
        row->addWidget(m_all); row->addWidget(m_count); row->addStretch(); row->addWidget(clear);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        connect(m_all,&QCheckBox::clicked,this,[this] { if (m_table->selectionModel()->selectedRows().size()==m_table->rowCount()) m_table->clearSelection(); else m_table->selectAll(); refresh(); });
        connect(clear,&QPushButton::clicked,table,&QTableWidget::clearSelection);
        connect(table,&QTableWidget::itemSelectionChanged,this,[this,clear] { refresh(); clear->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty()); });
        connect(table->model(),&QAbstractItemModel::rowsInserted,this,[this] { refresh(); });
        connect(table->model(),&QAbstractItemModel::rowsRemoved,this,[this] { refresh(); });
        if (itemChecks) connect(table,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *item) {
            if (item->column()!=0 || !(item->flags() & Qt::ItemIsUserCheckable)) return;
            m_table->selectionModel()->select(m_table->model()->index(item->row(),0),QItemSelectionModel::Rows | (item->checkState()==Qt::Checked?QItemSelectionModel::Select:QItemSelectionModel::Deselect));
        });
        if (itemChecks) table->viewport()->installEventFilter(this);
        clear->setEnabled(false); refresh();
    }
    void refresh() {
        const int selected=m_table->selectionModel()->selectedRows().size(), total=m_table->rowCount();
        const QSignalBlocker allBlocker(m_all);
        m_all->setCheckState(selected==0?Qt::Unchecked:selected==total?Qt::Checked:Qt::PartiallyChecked);
        m_all->setEnabled(total>0);
        m_count->setText(QStringLiteral("已选 %1 / %2 条").arg(selected).arg(total));
        if (m_itemChecks) {
            const QSignalBlocker tableBlocker(m_table);
            for (int row=0;row<total;++row) if (auto *item=m_table->item(row,0)) {
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(item->isSelected()?Qt::Checked:Qt::Unchecked);
            }
        }
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if (object==m_table->viewport() && (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonRelease)) {
            auto *mouse=static_cast<QMouseEvent *>(event);
            const auto index=m_table->indexAt(mouse->position().toPoint());
            if (mouse->button()==Qt::LeftButton && index.isValid() && index.column()==0) {
                QStyleOptionViewItem option; option.initFrom(m_table); option.rect=m_table->visualRect(index);
                option.features=QStyleOptionViewItem::HasCheckIndicator;
                const auto indicator=m_table->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator,&option,m_table);
                if (indicator.contains(mouse->position().toPoint())) {
                    if (event->type()==QEvent::MouseButtonPress) {
                        auto *item=m_table->item(index.row(),0);
                        item->setCheckState(item->isSelected()?Qt::Unchecked:Qt::Checked);
                        m_table->setFocus();
                    }
                    return true;
                }
            }
        }
        return QWidget::eventFilter(object,event);
    }
private:
    QTableWidget *m_table;
    bool m_itemChecks;
    QCheckBox *m_all;
    QLabel *m_count;
};
