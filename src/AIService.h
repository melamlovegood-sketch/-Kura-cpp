#pragma once
#include "AISettings.h"
#include "MonthlyStoryTemplate.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QMap>
#include <QSet>
class QNetworkReply;
class QJsonArray;
struct ParsedProduct {
    QString name, category = QStringLiteral("other"), note;
    qint64 amountCents = 0;
    QString purposeCategory, budgetScope;
};
Q_DECLARE_METATYPE(ParsedProduct)
struct ParsedPrice {
    QString name,category=QStringLiteral("other"),source,note;
    qint64 amountCents=0;
};
Q_DECLARE_METATYPE(ParsedPrice)
struct MonthlyAnalysis { QString story,persona; ReviewStoryContent content; };
struct BillClassifications { QMap<int,QString> categories; };
Q_DECLARE_METATYPE(MonthlyAnalysis)
Q_DECLARE_METATYPE(BillClassifications)
class AIService : public QObject {
    Q_OBJECT
public:
    explicit AIService(QObject *parent=nullptr);
    ~AIService() override;
    bool analyze(const AISettings &settings, const QString &text, const QByteArray &jpeg);
    bool analyzePrice(const AISettings &settings,const QString &context,const QByteArray &jpeg);
    bool analyzeDetails(const AISettings &settings,const QString &context,const QByteArray &jpeg);
    bool advise(const AISettings &settings,const QString &system,const QString &question,const QJsonArray &history={});
    bool monthly(const AISettings &settings,const QString &factsJson);
    bool classify(const AISettings &settings,const QJsonArray &items);
    void cancel();
    bool busy() const;
    static bool decodeProduct(const QByteArray &response, ParsedProduct &product, QString &error);
    static bool decodePrice(const QByteArray &response,ParsedPrice &price,QString &error);
    static bool decodeDetails(const QByteArray &response,QJsonObject &fields,QString &note,QString &error);
    static bool decodeMonthly(const QByteArray &response,MonthlyAnalysis &analysis,QString &error);
signals:
    void parsed(const ParsedProduct &product);
    void priceParsed(const ParsedPrice &price);
    void detailsParsed(const QJsonObject &fields,const QString &note);
    void failed(const QString &message);
    void textReady(const QString &text);
    void monthlyReady(const MonthlyAnalysis &analysis);
    void classified(const BillClassifications &result);
private:
    enum class Mode { Product,Price,Details,Advice,Monthly,Classify };
    bool send(const AISettings &settings,const QJsonArray &messages,Mode mode);
    QSet<int> m_classificationIds;
    QJsonObject m_monthlyFacts;
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply=nullptr;
};
