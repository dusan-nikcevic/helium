#include <memory>
#include "../aiclient.h"
#include "../aiplanner.h"
#include "../session.h"
#include "aiclientcheck.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

int runAiClientChecks() {
    int count = 0, failures = 0;
    auto check = [&](bool ok, const char *message) { ++count; if (!ok) { ++failures; qCritical() << "AI client FAIL:" << message; } };
    AiClient remote(QUrl("http://example.com/v1/chat/completions"), "configured-model", "test-key");
    check(!remote.configured(), "remote plain HTTP is rejected");
    AiClient incomplete(QUrl("https://api.example.com/v1/chat/completions"), "configured-model", {});
    check(!incomplete.configured(), "remote endpoint requires a key");
    AiClient unsafeKey(QUrl("https://api.example.com/v1/chat/completions"), "configured-model", "key\ninvalid");
    check(!unsafeKey.configured(), "header delimiters are rejected");
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) { check(false, "local server starts"); return 1; }
    const QUrl endpoint("http://127.0.0.1:" + QString::number(server.serverPort()) + "/v1/chat/completions");
    QString content = "{\"phase\":\"preview\",\"source\":\"ai\"}";
    QString finish = "stop";
    int httpStatus = 200;
    bool requestShape = false;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            auto input = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, input] {
                input->append(socket->readAll());
                const int split = input->indexOf("\r\n\r\n");
                if (split < 0) return;
                int size = 0;
                for (const auto &line : input->left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:")) size = line.mid(15).trimmed().toInt();
                if (input->size() - split - 4 < size) return;
                const auto body = QJsonDocument::fromJson(input->mid(split + 4, size)).object();
                requestShape = body.value("model").toString() == "configured-model" &&
                    body.value("messages").toArray().size() == 2 &&
                    body.value("response_format").toObject().value("type") == "json_object";
                const QByteArray reply = QJsonDocument(QJsonObject{{"choices", QJsonArray{
                    QJsonObject{{"finish_reason", finish}, {"message", QJsonObject{{"content", content}}}}
                }}}).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 " + QByteArray::number(httpStatus) + " Test\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(reply.size()) + "\r\nConnection: close\r\n\r\n" + reply);
                socket->disconnectFromHost();
                socket->disconnect(socket);
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    AiClient client(endpoint, "configured-model", {});
    check(client.configured(), "local endpoint can omit a key");
    auto run = [&] {
        QEventLoop loop;
        bool received = false, rejected = false;
        const auto got = QObject::connect(&client, &AiClient::planReceived, &loop, [&](const QString &, const QVariantMap &plan) {
            received = plan.value("phase") == "preview"; loop.quit();
        });
        const auto failed = QObject::connect(&client, &AiClient::failed, &loop, [&](const QString &) { rejected = true; loop.quit(); });
        QTimer limit; limit.setSingleShot(true);
        QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
        limit.start(3000);
        client.requestPlan("Lower the selected track by 3 dB", {{"sessionId", "test-session"}});
        loop.exec();
        QObject::disconnect(got); QObject::disconnect(failed); client.cancel();
        return QPair<bool, bool>(received, rejected);
    };
    auto result = run();
    check(result.first && !result.second, "valid completion emits a plan without executing it");
    check(requestShape, "request uses model, messages and JSON mode");
    content = "not JSON"; result = run();
    check(!result.first && result.second, "malformed plan is rejected");
    content = "{\"phase\":\"preview\"}"; finish = "length"; result = run();
    check(!result.first && result.second, "truncated output is rejected");
    finish = "stop"; httpStatus = 401; result = run();
    check(!result.first && result.second, "authentication failure is rejected");
    Session state;
    check(state.loadFixture(QStringLiteral(ZEPHYR_SOURCE_DIR "/fixtures/midnight.json")), "integration fixture loads");
    state.selectTrack("lead-vocal"); state.setExternalPlanner(true);
    check(state.aiProvider() == "OpenRouter", "application identifies the selected provider");
    const auto projectBefore = state.project();
    const auto planned = planAiCommand("lower gain by 3 dB", state.selectedResolvedContext());
    content = QString::fromUtf8(QJsonDocument::fromVariant(planned.value("request")).toJson(QJsonDocument::Compact));
    finish = "stop"; httpStatus = 200;
    QObject::connect(&state, &Session::aiPlanningRequested, &client, &AiClient::requestPlan);
    QObject::connect(&client, &AiClient::planReceived, &state, &Session::acceptAiPlan);
    QObject::connect(&client, &AiClient::failed, &state, &Session::failAiPlan);
    QEventLoop integration;
    QObject::connect(&state, &Session::changed, &integration, [&] {
        if (state.aiState() == "preview") integration.quit();
    });
    QTimer limit; limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &integration, &QEventLoop::quit);
    limit.start(3000);
    check(state.submitAi("lower gain by 3 dB"), "provider integration submits the selected command");
    integration.exec();
    check(state.aiState() == "preview" && state.project() == projectBefore, "provider completion stages authoritative preview without mutation");
    check(state.applyAi() && state.selectedTrack().value("volumeDb").toDouble() == projectBefore.value("tracks").toList()[2].toMap().value("volumeDb").toDouble() - 3,
          "provider plan applies only through explicit Apply");
    check(state.undo() && state.project() == projectBefore, "provider plan undo restores the project exactly");
    qInfo().noquote() << QString("AI client: %1 checks, %2 failures").arg(count).arg(failures);
    return failures ? 1 : 0;
}
