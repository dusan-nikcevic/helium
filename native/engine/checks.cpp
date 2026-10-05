#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "temporal/tempo.h"
#include "ardourbridge.h"
#include "checks.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QProcess>
#include <cstdio>
#include <cmath>
#include <limits>
#include <iostream>

bool runEngineLoopChecks();
void runEngineCommandChecks(ArdourBridge &, const QString &, int &, int &);
void runEngineCommandFailureChecks(ArdourBridge &, const QString &, int &, int &);
namespace {
void pump(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
QVariantMap channel(const ArdourBridge &bridge, const QString &id) {
    for (const auto &entry : bridge.channels()) if (entry.toMap().value("id") == id) return entry.toMap();
    return {};
}
}
int runEngineChecks(const QString &sessionDirectory, const QString &sessionName) {
    std::cout.rdbuf(std::cerr.rdbuf());
    QTemporaryDir workspace;
    if (!workspace.isValid()) { std::fprintf(stderr, "ENGINE FAIL: temporary directory\n"); return 1; }
    const auto oldConfig = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", (workspace.path() + "/config").toUtf8());
    int checks = 0, failures = 0;
    auto check = [&](bool value, const char *label) {
        ++checks;
        if (!value) { std::fprintf(stderr, "ENGINE FAIL: %s\n", label); ++failures; }
        return value;
    };
    {
        ArdourBridge bridge;
        if (!sessionDirectory.isEmpty()) {
            if (check(bridge.openSession(sessionDirectory, sessionName), "cold process opens saved session")) {
                pump(180);
                check(bridge.sampleRate() > 0 && !bridge.channels().isEmpty(), "cold process reads real engine routes");
                bool validIds = true;
                for (const auto &entry : bridge.channels()) validIds &= !entry.toMap().value("id").toString().isEmpty();
                check(validIds, "cold process reads route ids");
            } else std::fprintf(stderr, "Ardour error: %s\n", bridge.error().toUtf8().constData());
        } else {
        const QString directory = workspace.path() + "/session";
        if (check(bridge.createSession(directory, "Checks"), "create session")) {
            pump(120);
            check(bridge.sampleRate() == 48000 && bridge.channels().size() == 1, "real master and sample rate");
            check(bridge.addAudioTrack("Audio", 1), "create real mono track");
            pump(120);
            QString id;
            for (const auto &entry : bridge.channels()) if (entry.toMap().value("isTrack").toBool()) id = entry.toMap().value("id").toString();
            check(!id.isEmpty(), "real track id");
            check(bridge.setGainDb(id, -6) && bridge.setMuted(id, true) && bridge.setSoloed(id, true), "mixer commands");
            pump(180);
            auto values = channel(bridge, id);
            check(std::abs(values.value("gainDb").toDouble() + 6) < 0.001 && values.value("muted").toBool() && values.value("soloed").toBool(), "mixer readback");
            check(bridge.setPan(id, 0.25), "mono pan command");
            pump(90);
            check(std::abs(channel(bridge, id).value("pan").toDouble() - 0.25) < 0.001, "mono pan readback");
            {
                const auto actualRoute = ARDOUR::AudioEngine::instance()->session()->route_by_id(PBD::ID(id.toStdString()));
                actualRoute->gain_control()->set_automation_state(ARDOUR::Play);
                check(!bridge.setGainDb(id, -3), "automation playback rejects manual gain");
                actualRoute->gain_control()->set_automation_state(ARDOUR::Off);
            }
            runEngineCommandChecks(bridge, id, checks, failures);
            pump(90);
            const auto before = channel(bridge, id);
            check(!bridge.setGainDb(id, std::numeric_limits<double>::quiet_NaN()) && !bridge.setGainDb(id, 100) && !bridge.setGainDb("missing", -3), "invalid gain rejection");
            check(!bridge.setPan(id, -1) && !bridge.seekSamples(-1) && !bridge.seekBar(0), "invalid transport and pan rejection");
            pump(90);
            check(channel(bridge, id).value("gainDb") == before.value("gainDb") && channel(bridge, id).value("pan") == before.value("pan"), "invalid commands preserve controls");
            check(!bridge.createSession(directory, "../unsafe") && !bridge.openSession(directory, "missing"), "invalid session paths");
            check(bridge.sessionDirectory() == directory, "invalid load preserves session");
            check(bridge.requestPlayback(true), "play request");
            pump(240);
            check(bridge.playing() && bridge.transportSamples() > 0, "engine transport advances");
            check(bridge.requestPlayback(false), "stop request");
            pump(240);
            const auto stopped = bridge.transportSamples();
            pump(120);
            check(!bridge.playing() && bridge.transportSamples() == stopped, "engine transport stops");
            check(bridge.seekSamples(96000), "sample seek request");
            pump(180);
            check(bridge.transportSamples() == 96000, "sample seek readback");
            check(bridge.seekBar(3.5), "tempo map seek request");
            pump(180);
            check(std::abs(bridge.barPosition() - 3.5) < 0.002, "tempo map seek readback");
            auto map = Temporal::TempoMap::write_copy();
            map->set_meter(Temporal::Meter(3, 8), Temporal::timepos_t(0));
            Temporal::TempoMap::update(map);
            check(bridge.seekBar(2.5), "3/8 tempo map seek request");
            pump(180);
            check(std::abs(bridge.barPosition() - 2.5) < 0.002 && bridge.timeSignature() == "3/8", "3/8 tempo map readback");
            check(bridge.saveSession(), "save session");
            check(QFileInfo::exists(directory + "/Checks.ardour"), "Ardour snapshot exists");
            QFile saved(directory + "/Checks.ardour"), invalid(directory + "/InvalidRate.ardour");
            if (check(saved.open(QIODevice::ReadOnly) && invalid.open(QIODevice::WriteOnly), "invalid-rate test snapshot opens")) {
                auto state = saved.readAll();
                state.replace("sample-rate=\"48000\"", "sample-rate=\"0\"");
                invalid.write(state);
                invalid.close();
                check(!bridge.openSession(directory, "InvalidRate") && bridge.sessionDirectory() == directory && !channel(bridge, id).isEmpty(), "invalid sample rate preserves open session");
            }
            QProcess coldOpen;
            coldOpen.setProgram(QCoreApplication::applicationFilePath());
            coldOpen.setArguments({"--engine-self-test", "--engine-session", directory, "--session-name", "Checks"});
            coldOpen.start();
            const bool started = coldOpen.waitForStarted(2000);
            check(started, "cold process starts");
            const bool finished = started && coldOpen.waitForFinished(8000);
            check(finished, "cold process finishes within eight seconds");
            if (!finished) { coldOpen.kill(); coldOpen.waitForFinished(2000); }
            const auto output = coldOpen.readAllStandardOutput() + coldOpen.readAllStandardError();
            check(finished && coldOpen.exitStatus() == QProcess::NormalExit && coldOpen.exitCode() == 0 && output.contains("ENGINE CHECKS PASS:"), "cold process verifies saved session");
            if (!finished || coldOpen.exitCode() != 0) std::fprintf(stderr, "%s", output.constData());
            check(!bridge.createSession(directory, "Checks"), "existing snapshot rejection");
            QVariantMap closingResult;
            const auto closingConnection = QObject::connect(&bridge, &ArdourBridge::commandFinished, &bridge, [&](const QVariantMap &result) { closingResult = result; });
            const QVariantMap closingTarget{{"kind", "route"}, {"id", id}};
            const QVariantMap closingOperation{{"action", "set_route_control"}, {"target", closingTarget}, {"control", "gainDb"}, {"expected", -6.}, {"value", -8.}};
            bridge.submitCommand(QVariantMap{{"schemaVersion", 1}, {"commandId", "close-pending"}, {"sessionId", bridge.engineSessionId()}, {"phase", "apply"}, {"source", "ui"}, {"expectedRevision", bridge.revision()}, {"selection", QVariantList{closingTarget}}, {"groupMode", "independent"}, {"operations", QVariantList{closingOperation}}});
            bridge.closeSession();
            check(!closingResult.isEmpty() && closingResult.value("stateValid").toBool(), "pending close returns a structured completion");
            QObject::disconnect(closingConnection);
            check(bridge.channels().isEmpty() && !bridge.playing(), "close session");
            check(bridge.openSession(directory, "Checks"), "reopen snapshot");
            pump(180);
            values = channel(bridge, id);
            check(!values.isEmpty() && std::abs(values.value("gainDb").toDouble() + 6) < 0.001 && values.value("muted").toBool(), "persisted id and mixer readback");
            check(runEngineLoopChecks(), "SessionEvent callback queue and raw-target rejection");
            runEngineCommandFailureChecks(bridge, id, checks, failures);
            // Leave requests queued so destruction exercises callback draining.
            bridge.setMuted(id, false);
            bridge.requestPlayback(true);
        } else std::fprintf(stderr, "Ardour error: %s\n", bridge.error().toUtf8().constData());
        }
    }
    if (oldConfig.isNull()) qunsetenv("XDG_CONFIG_HOME"); else qputenv("XDG_CONFIG_HOME", oldConfig);
    std::fprintf(stderr, "ENGINE CHECKS %s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
