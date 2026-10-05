#include "selftest.h"
#include "../session.h"
#include <QDebug>
#include <limits>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QJsonDocument>

int runSelfTests(const QString &fixturePath) {
    int failures = 0, checks = 0;
    auto check = [&](bool condition, const char *message) {
        ++checks;
        if (!condition) { ++failures; qCritical().noquote() << "FAIL:" << message; }
    };
    Session s;
    if (!s.loadFixture(fixturePath)) { qCritical().noquote() << s.notice(); return 1; }
    const auto original = s.project();
    check(s.selectedTrackId() == "lead-vocal", "initial vocal selection");
    int selectedCount = 0, rejectedCount = 0;
    QObject::connect(&s, &Session::clipSelected, [&] { ++selectedCount; });
    QObject::connect(&s, &Session::operationRejected, [&] { ++rejectedCount; });
    check(!s.selectClip("piano", "lead-vocal-clip-2"), "cross-track clip rejected");
    check(!s.setTrackValue("missing", "volumeDb", -3), "invalid channel rejected");
    check(!s.setTrackValue("lead-vocal", "name", "oops"), "unknown channel key rejected");
    check(!s.setTrackValue("lead-vocal", "pan", 2), "pan range rejected");
    check(!s.setTrackValue("lead-vocal", "volumeDb", std::numeric_limits<double>::quiet_NaN()), "NaN rejected");
    check(!s.setTrackValue("lead-vocal", "muted", "true"), "non-boolean switch rejected");
    check(!s.setDeviceParam("lead-vocal", "lead-vocal-channel-eq", "250hz", 1.1), "parameter range rejected");
    check(!s.setSendAmount("vocal-bus", "vocal-bus-send-plate", -1), "send range rejected");
    check(!s.seek(-1) && !s.seek(std::numeric_limits<double>::infinity()), "invalid seeks rejected");
    check(s.project() == original && !s.canUndo(), "invalid edits leave project and undo unchanged");
    check(rejectedCount >= 10, "invalid operations emit rejection signals");
    check(s.setTrackValue("lead-vocal", "volumeDb", -7), "fader accepted");
    check(s.selectedTrack().value("volumeDb").toDouble() == -7, "selected track follows mixer edit");
    check(s.undo() && s.project() == original, "fader undo restores project");
    s.beginGesture();
    check(s.setTrackValue("lead-vocal", "volumeDb", -4) &&
          s.setTrackValue("lead-vocal", "volumeDb", -5) &&
          s.setTrackValue("lead-vocal", "volumeDb", -8), "gesture accepts consecutive fader values");
    s.endGesture();
    check(s.undo() && s.project() == original && !s.canUndo(), "one undo restores the entire fader gesture");
    s.beginGesture();
    check(!s.setTrackValue("lead-vocal", "volumeDb", 20), "gesture rejects an invalid value");
    s.endGesture();
    check(!s.canUndo(), "invalid gesture does not add an undo snapshot");
    check(s.selectClip("piano", "piano-clip-1"), "arrangement clip selected");
    check(selectedCount == 1 && s.selectedTrackId() == "piano", "clip selection signal and track agree");
    const auto pianoStart = s.selectedClip().value("startBar");
    check(s.moveClip("piano", "piano-clip-1", 10.38), "clip move accepted");
    check(s.selectedClip().value("startBar").toDouble() == 10.5, "clip move snaps to quarter bar");
    check(s.undo() && s.selectedClip().value("startBar") == pianoStart, "clip undo restores position");
    check(s.selectTrack("drums") && s.selectedClip().isEmpty(), "track change clears stale clip");
    check(s.selectLauncherSlot("chorus", "lead-vocal"), "launcher slot selected");
    const QString launcherId = original.contains("clipSources") ? "lead-vocal-clip-2-source" : "lead-vocal-clip-2";
    check(s.selectionOrigin() == "launcher" && s.selectedClipId() == launcherId, "launcher shares clip selection");
    auto sourceFixture = original;
    QVariantList sources;
    auto sourceTracks = sourceFixture.value("tracks").toList();
    for (auto &rawTrack : sourceTracks) {
        auto track = rawTrack.toMap();
        auto clips = track.value("clips").toList();
        for (auto &rawClip : clips) {
            auto clip = rawClip.toMap();
            const QString sourceId = clip.value("id").toString() + "-source";
            auto source = clip;
            source["id"] = sourceId; source["trackId"] = track.value("id");
            source.remove("startBar"); source.remove("sourceId");
            sources.append(source);
            clip["sourceId"] = sourceId; rawClip = clip;
        }
        track["clips"] = clips; rawTrack = track;
    }
    sourceFixture["tracks"] = sourceTracks; sourceFixture["clipSources"] = sources;
    auto sourceScenes = sourceFixture.value("launcherScenes").toList();
    for (auto &rawScene : sourceScenes) {
        auto scene = rawScene.toMap();
        auto sceneSlots = scene.value("slots").toList();
        for (auto &rawSlot : sceneSlots) {
            auto slot = rawSlot.toMap();
            const QString clipId = slot.value("clipId").toString();
            if (!clipId.isEmpty() && !clipId.endsWith("-source")) slot["clipId"] = clipId + "-source";
            rawSlot = slot;
        }
        scene["slots"] = sceneSlots; rawScene = scene;
    }
    sourceScenes.append(QVariantMap{{"id", "wrong-owner"}, {"slots", QVariantList{QVariantMap{
        {"trackId", "piano"}, {"clipId", "lead-vocal-clip-2-source"}}}}});
    sourceScenes.append(QVariantMap{{"id", "missing-source"}, {"slots", QVariantList{QVariantMap{
        {"trackId", "lead-vocal"}, {"clipId", "missing-source"}}}}});
    sourceFixture["launcherScenes"] = sourceScenes;
    QTemporaryFile sourceFile;
    check(sourceFile.open(), "clip source fixture file opens");
    sourceFile.write(QJsonDocument::fromVariant(sourceFixture).toJson()); sourceFile.flush();
    Session sourceSession;
    check(sourceSession.loadFixture(sourceFile.fileName()), "clip source fixture loads");
    QString signalOrigin;
    QObject::connect(&sourceSession, &Session::clipSelected, [&] { signalOrigin = sourceSession.selectionOrigin(); });
    check(sourceSession.selectLauncherSlot("chorus", "lead-vocal"), "launcher selects owned independent source");
    const auto selectedSource = sourceSession.selectedClip();
    check(selectedSource.value("id") == "lead-vocal-clip-2-source" && !selectedSource.contains("startBar"), "launcher selection exposes source metadata without placement");
    check(signalOrigin == "launcher", "clip signal observes launcher selection origin");
    const auto sourcesBeforeMove = sourceSession.project().value("clipSources");
    check(sourceSession.moveClip("lead-vocal", "lead-vocal-clip-2", 18.25), "arrangement placement moves while source remains selected");
    check(sourceSession.project().value("tracks").toList()[2].toMap().value("clips").toList()[2].toMap().value("startBar") == 18.25,
          "arrangement move changes placement");
    check(sourceSession.project().value("clipSources") == sourcesBeforeMove && sourceSession.selectedClip() == selectedSource,
          "arrangement move preserves source metadata and launcher selection");
    check(sourceSession.undo() && sourceSession.selectedClip() == selectedSource, "arrangement undo preserves launcher source selection");
    check(!sourceSession.selectLauncherSlot("wrong-owner", "piano") && sourceSession.selectedClip() == selectedSource,
          "launcher rejects another track source without changing selection");
    check(!sourceSession.selectLauncherSlot("missing-source", "lead-vocal"), "launcher rejects missing source");
    check(sourceSession.selectTrack("lead-vocal") && sourceSession.selectedClipId().isEmpty(), "channel selection clears launcher source placement mismatch");
    check(sourceSession.selectClip("lead-vocal", "lead-vocal-clip-2") && sourceSession.selectedClip().contains("startBar"),
          "arrangement selection still exposes placement metadata");
    check(s.launchScene("chorus") && s.activeSceneId() == "chorus", "scene launch records fixture scene");
    check(!s.launchScene("missing") && s.activeSceneId() == "chorus", "invalid scene preserves active scene");
    const auto beforeAi = s.project();
    const auto history = s.aiHistory();
    check(s.stagedAiPlan().size() == 4 && s.stagedAiPlan().first().toMap().contains("beforeDisplay"), "staged AI plan exposes actual before/after values");
    check(s.previewAi() && s.aiState() == "preview", "AI preview stages plan");
    check(s.project() == beforeAi && !s.canUndo(), "AI preview leaves parameters and undo unchanged");
    check(s.previewAi() && s.aiState() == "ready", "AI preview toggles off");
    check(s.previewAi() && s.applyAi(), "AI apply accepted");
    const auto applied = s.project();
    check(s.aiState() == "applied" && applied != beforeAi, "AI commits staged parameters");
    check(s.applyAi() && s.project() == applied, "AI apply idempotent");
    check(s.aiHistory().size() == history.size() + 1, "AI apply creates one history item");
    check(s.undo() && s.project() == beforeAi && s.aiHistory() == history, "AI undo restores project and history");
    check(!s.canUndo(), "AI apply uses one undo step");
    check(s.setDeviceParam("lead-vocal", "lead-vocal-channel-eq", "250hz", .8), "manual edit after staging accepted");
    const auto manual = s.project();
    check(!s.applyAi() && s.project() == manual, "stale AI plan rejected atomically");
    check(s.undo(), "manual edit undone");
    s.discardAi();
    check(s.stagedAiPlan().isEmpty(), "discard clears the exposed staged plan");
    check(!s.applyAi(), "discard prevents applying old plan");
    check(!s.submitAi("Humanize the hats"), "unsupported command rejected");
    check(s.selectTrack("piano") && !s.submitAi("Clean vocal chain"), "incompatible AI selection rejected");
    check(s.selectTrack("guitars") && s.submitAi("Make the chorus wider"), "width fixture command staged");
    check(s.project().value("diff").toMap().value("title") == "1 change", "width command shows its own diff");
    check(s.stagedAiPlan().size() == 1 && s.stagedAiPlan().first().toMap().value("afterDisplay") == "115%", "width plan exposes its physical after value");
    check(s.applyAi() && s.undo(), "width command applies and undoes");
    check(s.selectTrack("lead-vocal") && s.submitAi("Clean vocal chain"), "vocal fixture command staged");
    check(s.project().value("diff").toMap().value("title") == "4 changes", "vocal restores reference diff");
    for (const QString &id : {QString("drum-bus"), QString("plate-verb"), QString("master")}) {
        check(s.selectTrack(id) && s.setTrackValue(id, "volumeDb", -12), "bus/return/master shared edit path");
        check(s.selectedTrack().value("volumeDb").toDouble() == -12 && s.undo(), "bus/return/master undo path");
        check(!s.selectedTrack().value("devices").toList().isEmpty(), "every channel exposes devices");
    }
    check(s.selectTrack("master"), "master selected for synthesized processor test");
    const auto masterDevice = s.selectedTrack().value("devices").toList().first().toMap();
    check(s.setDeviceEnabled("master", masterDevice.value("id").toString(), false), "synthesized master device bypass accepted");
    check(!s.selectedTrack().value("inserts").toList().first().toMap().value("enabled").toBool(), "synthesized device mapping updates master insert");
    check(s.undo(), "synthesized master bypass undo");
    check(s.selectTrack("lead-vocal") && s.addDevice("lead-vocal", "eq") && s.undo(), "device addition and undo");
    check(!s.addDevice("lead-vocal", "unknown"), "unknown device kind rejected");
    const auto beforeControls = s.project();
    const auto firstInsert = s.selectedTrack().value("inserts").toList().front().toMap().value("id").toString();
    check(s.setDeviceEnabled("lead-vocal", "lead-vocal-channel-eq", false), "device bypass accepted");
    check(!s.selectedTrack().value("inserts").toList().first().toMap().value("enabled").toBool(), "device bypass updates its mapped insert");
    check(s.undo() && s.project() == beforeControls, "device bypass undo restores both views");
    check(s.setInsertEnabled("lead-vocal", firstInsert, false), "insert bypass accepted");
    check(!s.selectedTrack().value("devices").toList().first().toMap().value("enabled").toBool(), "insert bypass updates its mapped device");
    check(s.undo() && s.project() == beforeControls, "insert bypass undo restores both views");
    check(s.addDevice("lead-vocal", "eq"), "new mapped device added");
    const auto addedInsert = s.selectedTrack().value("inserts").toList().last().toMap();
    const auto addedDevice = s.selectedTrack().value("devices").toList().last().toMap();
    check(addedInsert.value("deviceId") == addedDevice.value("id"), "new device creates an explicit insert mapping");
    check(s.setInsertEnabled("lead-vocal", addedInsert.value("id").toString(), false), "new insert bypass accepted");
    check(!s.selectedTrack().value("devices").toList().last().toMap().value("enabled").toBool(), "new insert toggles only its added device");
    check(s.selectedTrack().value("devices").toList().first().toMap().value("enabled").toBool(), "duplicate processor names remain independent");
    check(s.undo() && s.undo() && s.project() == beforeControls, "new device and insert undo together");
    check(s.setDeviceParam("lead-vocal", "lead-vocal-compressor", "thr", .5), "scaled threshold edit accepted");
    check(s.selectedTrack().value("devices").toList()[1].toMap().value("params").toList().first().toMap().value("display") == "-25.5 dB", "manual threshold edit retains dB units");
    check(s.undo(), "scaled threshold edit undo");
    check(s.selectTrack("guitars") && s.setDeviceParam("guitars", "guitars-stereo-width", "width", .75), "scaled width edit accepted");
    check(s.selectedTrack().value("devices").toList().last().toMap().value("params").toList().first().toMap().value("display") == "125%", "manual width edit retains physical percent scale");
    check(s.undo() && s.selectTrack("lead-vocal"), "scaled width undo restores selection target");
    check(s.setSendAmount("vocal-bus", "vocal-bus-send-plate", 42) && s.undo(), "send amount and undo");
    check(s.project() == beforeControls, "control undo preserves other channel values");
    auto invalidFixture = s.project();
    auto invalidPlans = invalidFixture.value("aiPlans").toMap();
    auto invalidPlan = invalidPlans.value("vocal").toList();
    auto invalidChange = invalidPlan.last().toMap();
    invalidChange["after"] = 2.0;
    invalidPlan.last() = invalidChange;
    invalidPlans["vocal"] = invalidPlan;
    invalidFixture["aiPlans"] = invalidPlans;
    QTemporaryFile fixture;
    check(fixture.open(), "temporary fixture opens");
    fixture.write(QJsonDocument::fromVariant(invalidFixture).toJson()); fixture.flush();
    Session atomic;
    check(atomic.loadFixture(fixture.fileName()), "invalid plan fixture loads");
    const auto atomicBefore = atomic.project();
    check(!atomic.applyAi() && atomic.project() == atomicBefore && !atomic.canUndo(), "invalid final AI change prevents every mutation");
    auto unscaledFixture = s.project();
    auto unscaledTracks = unscaledFixture.value("tracks").toList();
    auto unscaledTrack = unscaledTracks[2].toMap();
    auto unscaledDevices = unscaledTrack.value("devices").toList();
    auto unscaledDevice = unscaledDevices[0].toMap();
    auto unscaledParams = unscaledDevice.value("params").toList();
    auto unscaledParam = unscaledParams[0].toMap();
    unscaledParam.remove("min"); unscaledParam.remove("max");
    unscaledParams[0] = unscaledParam; unscaledDevice["params"] = unscaledParams;
    unscaledDevices[0] = unscaledDevice; unscaledTrack["devices"] = unscaledDevices;
    unscaledTracks[2] = unscaledTrack; unscaledFixture["tracks"] = unscaledTracks;
    QTemporaryFile unscaledFile;
    check(unscaledFile.open(), "unscaled fixture file opens");
    unscaledFile.write(QJsonDocument::fromVariant(unscaledFixture).toJson()); unscaledFile.flush();
    Session unscaled;
    check(unscaled.loadFixture(unscaledFile.fileName()), "unscaled physical parameter fixture loads");
    const auto unscaledBefore = unscaled.project();
    check(!unscaled.setDeviceParam("lead-vocal", "lead-vocal-channel-eq", "250hz", .8) &&
          unscaled.project() == unscaledBefore, "unscaled physical edit rejects without losing units");
    s.togglePlayback(); check(s.playing(), "fixture transport starts");
    const double position = s.playheadBar();
    QEventLoop tick;
    QTimer::singleShot(80, &tick, &QEventLoop::quit); tick.exec();
    check(s.playheadBar() > position, "fixture timer advances playhead");
    s.stop(); check(!s.playing(), "fixture transport stops");
    const double stopped = s.playheadBar();
    check(s.playheadBar() == stopped, "stop preserves position");
    check(s.seek(22.999), "seek near loop end accepted");
    s.togglePlayback();
    QTimer::singleShot(80, &tick, &QEventLoop::quit); tick.exec();
    check(s.playheadBar() >= 9 && s.playheadBar() < 10, "visual transport loops within visible timeline");
    s.stop();
    qInfo().noquote() << QString("Session self-test: %1 checks, %2 failures").arg(checks).arg(failures);
    return failures ? 1 : 0;
}
