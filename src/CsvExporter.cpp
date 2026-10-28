#include "CsvExporter.h"
#include "CategoryCatalog.h"
#include <QSaveFile>

namespace {
QString field(QString value, bool userText = false)
{
    // Treat user text as text when opened in a spreadsheet, including formulas
    // preceded by whitespace. CSV itself cannot declare a cell's data type.
    const QString trimmed = value.trimmed();
    if (userText && !trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
        value.prepend(QChar('\''));
    value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QChar('"') + value + QChar('"');
}
}

bool CsvExporter::write(const QString &path, const QList<Transaction> &records, QString &error)
{
    error.clear();
    for (const auto &record : records) {
        if (record.id() <= 0 || !record.validationError().isEmpty()) {
            error = QStringLiteral("无法导出无效的消费记录。");
            return false;
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法创建导出文件：%1").arg(file.errorString());
        return false;
    }
    QByteArray header = QByteArray::fromHex("efbbbf"); // UTF-8 BOM for Excel Chinese text.
    header += QStringLiteral("编号,日期,时间,名称,分类,金额（元）,备注,细分类,预算归属\r\n").toUtf8();
    if (file.write(header) != header.size()) {
        error = file.errorString();
        return false;
    }
    for (const auto &record : records) {
        const QString amount = QStringLiteral("%1.%2").arg(record.amountCents() / 100)
            .arg(record.amountCents() % 100, 2, 10, QChar('0'));
        const QStringList cells = {QString::number(record.id()), field(record.date().toString(Qt::ISODate)),
            field(record.time().isValid() ? record.time().toString(QStringLiteral("HH:mm:ss")) : QString()),
            field(record.description(), true), field(record.category()), amount, field(record.note(), true), field(CategoryCatalog::label(record.detailCategory())), field(CategoryCatalog::isBasic(record.detailCategory())?QStringLiteral("基础生活"):QStringLiteral("可支配"))};
        const QByteArray row = (cells.join(QChar(',')) + QStringLiteral("\r\n")).toUtf8();
        if (file.write(row) != row.size()) {
            error = QStringLiteral("导出写入失败：%1").arg(file.errorString());
            return false;
        }
    }
    if (!file.commit()) {
        error = QStringLiteral("无法完成导出：%1").arg(file.errorString());
        return false;
    }
    return true;
}
