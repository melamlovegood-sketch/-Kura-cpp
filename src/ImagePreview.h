#pragma once
#include <QBuffer>
#include <QDialog>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
inline QPushButton *screenshotButton(QWidget *parent,const QByteArray &image)
{
    auto *button=new QPushButton(QStringLiteral("查看原截图"),parent); button->setObjectName("sourceImageButton");
    QObject::connect(button,&QPushButton::clicked,parent,[parent,image] {
        auto *dialog=new QDialog(parent); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(QStringLiteral("商品截图")); dialog->resize(760,600);
        auto *layout=new QVBoxLayout(dialog); auto *scroll=new QScrollArea(dialog); scroll->setWidgetResizable(true);
        auto *label=new QLabel; label->setAlignment(Qt::AlignCenter);
        QBuffer buffer; buffer.setData(image); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer);
        const auto size=reader.size();
        if (size.isValid() && qint64(size.width())*size.height()<=25000000) { reader.setScaledSize(size.scaled(1000,800,Qt::KeepAspectRatio)); label->setPixmap(QPixmap::fromImage(reader.read())); }
        if (!label->pixmap().isNull()) { /* image shown */ } else label->setText(QStringLiteral("无法读取保存的截图。"));
        scroll->setWidget(label); layout->addWidget(scroll); dialog->open();
    });
    return button;
}
