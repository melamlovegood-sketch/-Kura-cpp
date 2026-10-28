#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QMap>
#include <QXmlStreamWriter>
#include <QtEndian>
#include <zlib.h>
namespace BillTestFixtures {
inline QString csvField(QString text) { text.replace('"',"\"\""); return '"'+text+'"'; }
inline QByteArray csv(const QList<QStringList> &rows) { QByteArray bytes; for (const auto &row:rows) { QStringList fields; for (const auto &value:row) fields << csvField(value); bytes+=(fields.join(',')+"\r\n").toUtf8(); } return bytes; }
inline bool write(const QString &path,const QByteArray &bytes) { QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(); }
inline QList<QStringList> wechat(const QString &id="WX-00000000000000000001",const QString &name=QStringLiteral("地铁票"),const QString &amount="6.50") { return {{QStringLiteral("微信支付账单明细")},{QStringLiteral("交易时间"),QStringLiteral("交易类型"),QStringLiteral("交易对方"),QStringLiteral("商品"),QStringLiteral("收/支"),QStringLiteral("金额(元)"),QStringLiteral("当前状态"),QStringLiteral("交易单号"),QStringLiteral("备注")},{"2026-10-03 12:30:45",QStringLiteral("商户消费"),QStringLiteral("地铁公司"),name,QStringLiteral("支出"),amount,QStringLiteral("支付成功"),id,QStringLiteral("中文备注")}}; }
inline void u16(QByteArray &out,quint16 value) { char bytes[2]; qToLittleEndian(value,reinterpret_cast<uchar *>(bytes)); out.append(bytes,2); }
inline void u32(QByteArray &out,quint32 value) { char bytes[4]; qToLittleEndian(value,reinterpret_cast<uchar *>(bytes)); out.append(bytes,4); }
inline QByteArray zip(const QMap<QString,QByteArray> &parts,bool compressed=true) {
    QByteArray data,central;
    for (auto it=parts.cbegin();it!=parts.cend();++it) { const auto name=it.key().toUtf8(),plain=it.value(); QByteArray packed=plain;
        if (compressed) { packed.resize(compressBound(plain.size())+32); z_stream stream{}; stream.next_in=reinterpret_cast<Bytef *>(const_cast<char *>(plain.constData())); stream.avail_in=plain.size(); stream.next_out=reinterpret_cast<Bytef *>(packed.data()); stream.avail_out=packed.size(); if (deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)!=Z_OK) return {}; if (deflate(&stream,Z_FINISH)!=Z_STREAM_END) { deflateEnd(&stream); return {}; } packed.resize(stream.total_out); deflateEnd(&stream); }
        const auto crc=quint32(crc32(0,reinterpret_cast<const Bytef *>(plain.constData()),plain.size())),offset=quint32(data.size());
        u32(data,0x04034b50); u16(data,20); u16(data,0x800); u16(data,compressed?8:0); u16(data,0); u16(data,0); u32(data,crc); u32(data,packed.size()); u32(data,plain.size()); u16(data,name.size()); u16(data,0); data+=name; data+=packed;
        u32(central,0x02014b50); u16(central,20); u16(central,20); u16(central,0x800); u16(central,compressed?8:0); u16(central,0); u16(central,0); u32(central,crc); u32(central,packed.size()); u32(central,plain.size()); u16(central,name.size()); u16(central,0); u16(central,0); u16(central,0); u16(central,0); u32(central,0); u32(central,offset); central+=name;
    }
    const auto start=data.size(); data+=central; u32(data,0x06054b50); u16(data,0); u16(data,0); u16(data,parts.size()); u16(data,parts.size()); u32(data,central.size()); u32(data,start); u16(data,0); return data;
}
inline QByteArray xlsx(const QList<QStringList> &rows,bool compressed=true,bool shared=true,bool date1904=false,const QDateTime &numericDate={}) {
    QMap<QString,QByteArray> parts;
    parts["[Content_Types].xml"]="<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/></Types>";
    parts["_rels/.rels"]="<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>";
    parts["xl/workbook.xml"]="<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><workbookPr date1904=\""+QByteArray(date1904?"1":"0")+"\"/><sheets><sheet name=\"Bill\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
    parts["xl/_rels/workbook.xml.rels"]="<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Target=\"worksheets/sheet1.xml\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\"/></Relationships>";
    parts["xl/styles.xml"]="<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><cellXfs><xf numFmtId=\"0\"/><xf numFmtId=\"22\"/></cellXfs></styleSheet>";
    QByteArray sheet,strings; QXmlStreamWriter xml(&sheet),sst(&strings); xml.writeStartDocument(); xml.writeStartElement("worksheet"); xml.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main"); xml.writeStartElement("sheetData"); sst.writeStartDocument(); sst.writeStartElement("sst"); sst.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main"); int index=0;
    for (int row=0;row<rows.size();++row) { xml.writeStartElement("row"); xml.writeAttribute("r",QString::number(row+15)); for (int col=0;col<rows[row].size();++col) { int n=col+1; QString letters; while (n>0) { letters.prepend(QChar('A'+(n-1)%26)); n=(n-1)/26; } xml.writeStartElement("c"); xml.writeAttribute("r",letters+QString::number(row+15));
        if (numericDate.isValid() && row==2 && col==0) { xml.writeAttribute("s","1"); const double serial=(date1904?QDate(1904,1,1):QDate(1899,12,30)).daysTo(numericDate.date())+numericDate.time().msecsSinceStartOfDay()/86400000.0; xml.writeTextElement("v",QString::number(serial,'f',10)); }
        else if (shared) { xml.writeAttribute("t","s"); xml.writeTextElement("v",QString::number(index++)); sst.writeStartElement("si"); sst.writeTextElement("t",rows[row][col]); sst.writeEndElement(); }
        else { xml.writeAttribute("t","inlineStr"); xml.writeStartElement("is"); xml.writeTextElement("t",rows[row][col]); xml.writeEndElement(); } xml.writeEndElement(); }
        xml.writeEndElement(); }
    xml.writeEndElement(); xml.writeEndElement(); xml.writeEndDocument(); sst.writeEndElement(); sst.writeEndDocument(); parts["xl/worksheets/sheet1.xml"]=sheet; if (shared) parts["xl/sharedStrings.xml"]=strings; return zip(parts,compressed);
}
}
