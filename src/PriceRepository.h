#pragma once
#include "DatabaseManager.h"
#include <QDateTime>
struct PriceRecord { qint64 id=0,trackId=0,amountCents=0; QString source; QDateTime observedAt; };
struct PriceTrack {
    qint64 id=0,targetCents=0,latestCents=0,previousCents=0,lowestCents=0,latestRecordId=0,recordCount=0;
    QString name,category,sourceUrl,source;
    QDateTime observedAt;
    bool active=true,targetHit=false,priceDrop=false;
};
class PriceRepository {
public:
    explicit PriceRepository(DatabaseManager &database):m_database(database) {}
    bool load(QList<PriceTrack> &tracks,bool includeArchived=false);
    bool history(qint64 id,QList<PriceRecord> &records);
    bool intake(const QString &name,const QString &category,qint64 cents,const QString &source,const QString &token,qint64 &trackId,const QDateTime &observedAt=QDateTime::currentDateTimeUtc(),qint64 productId=0);
    bool record(qint64 id,qint64 cents,const QString &source,const QString &token,const QDateTime &observedAt=QDateTime::currentDateTimeUtc());
    bool configure(qint64 id,qint64 targetCents,const QString &sourceUrl);
    bool archive(qint64 id);
    bool dismiss(qint64 id,const QString &kind,qint64 recordId);
    static bool validLink(const QString &link);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
