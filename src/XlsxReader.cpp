#include "XlsxReader.h"
#include <QByteArray>
#include <QDate>
#include <QDir>
#include <QHash>
#include <QRegularExpression>
#include <QTime>
#include <QXmlStreamReader>
#include <QtEndian>
#include <zlib.h>
#include <cmath>

namespace {
constexpr qsizetype MaxFile=32*1024*1024,MaxPart=24*1024*1024;
quint16 u16(const QByteArray &bytes,qsizetype offset) { return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData()+offset)); }
quint32 u32(const QByteArray &bytes,qsizetype offset) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()+offset)); }
struct Entry { quint16 method=0,flags=0; quint32 crc=0,packed=0,size=0,offset=0; };
class Archive {
public:
    QByteArray bytes; QHash<QString,Entry> entries;
    bool open(const QByteArray &input,QString &error) {
        bytes=input;
        if (bytes.size()<22 || bytes.size()>MaxFile) { error=QStringLiteral("Excel 文件为空、过大或格式无效。"); return false; }
        qsizetype end=-1; for (qsizetype i=bytes.size()-22;i>=qMax(qsizetype(0),bytes.size()-65557);--i) if (u32(bytes,i)==0x06054b50 && i+22+u16(bytes,i+20)==bytes.size()) { end=i; break; }
        if (end<0 || u16(bytes,end+4)!=0 || u16(bytes,end+6)!=0 || u16(bytes,end+8)!=u16(bytes,end+10)) { error=QStringLiteral("不支持加密、分卷或旧版 Excel 文件，请另存为 CSV 或普通 XLSX。"); return false; }
        const auto count=u16(bytes,end+10); const auto start=u32(bytes,end+16),length=u32(bytes,end+12);
        if (count==65535 || count>1024 || quint64(start)+length>quint64(end)) { error=QStringLiteral("Excel 压缩目录过大或损坏。"); return false; }
        qsizetype at=start; quint64 expanded=0;
        for (int i=0;i<count;++i) {
            if (at+46>end || u32(bytes,at)!=0x02014b50) { error=QStringLiteral("Excel 压缩目录损坏。"); return false; }
            Entry e; e.flags=u16(bytes,at+8); e.method=u16(bytes,at+10); e.crc=u32(bytes,at+16); e.packed=u32(bytes,at+20); e.size=u32(bytes,at+24); e.offset=u32(bytes,at+42);
            const auto nameLength=u16(bytes,at+28),extra=u16(bytes,at+30),comment=u16(bytes,at+32);
            if (at+46+nameLength+extra+comment>end || e.size>MaxPart || e.packed>MaxFile || (e.flags&1) || (e.method!=0 && e.method!=8) || e.offset==0xffffffff) { error=QStringLiteral("Excel 包含不支持的加密、压缩或过大内容。"); return false; }
            const auto name=QString::fromUtf8(bytes.mid(at+46,nameLength));
            if (entries.contains(name)) { error=QStringLiteral("Excel 包含重复的内部文件名。"); return false; }
            entries.insert(name,e); expanded+=e.size; if (expanded>64*1024*1024) { error=QStringLiteral("Excel 展开内容过大，请拆分账单。"); return false; }
            at+=46+nameLength+extra+comment;
        }
        if (at!=qsizetype(start)+length) { error=QStringLiteral("Excel 压缩目录长度不一致。"); return false; } return true;
    }
    bool part(const QString &name,QByteArray &result,QString &error,bool optional=false) const {
        if (!entries.contains(name)) { if (optional) { result.clear(); return true; } error=QStringLiteral("Excel 缺少工作簿或工作表。"); return false; }
        const auto e=entries.value(name); const qsizetype at=e.offset;
        if (at+30>bytes.size() || u32(bytes,at)!=0x04034b50) { error=QStringLiteral("Excel 内部数据头损坏。"); return false; }
        const quint64 start=quint64(at)+30+u16(bytes,at+26)+u16(bytes,at+28);
        if (start+e.packed>quint64(bytes.size())) { error=QStringLiteral("Excel 内部数据越界。"); return false; }
        const auto packed=bytes.mid(start,e.packed); QByteArray plain;
        if (e.method==0) plain=packed;
        else {
            plain.resize(e.size); z_stream stream{}; stream.next_in=reinterpret_cast<Bytef *>(const_cast<char *>(packed.constData())); stream.avail_in=packed.size();
            uchar empty=0; stream.next_out=e.size?reinterpret_cast<Bytef *>(plain.data()):&empty; stream.avail_out=e.size?e.size:1;
            if (inflateInit2(&stream,-MAX_WBITS)!=Z_OK) { error=QStringLiteral("无法读取 Excel 压缩内容。"); return false; }
            const auto status=inflate(&stream,Z_FINISH); const bool ok=status==Z_STREAM_END && stream.total_out==e.size && stream.total_in==e.packed; inflateEnd(&stream);
            if (!ok) { error=QStringLiteral("Excel 压缩内容损坏。"); return false; }
        }
        if (plain.size()!=e.size || quint32(crc32(0,reinterpret_cast<const Bytef *>(plain.constData()),plain.size()))!=e.crc) { error=QStringLiteral("Excel 内容校验失败。"); return false; }
        result=plain; return true;
    }
};
bool xmlOk(QXmlStreamReader &reader,QString &error) { if (reader.hasError()) { error=QStringLiteral("Excel XML 内容损坏：%1").arg(reader.errorString()); return false; } return true; }
bool isDateFormat(QString format) { format.remove(QRegularExpression(QStringLiteral("\"[^\"]*\"|\\[[^\\]]*\\]|\\\\."))); return format.contains(QRegularExpression("[ymdhs]",QRegularExpression::CaseInsensitiveOption)); }
QString excelDate(const QString &text,bool date1904) {
    bool ok=false; const double value=text.toDouble(&ok); if (!ok || !std::isfinite(value) || value<0 || value>3000000) return {};
    const auto days=qint64(std::floor(value)); if (!date1904 && days==60) return {};
    const auto seconds=qRound64((value-days)*86400); QDate date=(date1904?QDate(1904,1,1):(days<60?QDate(1899,12,31):QDate(1899,12,30))).addDays(days+seconds/86400);
    if (!date.isValid() || date.year()<1 || date.year()>9999) return {};
    return date.toString(Qt::ISODate)+" "+QTime(0,0).addSecs(seconds%86400).toString("HH:mm:ss");
}
}

bool XlsxReader::read(const QByteArray &file,QList<QStringList> &rows,QString &error,const std::function<bool()> &cancelled)
{
    error.clear(); Archive zip; if (!zip.open(file,error)) return false;
    QByteArray part; if (!zip.part("xl/workbook.xml",part,error)) return false;
    bool date1904=false; QString relationship; QXmlStreamReader workbook(part);
    while (!workbook.atEnd()) { workbook.readNext(); if (workbook.tokenType()==QXmlStreamReader::DTD) { error=QStringLiteral("Excel XML 不支持 DTD。"); return false; } if (!workbook.isStartElement()) continue; if (workbook.name()=="workbookPr") date1904=workbook.attributes().value("date1904")=="1" || workbook.attributes().value("date1904")=="true"; if (workbook.name()=="sheet" && relationship.isEmpty()) { relationship=workbook.attributes().value("http://schemas.openxmlformats.org/officeDocument/2006/relationships","id").toString(); } }
    if (!xmlOk(workbook,error) || relationship.isEmpty()) { if (error.isEmpty()) error=QStringLiteral("Excel 没有可读取的工作表。"); return false; }
    if (!zip.part("xl/_rels/workbook.xml.rels",part,error)) return false; QXmlStreamReader rels(part); QString target;
    while (!rels.atEnd()) { rels.readNext(); if (rels.tokenType()==QXmlStreamReader::DTD) { error=QStringLiteral("Excel XML 不支持 DTD。"); return false; } if (rels.isStartElement() && rels.name()=="Relationship" && rels.attributes().value("Id")==relationship) { if (rels.attributes().value("TargetMode")=="External") { error=QStringLiteral("不支持外部工作表链接。"); return false; } target=rels.attributes().value("Target").toString(); } }
    if (!xmlOk(rels,error) || target.isEmpty()) { if (error.isEmpty()) error=QStringLiteral("无法定位 Excel 第一张工作表。"); return false; }
    const auto sheet=QDir::cleanPath(target.startsWith('/')?target.mid(1):"xl/"+target); if (!sheet.startsWith("xl/") || sheet.contains('\\')) { error=QStringLiteral("Excel 工作表路径无效。"); return false; }
    QStringList strings; if (!zip.part("xl/sharedStrings.xml",part,error,true)) return false;
    if (!part.isEmpty()) { QXmlStreamReader xml(part); QString current; bool inItem=false; qsizetype chars=0;
        while (!xml.atEnd()) { xml.readNext(); if (xml.tokenType()==QXmlStreamReader::DTD) { error=QStringLiteral("Excel XML 不支持 DTD。"); return false; } if (xml.isStartElement() && xml.name()=="si") { current.clear(); inItem=true; } else if (inItem && xml.isStartElement() && xml.name()=="t") current+=xml.readElementText(); else if (xml.isEndElement() && xml.name()=="si") { strings << current; chars+=current.size(); inItem=false; if (strings.size()>300000 || chars>16*1024*1024) { error=QStringLiteral("Excel 文本内容过大。"); return false; } } if (cancelled && cancelled()) { error=QStringLiteral("已取消读取。"); return false; } } if (!xmlOk(xml,error)) return false; }
    QHash<int,QString> formats; QList<bool> dateStyles; if (!zip.part("xl/styles.xml",part,error,true)) return false;
    if (!part.isEmpty()) { QXmlStreamReader xml(part); bool inXfs=false; while (!xml.atEnd()) { xml.readNext(); if (xml.tokenType()==QXmlStreamReader::DTD) { error=QStringLiteral("Excel XML 不支持 DTD。"); return false; } if (xml.isStartElement() && xml.name()=="numFmt") formats.insert(xml.attributes().value("numFmtId").toInt(),xml.attributes().value("formatCode").toString()); if (xml.isStartElement() && xml.name()=="cellXfs") inXfs=true; else if (xml.isEndElement() && xml.name()=="cellXfs") inXfs=false; if (inXfs && xml.isStartElement() && xml.name()=="xf") { const int id=xml.attributes().value("numFmtId").toInt(); dateStyles << ((id>=14 && id<=22) || (id>=45 && id<=47) || (formats.contains(id) && isDateFormat(formats[id]))); } } if (!xmlOk(xml,error)) return false; }
    if (!zip.part(sheet,part,error)) return false; QXmlStreamReader xml(part); QList<QStringList> loaded; int rowIndex=-1,column=-1,style=0; QString type,value; bool inCell=false;
    while (!xml.atEnd()) {
        xml.readNext(); if (xml.tokenType()==QXmlStreamReader::DTD) { error=QStringLiteral("Excel XML 不支持 DTD。"); return false; }
        if (xml.isStartElement() && xml.name()=="row") { bool ok=false; const int number=xml.attributes().value("r").toInt(&ok); rowIndex=ok?number-1:rowIndex+1; column=-1; if (rowIndex<0 || rowIndex>=100000) { error=QStringLiteral("Excel 超过 10 万行，请拆分导入。"); return false; } if (loaded.size()<=rowIndex) loaded.resize(rowIndex+1); }
        else if (xml.isStartElement() && xml.name()=="c") { if (rowIndex<0) { error=QStringLiteral("Excel 单元格缺少所属行。"); return false; } const auto ref=xml.attributes().value("r").toString(); int col=0; for (const auto ch:ref) { if (ch<'A' || ch>'Z') break; col=col*26+(ch.unicode()-'A'+1); } column=col?col-1:column+1; if (column<0 || column>=256) { error=QStringLiteral("Excel 列数超出支持范围。"); return false; } type=xml.attributes().value("t").toString(); style=xml.attributes().value("s").toInt(); value.clear(); inCell=true; }
        else if (inCell && xml.isStartElement() && (xml.name()=="v" || (type=="inlineStr" && xml.name()=="t"))) value+=xml.readElementText();
        else if (xml.isEndElement() && xml.name()=="c" && inCell) { QString text=value; if (type=="s") { bool ok=false; const int index=value.toInt(&ok); if (!ok || index<0 || index>=strings.size()) { error=QStringLiteral("Excel 共享字符串索引无效。"); return false; } text=strings[index]; } else if (type=="b") text=value=="1"?"TRUE":"FALSE"; else if ((type.isEmpty() || type=="n") && style>=0 && style<dateStyles.size() && dateStyles[style]) text=excelDate(value,date1904); if (text.size()>8000) { error=QStringLiteral("Excel 单元格内容过长。"); return false; } if (loaded[rowIndex].size()<=column) loaded[rowIndex].resize(column+1); loaded[rowIndex][column]=text; inCell=false; }
        if (cancelled && cancelled()) { error=QStringLiteral("已取消读取。"); return false; }
    }
    if (!xmlOk(xml,error)) return false; rows=loaded; return true;
}
