#pragma once
#include "ProductRepository.h"
#include <QDialog>
#include <QImage>
class QLabel;

struct ProductShareOptions {
    bool details=true,reason=true,alternatives=true,notes=false,image=true;
};

class ProductShareDialog:public QDialog {
    Q_OBJECT
public:
    ProductShareDialog(const ProductContext &context,const ProductProfile &profile,const QList<ProductEvidence> &images,QWidget *parent=nullptr);
    static QImage renderCard(const ProductContext &context,const ProductProfile &profile,const QByteArray &image,const ProductShareOptions &options={});
    const QImage &cardImage() const { return m_card; }
    bool saveTo(const QString &path);
private:
    QImage m_card;
    QLabel *m_status;
};
