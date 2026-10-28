#pragma once
#include <QJsonObject>
#include <QStringList>
struct MonthlyReviewData;

struct ReviewStoryContent {
    QString personaId,subtitle,title,story,interlude,nextStep;
    QStringList evidenceIds;
    QString customName;
    int schemaVersion=2;
};
namespace MonthlyStoryTemplate {
QString personaName(const QString &id);
QString personaSymbol(const QString &id);
QString personaCaption(const ReviewStoryContent &content);
QString prompt();
void addContext(QJsonObject &facts,const MonthlyReviewData &data);
QString encode(const ReviewStoryContent &content);
bool decode(const QString &json,ReviewStoryContent &content,QString &error);
bool matchesFacts(const ReviewStoryContent &content,const QJsonObject &facts,QString &error);
ReviewStoryContent local(const MonthlyReviewData &data);
QString plainText(const ReviewStoryContent &content);
}
