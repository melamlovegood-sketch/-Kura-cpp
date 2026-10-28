#pragma once
#include <QDate>
#include <QTime>
#include <QStringList>
#include <functional>
struct BillDraft {
    int sourceRow=0;
    QString name,originalName,note,category,platform,externalId,key,payloadHash,fingerprint,reason;
    QDate date; QTime time;
    qint64 cents=0;
    bool selected=true,confirmed=false;
    enum State { Ready,NeedsReview,PossibleDuplicate,Duplicate,Conflict } state=Ready;
};
struct BillParseResult {
    QList<BillDraft> rows;
    QString platform,fileName,fileHash;
    int total=0,filtered=0,invalid=0;
    QStringList skippedReasons;
};
namespace BillParser {
bool parseFile(const QString &path,BillParseResult &result,QString &error,const QString &platform={},bool includeTransfers=false,const std::function<bool()> &cancelled={});
bool parseRows(const QList<QStringList> &rows,BillParseResult &result,QString &error,const QString &platform={},bool includeTransfers=false,const QString &fileHash={},const std::function<bool()> &cancelled={});
bool parseCsv(const QByteArray &bytes,QList<QStringList> &rows,QString &error);
bool money(QString text,qint64 &cents);
QString normalized(const QString &text);
QString guessCategory(const QString &text,bool &certain);
QString hash(const QStringList &fields);
}
