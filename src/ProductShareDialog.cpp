#include "ProductShareDialog.h"
#include "AppTypography.h"
#include "ShellWidgets.h"
#include "Transaction.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPicture>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>

namespace {
// Scale to the viewport width while allowing the entire card to scroll vertically.
class CardPreview final:public QWidget {
public:
    explicit CardPreview(QWidget *parent=nullptr):QWidget(parent) { setMinimumWidth(180); }
    void setImage(const QImage &image) { m_image=image; fit(); update(); }
protected:
    void resizeEvent(QResizeEvent *) override { fit(); }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.setRenderHint(QPainter::SmoothPixmapTransform);
        if (!m_image.isNull()) painter.drawImage(QRectF(0,0,width(),double(width())*m_image.height()/m_image.width()),m_image);
    }
private:
    void fit() { if (!m_image.isNull()) setMinimumHeight(qCeil(double(width())*m_image.height()/m_image.width())); }
    QImage m_image;
};
QImage thumbnail(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size()>2097152) return {};
    QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer); reader.setAutoTransform(true);
    const auto size=reader.size(); if (!size.isValid() || qint64(size.width())*size.height()>25000000) return {};
    reader.setScaledSize(size.scaled(744,500,Qt::KeepAspectRatio)); return reader.read();
}
}

QImage ProductShareDialog::renderCard(const ProductContext &context,const ProductProfile &profile,const QByteArray &bytes,const ProductShareOptions &options)
{
    constexpr int width=900,margin=78,contentWidth=width-2*margin;
    const QColor ink("#252c28"),green("#315f50"),muted("#747b74");
    QPicture picture; QPainter painter(&picture); painter.setRenderHint(QPainter::Antialiasing);
    qreal y=68;
    const auto text=[&](const QString &value,int pixels,QFont::Weight weight,const QColor &color) {
        const auto font=AppTypography::sans(pixels,weight);
        QTextDocument document; document.setDocumentMargin(0); document.setDefaultFont(font);
        QTextOption wrap; wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); document.setDefaultTextOption(wrap);
        document.setPlainText(value); document.setTextWidth(contentWidth);
        QAbstractTextDocumentLayout::PaintContext paintContext; paintContext.palette.setColor(QPalette::Text,color);
        painter.save(); painter.translate(margin,y); document.documentLayout()->draw(&painter,paintContext); painter.restore();
        y+=document.size().height();
    };
    const auto section=[&](const QString &label,const QString &value) {
        if (value.trimmed().isEmpty()) return;
        y+=30; text(label,22,QFont::DemiBold,green); y+=12; text(value,27,QFont::Normal,ink);
    };
    text(QStringLiteral("KURA  /  一起想一想"),20,QFont::DemiBold,green); y+=26;
    text(QStringLiteral("帮我看看，值得买吗？"),30,QFont::Normal,muted); y+=18;
    text(context.name.isEmpty()?profile.name:context.name,42,QFont::DemiBold,ink); y+=20;
    text(context.amountCents?Transaction::formatAmount(context.amountCents):QStringLiteral("价格待补充"),48,QFont::DemiBold,green); y+=6;
    text(QStringLiteral("用户记录价格 · 非实时行情"),18,QFont::Normal,muted); y+=28;
    painter.setPen(QColor("#dedfd6")); painter.drawLine(QPointF(margin,y),QPointF(width-margin,y)); y+=12;
    const auto image=options.image?thumbnail(bytes):QImage();
    if (!image.isNull()) { y+=24; painter.drawImage(QPointF((width-image.width())/2.,y),image); y+=image.height()+8; }
    if (options.details) {
        QStringList lines; for (const auto &field:ProductRepository::fieldLabels()) {
            const auto value=profile.fields.value(field.first).toString(); if (!value.isEmpty()) lines << field.second+QStringLiteral("：")+value;
        }
        section(QStringLiteral("商品资料"),lines.join('\n'));
    }
    if (options.reason) section(QStringLiteral("想买理由"),ProductRepository::purchaseReason(profile,context));
    if (options.alternatives) section(QStringLiteral("替代品／其他办法"),profile.fields.value("alternatives").toString());
    if (options.notes) section(QStringLiteral("我的需求／备注"),profile.notes);
    y+=42; painter.setPen(QColor("#dedfd6")); painter.drawLine(QPointF(margin,y),QPointF(width-margin,y)); y+=22;
    text(QStringLiteral("听听你的意见，再由我自己决定。"),21,QFont::Normal,green); y+=10;
    text(QStringLiteral("Kura · 让每一次购买更有数"),17,QFont::Normal,muted); y+=60;
    painter.end();
    QImage card(width,qCeil(y),QImage::Format_RGB32); card.fill(QColor("#f7f6f0"));
    QPainter output(&card); output.setRenderHint(QPainter::Antialiasing); output.setPen(Qt::NoPen); output.setBrush(QColor("#ffffff"));
    output.drawRoundedRect(QRectF(28,28,width-56,y-56),28,28); output.drawPicture(0,0,picture); return card;
}

ProductShareDialog::ProductShareDialog(const ProductContext &context,const ProductProfile &profile,const QList<ProductEvidence> &images,QWidget *parent):QDialog(parent)
{
    setObjectName("productShareDialog"); setWindowTitle(QStringLiteral("分享商品卡片")); resize(570,760); setMinimumSize(360,360); setProperty("dialogFixedViewport",true);
    auto *layout=new QVBoxLayout(this); layout->setSpacing(12);
    layout->addWidget(ShellWidgets::label(QStringLiteral("选好内容后复制图片，在微信等聊天中粘贴发送。"),this,"muted"));
    auto *choices=new QGridLayout; QStringList labels{QStringLiteral("商品资料"),QStringLiteral("想买理由"),QStringLiteral("替代品"),QStringLiteral("需求／备注"),QStringLiteral("商品图片")};
    const QStringList names{"shareIncludeDetails","shareIncludeReason","shareIncludeAlternatives","shareIncludeNotes","shareIncludeImage"};
    QList<QCheckBox *> checks;
    for (int i=0;i<labels.size();++i) {
        auto *check=new QCheckBox(labels[i],this); check->setObjectName(names[i]); check->setChecked(i!=3); choices->addWidget(check,i/3,i%3); checks << check;
    }
    layout->addLayout(choices);
    auto *source=new QComboBox(this); source->setObjectName("shareImageSourceCombo");
    if (!context.image.isEmpty()) source->addItem(QStringLiteral("商品录入时的截图"),context.image);
    for (int i=0;i<images.size();++i) source->addItem(QStringLiteral("详情截图 %1").arg(i+1),images[i].image);
    if (!source->count()) { checks[4]->setChecked(false); checks[4]->setEnabled(false); }
    source->setVisible(source->count()>0); layout->addWidget(source);
    auto *preview=new CardPreview; preview->setObjectName("productSharePreview");
    auto *scroll=new QScrollArea(this); scroll->setObjectName("productSharePreviewScroll"); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(preview); layout->addWidget(scroll,1);
    m_status=ShellWidgets::label(QString(),this,"muted"); m_status->setObjectName("productShareStatus"); layout->addWidget(m_status);
    const auto refresh=[this,context,profile,checks,source,preview] {
        const ProductShareOptions options{checks[0]->isChecked(),checks[1]->isChecked(),checks[2]->isChecked(),checks[3]->isChecked(),checks[4]->isChecked()};
        source->setEnabled(options.image); m_card=renderCard(context,profile,source->currentData().toByteArray(),options); preview->setImage(m_card); m_status->clear();
    };
    for (auto *check:checks) connect(check,&QCheckBox::toggled,this,refresh);
    connect(source,&QComboBox::currentIndexChanged,this,refresh);
    auto *footer=new QHBoxLayout; auto *close=new QPushButton(QStringLiteral("关闭"),this); auto *save=new QPushButton(QStringLiteral("保存图片…"),this); auto *copy=new QPushButton(QStringLiteral("复制图片"),this);
    save->setObjectName("saveProductCardButton"); copy->setObjectName("copyProductCardButton"); copy->setProperty("primary",true);
    footer->addWidget(close); footer->addStretch(); footer->addWidget(save); footer->addWidget(copy); layout->addLayout(footer);
    connect(close,&QPushButton::clicked,this,&QDialog::reject);
    connect(copy,&QPushButton::clicked,this,[this] { QApplication::clipboard()->setImage(m_card); m_status->setText(QStringLiteral("图片已复制，去聊天窗口粘贴即可。")); });
    connect(save,&QPushButton::clicked,this,[this] {
        auto path=QFileDialog::getSaveFileName(this,QStringLiteral("保存商品卡片"),QStringLiteral("Kura-商品卡片.png"),QStringLiteral("PNG 图片 (*.png)"));
        if (path.isEmpty()) return; if (!path.endsWith(".png",Qt::CaseInsensitive)) path+=".png"; saveTo(path);
    });
    refresh(); for (auto *button:findChildren<QPushButton *>()) button->setAutoDefault(false);
}

bool ProductShareDialog::saveTo(const QString &path)
{
    QSaveFile file(path);
    if (m_card.isNull() || !file.open(QIODevice::WriteOnly) || !m_card.save(&file,"PNG") || !file.commit()) {
        m_status->setText(QStringLiteral("图片保存失败，请检查保存位置后重试。")); return false;
    }
    m_status->setText(QStringLiteral("图片已保存，可以作为图片发给朋友。")); return true;
}
