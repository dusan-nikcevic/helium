#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class QNetworkReply;
class AiClient : public QObject {
    Q_OBJECT
public:
    AiClient(const QUrl &endpoint, const QString &model, const QByteArray &key, QObject *parent = nullptr);
    ~AiClient() override;
    bool configured() const;
    void requestPlan(const QString &text, const QVariantMap &context);
    void cancel();
signals:
    void planReceived(QString text, QVariantMap request);
    void failed(QString message);
private:
    QUrl m_endpoint;
    QString m_model;
    QByteArray m_key;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timeout;
};
