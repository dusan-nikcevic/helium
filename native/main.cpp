#include "session.h"
#include "waveform.h"
#include "aiclient.h"
#include "tests/aiplannercheck.h"
#include "tests/mixhistorycheck.h"
#include "tests/selftest.h"
#include "tests/aiclientcheck.h"
#ifdef ZEPHYR_ENGINE
#include "engine/ardourbridge.h"
#include "engine/checks.h"
#endif
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDebug>
#include <memory>
#include <cstdio>

int main(int argc, char **argv) {
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
        const QByteArray text = message.toLocal8Bit();
        std::fprintf(stderr, "%s\n", text.constData());
    });
    bool selfTest = false, engineTest = false;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == "--self-test") selfTest = true;
        if (argument == "--engine-self-test") engineTest = true;
    }
    std::unique_ptr<QCoreApplication> app;
    if (selfTest || engineTest) app = std::make_unique<QCoreApplication>(argc, argv);
    else app = std::make_unique<QGuiApplication>(argc, argv);
    QCoreApplication::setApplicationName("Zephyr");
    QCoreApplication::setApplicationVersion("0.1");
    QCommandLineParser parser;
    parser.setApplicationDescription("Zephyr native UI. Optional libardour Dummy sessions expose mixer and transport controls.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"self-test", "Run Session state checks without a GUI."});
    parser.addOption({"test-ui", "Run qml/TestHarness.qml and return its test result."});
    parser.addOption({"engine-self-test", "Check real libardour sessions with the Dummy backend."});
    parser.addOption({"test-engine-ui", "Check native controls against a real engine session."});
    parser.addOption({"engine-session", "Open an Ardour session with the Dummy backend.", "directory"});
    parser.addOption({"new-engine-session", "Create an Ardour session with the Dummy backend.", "directory"});
    parser.addOption({"session-name", "Ardour snapshot name; defaults to the session folder name.", "name"});
    parser.addOption({"view", "Initial view: arrange, mix, split, session.", "view", "arrange"});
    parser.addOption({"width", "Window width.", "pixels", "1440"});
    parser.addOption({"height", "Window height.", "pixels", "900"});
    parser.addOption({"screenshot", "Save the window to an absolute PNG path and quit.", "path"});
    parser.addOption({"fixture", "Load an alternate fixture JSON file.", "path", QStringLiteral(ZEPHYR_SOURCE_DIR "/fixtures/midnight.json")});
    parser.process(*app);
    if (selfTest && engineTest) { qCritical() << "Choose one state check suite."; return 2; }
    if (selfTest) {
        const int result = runSelfTests(parser.value("fixture"));
        if (result) return result;
        if (const int checks = runAiClientChecks()) return checks;
        if (const int checks = runAiPlannerChecks()) return checks;
        return runMixHistoryChecks();
    }
    const bool engineMode = parser.isSet("engine-session") || parser.isSet("new-engine-session");
    if (parser.isSet("engine-session") && parser.isSet("new-engine-session")) { qCritical() << "Choose an existing or a new engine session."; return 2; }
    if (engineMode && (parser.isSet("fixture") || parser.isSet("test-ui"))) { qCritical() << "Engine sessions cannot run fixture checks or load fixture data."; return 2; }
    if (parser.isSet("test-engine-ui") && !engineMode) { qCritical() << "Engine UI checks require an engine session."; return 2; }
    if (parser.isSet("test-engine-ui") && !parser.isSet("new-engine-session")) { qCritical() << "Engine UI checks require a new disposable session."; return 2; }
#ifndef ZEPHYR_ENGINE
    if (engineMode || engineTest || parser.isSet("test-engine-ui")) { qCritical() << "Build engine support with sh native/run-engine.sh."; return 2; }
#else
    if (engineTest) {
        const QString directory = parser.value("engine-session");
        const QString name = parser.isSet("session-name") ? parser.value("session-name") : QFileInfo(directory).fileName();
        return runEngineChecks(directory, name);
    }
#endif
    const QString view = parser.value("view");
    if (!QStringList{"arrange", "mix", "split", "session"}.contains(view)) { qCritical() << "Invalid --view"; return 2; }
    bool widthOk, heightOk;
    const int width = parser.value("width").toInt(&widthOk), height = parser.value("height").toInt(&heightOk);
    if (!widthOk || !heightOk || width < 1100 || height < 640) { qCritical() << "Window size requires at least 1100 x 640."; return 2; }
    const QString output = parser.value("screenshot");
    if (!output.isEmpty() && (!QFileInfo(output).isAbsolute() || !output.endsWith(".png", Qt::CaseInsensitive))) {
        qCritical() << "--screenshot requires an absolute PNG path."; return 2;
    }
#ifdef ZEPHYR_ENGINE
    std::unique_ptr<ArdourBridge> bridge;
#endif
    Session state;
    if (engineMode) {
#ifdef ZEPHYR_ENGINE
        const bool create = parser.isSet("new-engine-session");
        const QString directory = parser.value(create ? "new-engine-session" : "engine-session");
        const QString name = parser.isSet("session-name") ? parser.value("session-name") : QFileInfo(directory).fileName();
        bridge = std::make_unique<ArdourBridge>();
        const bool loaded = create ? bridge->createSession(directory, name) : bridge->openSession(directory, name);
        if (!loaded) { qCritical().noquote() << bridge->error(); return 1; }
        if (create && (!bridge->addAudioTrack("Audio 1") || !bridge->saveSession())) { qCritical().noquote() << bridge->error(); return 1; }
        state.attachEngine(bridge.get());
#endif
    } else if (!state.loadFixture(parser.value("fixture"))) { qCritical().noquote() << state.notice(); return 1; }
    state.setView(engineMode && !parser.isSet("view") ? "mix" : view);
    AiClient ai(QUrl("https://openrouter.ai/api/v1/chat/completions"),
                qEnvironmentVariable("OPENROUTER_MODEL"), qgetenv("OPENROUTER_API_KEY"));
    if (!parser.isSet("test-ui") && !parser.isSet("test-engine-ui")) {
        state.setExternalPlanner(true);
        QObject::connect(&state, &Session::aiPlanningRequested, &ai, &AiClient::requestPlan);
        QObject::connect(&ai, &AiClient::planReceived, &state, &Session::acceptAiPlan);
        QObject::connect(&ai, &AiClient::failed, &state, &Session::failAiPlan);
        QObject::connect(&state, &Session::changed, &ai, [&] {
            if (state.aiState() != "planning") ai.cancel();
        });
    }
    QQuickStyle::setStyle("Basic");
    qmlRegisterSingletonInstance("Zephyr", 1, 0, "Session", &state);
    qmlRegisterType<Waveform>("Zephyr", 1, 0, "Waveform");
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, app.get(), [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlEngine::exit, app.get(), [](int code) { QCoreApplication::exit(code); });
    const QString qmlPath = QStringLiteral(ZEPHYR_SOURCE_DIR "/qml/") +
        (parser.isSet("test-engine-ui") ? "EngineHarness.qml" : parser.isSet("test-ui") ? "TestHarness.qml" : "Main.qml");
    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) return 1;
    QObject *root = engine.rootObjects().front();
    if (parser.isSet("test-engine-ui")) {
        QTimer::singleShot(0, app.get(), [root] {
            if (!QMetaObject::invokeMethod(root, "runTests")) QCoreApplication::exit(1);
        });
        return app->exec();
    }
    if (parser.isSet("test-ui")) {
        QTimer::singleShot(0, app.get(), [root] {
            QVariant result;
            const bool invoked = QMetaObject::invokeMethod(root, "runTests", Q_RETURN_ARG(QVariant, result));
            if (!invoked) qCritical() << "TestHarness.qml must expose runTests().";
            QCoreApplication::exit(invoked && result.toBool() ? 0 : 1);
        });
        return app->exec();
    }
    auto *window = qobject_cast<QQuickWindow *>(root);
    if (!window) { qCritical() << "Main.qml must create a QQuickWindow."; return 1; }
    window->setTitle(QString::fromUtf8("Zephyr · ") + state.project().value("name").toString());
    window->resize(width, height); window->show();
    if (!output.isEmpty()) QTimer::singleShot(300, app.get(), [window, output] {
        const QImage image = window->grabWindow();
        if (image.isNull() || !image.save(output, "PNG")) { qCritical() << "Screenshot failed:" << output; QCoreApplication::exit(1); }
        else { qInfo() << "Saved" << output; QCoreApplication::exit(0); }
    });
    return app->exec();
}
