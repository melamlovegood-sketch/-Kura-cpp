#pragma once
#include <QDoubleSpinBox>
#include <QLineEdit>

// Keep the hint outside the editable value. QSpinBox specialValueText puts
// words into the editor, so a normal click followed by typing fails validation.
class EmptyAmountSpinBox final : public QDoubleSpinBox {
public:
    explicit EmptyAmountSpinBox(QWidget *parent,const QString &hint=QStringLiteral("待填写（元）"))
        :QDoubleSpinBox(parent) { lineEdit()->setPlaceholderText(hint); }
protected:
    QString textFromValue(double value) const override {
        return value==0?QString():QDoubleSpinBox::textFromValue(value);
    }
    QValidator::State validate(QString &text,int &position) const override {
        return text.trimmed().isEmpty()?QValidator::Acceptable:QDoubleSpinBox::validate(text,position);
    }
    double valueFromText(const QString &text) const override {
        return text.trimmed().isEmpty()?0:QDoubleSpinBox::valueFromText(text);
    }
};
