#pragma once
#include "AISettings.h"
#include "MonthlyStoryTemplate.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <functional>
struct AIStubServer {
    QTcpServer server;
    QList<QJsonObject> requests;
    QList<QByteArray> requestBodies;
    int status=200,delay=0;
    std::function<QString(const QJsonObject &)> handler;
    AIStubServer() {
        server.listen(QHostAddress::LocalHost);
        handler=[](const QJsonObject &body) {
            const auto messages=body.value("messages").toArray(); const auto system=messages[0].toObject().value("content").toString(); const auto user=messages[1].toObject().value("content").toString();
            if (system.contains(QStringLiteral("月度消费复盘"))) {
                const auto facts=QJsonDocument::fromJson(user.toUtf8()).object(); ReviewStoryContent c;
                c.personaId=facts.value("persona_options").toArray().first().toString();
                c.subtitle=QStringLiteral("给生活留下一页故事");
                if (c.personaId=="custom") c.customName=QStringLiteral("生活拼图型");
                c.title=QStringLiteral("先留下足迹，再慢慢认识你。");
                c.story=QStringLiteral("这个月的小本子记下了 %1 的支出，这些真实的足迹拼成了自己的生活主题。每一笔记录，都能成为这一页故事里的小片段。回头看看其中的喜欢与取舍，再为下一次心动留一个参照。").arg(facts.value("recorded_expenses").toString());
                c.nextStep=QStringLiteral("挑一笔消费，补一句自己的感受。");
                const auto samples=facts.value("selected_expenses").toArray();
                if (c.personaId!="squirrel" && !samples.isEmpty()) c.evidenceIds.append(samples.first().toObject().value("id").toString());
                return MonthlyStoryTemplate::encode(c);
            }
            if (system.contains(QStringLiteral("历史支出提供细分类建议"))) { const auto source=QJsonDocument::fromJson(user.toUtf8()).array(); QJsonArray result; for (const auto &value:source) result.append(QJsonObject{{"id",value.toObject().value("id")},{"category","daily"}}); return QString::fromUtf8(QJsonDocument(QJsonObject{{"items",result}}).toJson(QJsonDocument::Compact)); }
            if (system.contains(QStringLiteral("从用户文字或商品截图"))) return QString::fromUtf8(QJsonDocument(QJsonObject{{"name",QStringLiteral("测试笔记本")},{"amount","12.34"},{"category","study"},{"note",""}}).toJson(QJsonDocument::Compact));
            return QStringLiteral("连接成功");
        };
        QObject::connect(&server,&QTcpServer::newConnection,&server,[this] { while (server.hasPendingConnections()) { auto *socket=server.nextPendingConnection();
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[this,socket] {
                const auto bytes=socket->property("request").toByteArray()+socket->readAll(); socket->setProperty("request",bytes); const auto end=bytes.indexOf("\r\n\r\n"); if (end<0 || socket->property("sent").toBool()) return;
                const auto length=QRegularExpression("content-length: *([0-9]+)",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(bytes.left(end))); if (!length.hasMatch() || bytes.size()<end+4+length.captured(1).toInt()) return;
                socket->setProperty("sent",true); const auto raw=bytes.mid(end+4,length.captured(1).toInt()); const auto request=QJsonDocument::fromJson(raw).object(); requests << request; requestBodies << raw;
                const auto content=handler(request); const auto body=QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",content}}}}}}}).toJson(QJsonDocument::Compact); const auto code=status;
                QTimer::singleShot(delay,socket,[socket,body,code] { if (socket->state()!=QAbstractSocket::UnconnectedState) { socket->write("HTTP/1.1 "+QByteArray::number(code)+" Fixture\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body); socket->disconnectFromHost(); } });
            }); QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        } });
    }
    AISettings settings() const { AISettings value; value.baseUrl=QStringLiteral("http://127.0.0.1:%1/v1").arg(server.serverPort()); value.model="fixture-vision"; value.apiKey="fixture-only-key"; return value; }
};
