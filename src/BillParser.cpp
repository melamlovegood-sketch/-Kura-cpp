#include "BillParser.h"
#include "XlsxReader.h"
#include "CategoryCatalog.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

QString BillParser::hash(const QStringList &fields) { QCryptographicHash digest(QCryptographicHash::Sha256); for (const auto &field:fields) { const auto bytes=field.toUtf8(); digest.addData(QByteArray::number(bytes.size())+":"+bytes); } return QString::fromLatin1(digest.result().toHex()); }
QString BillParser::normalized(const QString &text) { auto result=text.normalized(QString::NormalizationForm_KC).toCaseFolded(); result.remove(QRegularExpression("\\s+")); return result; }
bool BillParser::money(QString text,qint64 &cents)
{
    text=text.trimmed(); text.remove(QRegularExpression(QStringLiteral("[¥￥,，\\s元]")));
    const auto match=QRegularExpression("^\\+?([0-9]{1,18})(?:\\.([0-9]{1,12}))?(?:[eE]([+-]?[0-9]{1,2}))?$").match(text); if (!match.hasMatch()) return false;
    QString digits=match.captured(1)+match.captured(2); int scale=match.captured(2).size()-match.captured(3).toInt();
    while (digits.size()>1 && digits.startsWith('0')) digits.remove(0,1);
    if (scale>2) { const int extra=scale-2; if (extra>digits.size()) return false; for (const auto ch:digits.right(extra)) if (ch!='0') return false; digits.chop(extra); scale=2; }
    if (scale<2) { if (digits.size()+2-scale>12) return false; digits+=QString(2-scale,'0'); }
    bool ok=false; const auto value=digits.toLongLong(&ok); if (!ok || value<1 || value>9999999999LL) return false; cents=value; return true;
}
QString BillParser::guessCategory(const QString &text,bool &certain)
{
    const QList<QPair<QString,QString>> patterns={{"transport",QStringLiteral("地铁|公交|滴滴|打车|铁路|火车|高铁|交通")},{"canteen",QStringLiteral("食堂|饭卡")},{"daily_supplies",QStringLiteral("洗发|沐浴|纸巾|洗衣|牙膏|洗洁|日用")},{"subscription",QStringLiteral("订阅|会员|网盘|月费")},{"daily",QStringLiteral("奶茶|咖啡|外卖|美团|饿了么|餐|饭|食品")},{"entertainment",QStringLiteral("电影|演出|游戏|乐园|影院")},{"study",QStringLiteral("教材|教辅|学费")},{"online_shopping",QStringLiteral("淘宝|京东|拼多多|天猫|网购")}};
    for (const auto &pattern:patterns) if (QRegularExpression(pattern.second).match(text).hasMatch()) { certain=true; return pattern.first; } certain=false; return "other";
}
bool BillParser::parseCsv(const QByteArray &bytes,QList<QStringList> &rows,QString &error)
{
    error.clear(); QString text;
    if (bytes.startsWith("\xff\xfe")) { QStringDecoder decoder(QStringDecoder::Utf16LE); text=decoder(bytes.mid(2)); if (decoder.hasError()) { error=QStringLiteral("CSV UTF-16 编码损坏。"); return false; } }
    else if (bytes.startsWith("\xfe\xff")) { QStringDecoder decoder(QStringDecoder::Utf16BE); text=decoder(bytes.mid(2)); if (decoder.hasError()) { error=QStringLiteral("CSV UTF-16 编码损坏。"); return false; } }
    else { QStringDecoder decoder(QStringDecoder::Utf8); text=decoder(bytes.startsWith("\xef\xbb\xbf")?bytes.mid(3):bytes); if (decoder.hasError()) {
#ifdef Q_OS_WIN
        const auto size=MultiByteToWideChar(54936,MB_ERR_INVALID_CHARS,bytes.constData(),bytes.size(),nullptr,0); if (size<=0) { error=QStringLiteral("CSV 不是有效的 UTF-8／GB18030 文本。"); return false; } text.resize(size); if (!MultiByteToWideChar(54936,MB_ERR_INVALID_CHARS,bytes.constData(),bytes.size(),reinterpret_cast<wchar_t *>(text.data()),size)) { error=QStringLiteral("CSV 中文编码读取失败。"); return false; }
#else
        error=QStringLiteral("请将 CSV 另存为 UTF-8 后再导入。"); return false;
#endif
    } }
    if (text.contains(QChar(0))) { error=QStringLiteral("CSV 中含无效二进制内容。"); return false; }
    QList<QStringList> loaded; QStringList row; QString field; bool quoted=false,closed=false;
    for (qsizetype i=0;i<text.size();++i) {
        const auto ch=text[i];
        if (quoted) { if (ch=='"') { if (i+1<text.size() && text[i+1]=='"') { field+='"'; ++i; } else { quoted=false; closed=true; } } else field+=ch; }
        else if (ch==',' || ch=='\n' || ch=='\r') { row << field; field.clear(); closed=false; if (row.size()>256) { error=QStringLiteral("CSV 列数过多。"); return false; } if (ch!=',') { loaded << row; row.clear(); if (ch=='\r' && i+1<text.size() && text[i+1]=='\n') ++i; } }
        else if (ch=='"' && field.trimmed().isEmpty() && !closed) { field.clear(); quoted=true; }
        else if (closed) { if (!ch.isSpace()) { error=QStringLiteral("CSV 引号结束后存在无效内容。"); return false; } }
        else field+=ch;
        if (field.size()>8000 || loaded.size()>100000) { error=QStringLiteral("CSV 内容过大或字段过长，请拆分导入。"); return false; }
    }
    if (quoted) { error=QStringLiteral("CSV 引号未闭合。"); return false; }
    if (!field.isEmpty() || !row.isEmpty() || closed) { row << field; loaded << row; } rows=loaded; return true;
}
bool BillParser::parseRows(const QList<QStringList> &rows,BillParseResult &result,QString &error,const QString &requested,bool includeTransfers,const QString &fileHash,const std::function<bool()> &cancelled)
{
    error.clear(); BillParseResult parsed; parsed.fileHash=fileHash; QStringList header; int headerRow=-1;
    const auto norm=[](QString text) { text=text.trimmed(); text.remove(QRegularExpression("\\s+")); text.replace(QChar(0xff08),'('); text.replace(QChar(0xff09),')'); return text; };
    for (int i=0;i<rows.size();++i) { QStringList cells; for (const auto &cell:rows[i]) cells << norm(cell); if (cells.contains(QStringLiteral("收/支")) || (cells.contains(QStringLiteral("日期")) && cells.contains(QStringLiteral("名称")) && cells.contains(QStringLiteral("金额(元)")))) { header=cells; headerRow=i; break; } }
    if (headerRow<0) { error=QStringLiteral("未找到账单表头，请选择微信、支付宝或 Kura 导出的原始 CSV／XLSX。"); return false; }
    const auto column=[&](const QStringList &choices,bool includes=false) { for (const auto &choice:choices) for (int i=0;i<header.size();++i) if (includes?header[i].contains(choice):header[i]==choice) return i; return -1; };
    const bool kura=header.contains(QStringLiteral("日期")) && header.contains(QStringLiteral("金额(元)")) && header.contains(QStringLiteral("名称"));
    QString prelude; for (int i=0;i<headerRow && prelude.size()<4000;++i) prelude+=rows[i].join(' ');
    parsed.platform=kura?QStringLiteral("kura"):prelude.contains(QStringLiteral("支付宝")) || header.contains(QStringLiteral("商品说明"))?QStringLiteral("alipay"):prelude.contains(QStringLiteral("微信")) || (header.contains(QStringLiteral("交易类型")) && (header.contains(QStringLiteral("商品")) || header.contains(QStringLiteral("商品名称"))))?QStringLiteral("wechat"):QString();
    if (!requested.isEmpty() && parsed.platform!=requested) { error=QStringLiteral("所选平台与表头不一致，请核对文件或选择自动识别。"); return false; }
    if (parsed.platform.isEmpty()) { error=QStringLiteral("无法识别账单平台。"); return false; }
    const int dateCol=column({QStringLiteral("交易时间"),QStringLiteral("交易创建时间"),QStringLiteral("付款时间"),QStringLiteral("日期")}), timeCol=kura?column({QStringLiteral("时间")}):-1;
    const int nameCol=column({QStringLiteral("商品说明"),QStringLiteral("商品名称"),QStringLiteral("商品"),QStringLiteral("名称")}), amountCol=column({QStringLiteral("金额")},true), ioCol=column({QStringLiteral("收/支")});
    const int statusCol=column({QStringLiteral("当前状态"),QStringLiteral("交易状态")}), partyCol=column({QStringLiteral("交易对方")},true), typeCol=column({QStringLiteral("交易类型"),QStringLiteral("交易分类")}), numberCol=column({QStringLiteral("交易单号"),QStringLiteral("交易订单号"),QStringLiteral("交易号")},true), categoryCol=column({QStringLiteral("细分类"),QStringLiteral("分类")}),noteCol=column({QStringLiteral("备注")});
    const int purposeCol=column({QStringLiteral("消费用途")}), scopeCol=column({QStringLiteral("预算归属")});
    if (dateCol<0 || nameCol<0 || amountCol<0 || (!kura && ioCol<0)) { error=QStringLiteral("账单缺少日期、商品、金额或收支字段。"); return false; }
    const auto get=[](const QStringList &row,int i) { return i>=0 && i<row.size()?row[i].trimmed():QString(); };
    for (int i=headerRow+1;i<rows.size();++i) {
        if (cancelled && cancelled()) { error=QStringLiteral("已取消读取。"); return false; }
        const auto &row=rows[i]; bool empty=true; for (const auto &cell:row) if (!cell.trimmed().isEmpty()) { empty=false; break; } if (empty) continue;
        ++parsed.total; QString reason; const auto status=get(row,statusCol),type=get(row,typeCol);
        if (!kura && get(row,ioCol)!=QStringLiteral("支出")) reason=QStringLiteral("非支出／说明行");
        else if (status.contains(QStringLiteral("退款")) || status.contains(QStringLiteral("关闭")) || status.contains(QStringLiteral("失败")) || status.contains(QStringLiteral("撤销"))) reason=QStringLiteral("退款、关闭或失败记录");
        else if (!includeTransfers && (type.contains(QStringLiteral("转账")) || type.contains(QStringLiteral("红包")))) reason=QStringLiteral("转账或红包（未勾选包含）");
        if (!reason.isEmpty()) { ++parsed.filtered; if (parsed.skippedReasons.size()<300) parsed.skippedReasons << QStringLiteral("第 %1 行：%2").arg(i+1).arg(reason); continue; }
        BillDraft draft; draft.platform=parsed.platform; draft.sourceRow=i+1; if (!money(get(row,amountCol),draft.cents)) reason=QStringLiteral("金额无效或不是正的整分金额");
        auto dateText=get(row,dateCol); if (kura && !get(row,timeCol).isEmpty()) dateText+=" "+get(row,timeCol);
        const auto date=QRegularExpression("^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})(?:[ T](\\d{1,2}):(\\d{2})(?::(\\d{2}))?(?:\\.\\d{1,3})?)?$").match(dateText);
        if (!date.hasMatch()) reason=QStringLiteral("日期或具体时间格式无效"); else { draft.date=QDate(date.captured(1).toInt(),date.captured(2).toInt(),date.captured(3).toInt()); if (!date.captured(4).isEmpty()) draft.time=QTime(date.captured(4).toInt(),date.captured(5).toInt(),date.captured(6).isEmpty()?0:date.captured(6).toInt()); if (!draft.date.isValid() || draft.date.year()<1 || draft.date.year()>9999 || (!date.captured(4).isEmpty() && !draft.time.isValid())) reason=QStringLiteral("日期或具体时间不存在"); }
        const auto goods=get(row,nameCol),party=get(row,partyCol); draft.originalName=parsed.platform=="wechat"?QStringList{party,goods}.join(' ').trimmed():goods.isEmpty()?party:goods;
        if (draft.originalName.isEmpty()) reason=QStringLiteral("商品和交易对方均为空");
        draft.note=get(row,noteCol); if (draft.note.size()>1000) reason=QStringLiteral("备注超过 1000 字，请先核对源文件");
        if (!reason.isEmpty()) { ++parsed.invalid; if (parsed.skippedReasons.size()<300) parsed.skippedReasons << QStringLiteral("第 %1 行：%2").arg(i+1).arg(reason); continue; }
        draft.name=draft.originalName.left(100); draft.externalId=get(row,numberCol); if (draft.externalId.startsWith("=\"") && draft.externalId.endsWith('"')) draft.externalId=draft.externalId.mid(2,draft.externalId.size()-3); if (draft.externalId.startsWith('\'')) draft.externalId.remove(0,1); if (draft.externalId.size()>256) { ++parsed.invalid; continue; }
        bool certain=false; draft.category=guessCategory(goods.isEmpty()?party:goods,certain); if (!certain) draft.category=guessCategory(draft.originalName,certain);
        if (kura && categoryCol>=0) for (const auto &key:CategoryCatalog::keys()) if (get(row,categoryCol)==key || get(row,categoryCol)==CategoryCatalog::label(key)) { draft.category=key; certain=true; break; }
        if (kura && !certain && categoryCol>=0) { const auto legacy=CategoryCatalog::fromLegacy(get(row,categoryCol)); if (!legacy.isEmpty()) { draft.category=legacy; certain=true; } }
        draft.purposeCategory=CategoryCatalog::purposeFromDetail(draft.category);
        draft.budgetScope=CategoryCatalog::isBasic(draft.category)?QStringLiteral("basic"):QStringLiteral("discretionary");
        // Only recognize explicit garment terms; an online purchase alone is insufficient.
        if (!kura && QRegularExpression(QStringLiteral("衣服|衬衫|外套|裙|裤|运动鞋|皮鞋|背包|手提包")).match(draft.originalName).hasMatch()) draft.purposeCategory="clothing";
        if (kura && purposeCol>=0) for (const auto &key:CategoryCatalog::purposeKeys()) if (get(row,purposeCol)==key || get(row,purposeCol)==CategoryCatalog::purposeLabel(key)) { draft.purposeCategory=key; break; }
        if (kura && scopeCol>=0) {
            const auto scope=get(row,scopeCol);
            if (scope==QStringLiteral("必要生活") || scope==QStringLiteral("基础生活") || scope=="basic") draft.budgetScope="basic";
            else if (scope==QStringLiteral("自由消费") || scope==QStringLiteral("可支配") || scope=="discretionary") draft.budgetScope="discretionary";
        }
        draft.fingerprint=hash({draft.date.toString(Qt::ISODate),draft.time.isValid()?draft.time.toString("HH:mm:ss"):QString(),QString::number(draft.cents),normalized(draft.originalName)});
        draft.payloadHash=hash({draft.platform,draft.externalId,draft.fingerprint});
        draft.key=hash(draft.externalId.isEmpty()?QStringList{draft.platform,"file",fileHash.isEmpty()?hash({QString::number(rows.size())}):fileHash,QString::number(i+1),draft.fingerprint}:QStringList{draft.platform,"external",draft.externalId});
        if (!certain || draft.originalName.size()>100) { draft.state=BillDraft::NeedsReview; draft.confirmed=false; draft.reason=draft.originalName.size()>100?QStringLiteral("原名称超过 100 字，请核对简称和分类"):QStringLiteral("分类为本地建议，请确认"); } else draft.confirmed=true;
        parsed.rows << draft;
    }
    result=parsed; return true;
}
bool BillParser::parseFile(const QString &path,BillParseResult &result,QString &error,const QString &platform,bool includeTransfers,const std::function<bool()> &cancelled)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size()>32*1024*1024 || file.size()==0) { error=QStringLiteral("无法读取账单，文件须为 1 字节至 32 MB。"); return false; } const auto bytes=file.readAll(); if (file.error()!=QFileDevice::NoError) { error=QStringLiteral("账单读取失败。"); return false; }
    QList<QStringList> rows; if (QFileInfo(path).suffix().compare("xlsx",Qt::CaseInsensitive)==0) { if (!XlsxReader::read(bytes,rows,error,cancelled)) return false; } else if (QFileInfo(path).suffix().compare("csv",Qt::CaseInsensitive)==0) { if (!parseCsv(bytes,rows,error)) return false; } else { error=QStringLiteral("请选择 CSV 或 XLSX 文件，旧版 XLS 请先另存为 XLSX。"); return false; }
    BillParseResult parsed; if (!parseRows(rows,parsed,error,platform,includeTransfers,QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()),cancelled)) return false; parsed.fileName=QFileInfo(path).fileName(); result=parsed; return true;
}
