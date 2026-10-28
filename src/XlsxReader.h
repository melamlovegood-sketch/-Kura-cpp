#pragma once
#include <QStringList>
#include <functional>
namespace XlsxReader {
bool read(const QByteArray &file,QList<QStringList> &rows,QString &error,const std::function<bool()> &cancelled={});
}
