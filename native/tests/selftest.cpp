#include "selftest.h"
#include "../aiplanner.h"
#include <QUuid>
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
    Session commands;
    check(commands.loadFixture(fixturePath), "structured command fixture loads");
    auto envelope = [&](const QString &phase, const QVariantList &operations = QVariantList{}) {
        QVariantMap request{{"schemaVersion", 1},
                            {"commandId", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                            {"sessionId", commands.sessionId()},
                            {"phase", phase},
                            {"source", "ai"},
                            {"groupMode", "independent"},
                            {"expectedRevision", QVariant::fromValue(commands.revision())},
                            {"selection", QVariantList{QVariantMap{{"kind", "route"}, {"id", "lead-vocal"}}}},
                            {"label", "Test batch"}};
        if (phase != "undo")
            request["operations"] = operations;
        return request;
    };
    commands.selectTrack("lead-vocal");
    const double gain = commands.selectedTrack().value("volumeDb").toDouble();
    const QVariantMap gainOp{{"action", "set_route_control"},
                             {"target", QVariantMap{{"kind", "route"}, {"id", "lead-vocal"}}},
                             {"control", "gainDb"},
                             {"expected", gain},
                             {"value", gain - 3}};
    const auto untouched = commands.project();
    const auto revisionBefore = commands.revision();
    auto malformed = gainOp;
    malformed["unexpected"] = true;
    check(!commands.submitStructuredCommand(envelope("apply", {gainOp, malformed})) &&
              commands.project() == untouched && commands.revision() == revisionBefore && !commands.canUndo(),
          "malformed second operation rejects whole batch without mutation");
    auto duplicateGain = gainOp;
    duplicateGain["expected"] = gain - 3;
    duplicateGain["value"] = gain - 6;
    check(!commands.submitStructuredCommand(envelope("apply", {gainOp, duplicateGain})) &&
              commands.project() == untouched,
          "duplicate target property writes reject atomically");
    auto preview = envelope("preview", {gainOp});
    check(commands.submitStructuredCommand(preview) &&
              commands.lastCommandResult().value("status") == "previewed" &&
              commands.project() == untouched && !commands.canUndo(),
          "structured preview exposes diff without parameter or undo mutation");
    check(commands.lastCommandResult().value("changes").toList().front().toMap().value("before") == gain,
          "structured diff uses authoritative before value");
    commands.selectTrack("piano");
    check(commands.applyAi(), "structured apply survives selection changes without redirecting target");
    check(commands.selectedTrack().value("volumeDb") ==
              untouched.value("tracks").toList()[4].toMap().value("volumeDb"),
          "selection change leaves selected piano gain untouched");
    commands.selectTrack("lead-vocal");
    check(commands.selectedTrack().value("volumeDb").toDouble() == gain - 3,
          "captured vocal target receives change");
    const auto appliedResult = commands.lastCommandResult();
    const auto appliedProject = commands.project();
    auto applyReceipt = preview;
    applyReceipt["phase"] = "apply";
    applyReceipt["commandId"] = appliedResult.value("commandId");
    check(commands.submitStructuredCommand(applyReceipt) && commands.project() == appliedProject &&
              commands.lastCommandResult() == appliedResult,
          "exact retry returns stored apply receipt");
    applyReceipt["label"] = "Different payload";
    check(!commands.submitStructuredCommand(applyReceipt) &&
              commands.lastCommandResult().value("error").toMap().value("code") == "IDEMPOTENCY_CONFLICT" &&
              commands.project() == appliedProject,
          "same command ID with changed payload rejects");
    auto undoRequest = envelope("undo");
    undoRequest["transactionId"] = appliedResult.value("transactionId");
    check(commands.submitStructuredCommand(undoRequest) && commands.project() == untouched &&
              commands.lastCommandResult().value("status") == "undone",
          "structured latest transaction undo restores whole project");
    check(!commands.mixHistory().isEmpty() && commands.mixHistory().first().toMap().value("undone").toBool(),
          "undone structured change remains inspectable");
    auto wrongRevision = envelope("apply", {gainOp});
    wrongRevision["expectedRevision"] = 0;
    check(!commands.submitStructuredCommand(wrongRevision) && commands.project() == untouched,
          "stale revision rejects atomically");
    auto unknown = gainOp;
    unknown["target"] = QVariantMap{{"kind", "route"}, {"id", "missing"}};
    check(!commands.submitStructuredCommand(envelope("apply", {gainOp, unknown})) &&
              commands.project() == untouched,
          "unknown target at end prevents first mutation");
    auto badOwner = QVariantMap{
        {"action", "set_processor_enabled"},
        {"target", QVariantMap{{"kind", "processor"}, {"id", "lead-vocal-channel-eq"}, {"routeId", "piano"}}},
        {"expected", true},
        {"value", false}};
    check(!commands.submitStructuredCommand(envelope("apply", {badOwner})) && commands.project() == untouched,
          "processor owner mismatch rejects");
    const auto physicalParam = commands.selectedTrack()
                                   .value("devices")
                                   .toList()[1]
                                   .toMap()
                                   .value("params")
                                   .toList()
                                   .first()
                                   .toMap();
    const double physicalBefore =
        physicalParam.value("min").toDouble() +
        physicalParam.value("value").toDouble() *
            (physicalParam.value("max").toDouble() - physicalParam.value("min").toDouble());
    QVariantMap paramOp{
        {"action", "set_parameter"},
        {"target",
         QVariantMap{{"kind", "processor"}, {"id", "lead-vocal-compressor"}, {"routeId", "lead-vocal"}}},
        {"parameterId", "thr"},
        {"unit", "Hz"},
        {"expected", physicalBefore},
        {"value", -25.5}};
    check(!commands.submitStructuredCommand(envelope("apply", {paramOp})) && commands.project() == untouched,
          "physical parameter unit mismatch rejects");
    paramOp["unit"] = "dB";
    check(commands.submitStructuredCommand(envelope("apply", {paramOp})) && commands.selectedTrack()
                                                                                    .value("devices")
                                                                                    .toList()[1]
                                                                                    .toMap()
                                                                                    .value("params")
                                                                                    .toList()
                                                                                    .first()
                                                                                    .toMap()
                                                                                    .value("value")
                                                                                    .toDouble() == .5,
          "structured parameter converts physical dB to normalized fixture value");
    check(commands.undo(), "structured physical parameter undo");
    check(commands.submitAi("lower gain by 3 dB") && commands.aiProvider() == "Local commands",
          "generic local gain request stages with provider label");
    check(commands.setTrackValue("lead-vocal", "volumeDb", gain - 1),
          "manual edit advances revision after generic preview");
    const auto afterManual = commands.project();
    check(!commands.applyAi() && commands.project() == afterManual,
          "manual edit invalidates staged generic preview");
    commands.undo();
    commands.discardAi();
    commands.selectClip("piano", "piano-clip-1");
    const auto notesBefore = commands.selectedClip().value("notes").toList();
    const auto beforeTranspose = commands.project();
    check(commands.submitAi("transpose down 2 semitones") && commands.project() == beforeTranspose,
          "natural transposition previews stored notes without mutation");
    check(commands.applyAi() &&
              commands.selectedClip().value("notes").toList().first().toMap().value("pitch").toInt() ==
                  notesBefore.first().toMap().value("pitch").toInt() - 2,
          "transposition applies note pitches");
    const auto changedNote = commands.selectedClip().value("notes").toList().first().toMap();
    auto preservedNote = changedNote;
    preservedNote["pitch"] = notesBefore.first().toMap().value("pitch");
    check(preservedNote == notesBefore.first().toMap(), "transposition preserves note metadata");
    check(!commands.mixHistory().first().toMap().value("changes").toList().isEmpty() &&
              commands.mixHistory().first().toMap().value("source") == "ai",
          "transposition records inspectable note changes in AI history");
    check(commands.undo() && commands.project() == beforeTranspose,
          "transposition undo restores note vector");
    commands.selectTrack("lead-vocal");
    const auto beforeRoute = commands.project();
    check(commands.submitAi("route to Drum Bus") && commands.project() == beforeRoute,
          "natural routing previews without mutation");
    check(commands.applyAi() && commands.selectedTrack().value("outputRouteId") == "drum-bus",
          "routing applies persistent destination ID");
    check(commands.undo() && commands.project() == beforeRoute, "routing undo restores destination");
    check(commands.captureMixSnapshot("Original mix"), "named mix snapshot saves");
    const QString snapshotId = commands.mixSnapshots().first().toMap().value("id").toString();
    commands.selectClip("piano", "piano-clip-1");
    commands.moveClip("piano", "piano-clip-1", 12);
    const auto clipsAfterMove = commands.selectedTrack().value("clips");
    const QString selectedId = commands.selectedClipId();
    commands.selectTrack("lead-vocal");
    commands.submitAi("route to Drum Bus");
    commands.applyAi();
    commands.selectClip("piano", "piano-clip-1");
    commands.setTrackValue("piano", "volumeDb", -20);
    commands.setDeviceParam("lead-vocal", "lead-vocal-compressor", "thr", .5);
    check(commands.restoreMixSnapshot(snapshotId) &&
              commands.selectedTrack().value("clips") == clipsAfterMove &&
              commands.selectedClipId() == selectedId,
          "mix snapshot restore preserves clips and selection");
    commands.selectTrack("lead-vocal");
    check(MixHistory::capture(commands.project()).value("lead-vocal").toMap().value("devices") ==
              MixHistory::capture(beforeRoute).value("lead-vocal").toMap().value("devices"),
          "mix snapshot restores device parameters and displays");
    check(commands.selectedResolvedContext().value("selectedRoute").toMap().value("outputRouteId") ==
              "vocal-bus",
          "mix snapshot restores canonical output route identity");
    check(commands.undo() && commands.selectedTrack()
                                     .value("devices")
                                     .toList()[1]
                                     .toMap()
                                     .value("params")
                                     .toList()
                                     .first()
                                     .toMap()
                                     .value("value")
                                     .toDouble() == .5,
          "snapshot restore creates one reversible transaction");
    check(commands.removeMixSnapshot(snapshotId) && commands.mixSnapshots().isEmpty(),
          "mix snapshot removal updates list");
    commands.setExternalPlanner(true);
    commands.selectTrack("lead-vocal");
    QVariantMap captured;
    QString plannedText;
    QObject::connect(&commands, &Session::aiPlanningRequested,
                     [&](const QString &text, const QVariantMap &context) {
                         plannedText = text;
                         captured = context;
                     });
    const auto externalBefore = commands.project();
    check(commands.submitAi("mute") && commands.commandBusy() && commands.aiProvider() == "OpenRouter" &&
              captured.value("selection").toList().size() == 1 && commands.project() == externalBefore,
          "OpenRouter request captures selection without mutation");
    const auto externalPlan = planAiCommand(plannedText, captured).value("request").toMap();
    commands.selectTrack("piano");
    check(!commands.acceptAiPlan(plannedText, externalPlan) && !commands.commandBusy() &&
              commands.project() == externalBefore,
          "late remote plan rejects changed selection");
    commands.selectTrack("lead-vocal");
    commands.submitAi("mute");
    auto remote = planAiCommand(plannedText, captured).value("request").toMap();
    auto remoteOperations = remote.value("operations").toList();
    auto corrupt = remoteOperations.first().toMap();
    corrupt["extra"] = true;
    remoteOperations.append(corrupt);
    remote["operations"] = remoteOperations;
    check(!commands.acceptAiPlan(plannedText, remote) && commands.project() == externalBefore,
          "malformed remote operation batch rejects before mutation");
    commands.submitAi("mute");
    auto unsafeRemote = planAiCommand(plannedText, captured).value("request").toMap();
    unsafeRemote["phase"] = "apply";
    check(!commands.acceptAiPlan(plannedText, unsafeRemote) && commands.project() == externalBefore,
          "remote response cannot apply without explicit user review");
    commands.submitAi("mute");
    unsafeRemote = planAiCommand(plannedText, captured).value("request").toMap();
    unsafeRemote["source"] = "ui";
    check(!commands.acceptAiPlan(plannedText, unsafeRemote) && commands.project() == externalBefore,
          "remote response cannot claim manual source");
    commands.submitAi("mute");
    unsafeRemote = planAiCommand(plannedText, captured).value("request").toMap();
    unsafeRemote["sessionId"] = "other-session";
    check(!commands.acceptAiPlan(plannedText, unsafeRemote) && commands.project() == externalBefore,
          "remote response cannot name another session");
    commands.setExternalPlanner(false);
    commands.selectClip("piano", "piano-clip-1");
    const auto beforePhysicalBatch = commands.project();
    const auto regionTarget =
        commands.selectedResolvedContext().value("selectedRegion").toMap().value("target").toMap();
    const auto moveOp = QVariantMap{
        {"action", "move_region"},
        {"target", regionTarget},
        {"expected", QVariantMap{{"domain", "beats"},
                                 {"value", commands.selectedClip().value("startBar").toDouble() * 4}}},
        {"value", QVariantMap{{"domain", "beats"}, {"value", 48.0}}}};
    commands.selectTrack("vocal-bus");
    const auto send = commands.selectedResolvedContext().value("sends").toList().first().toMap();
    const auto sendOp = QVariantMap{{"action", "set_send_gain"},
                                    {"target", send.value("target")},
                                    {"expected", send.value("gainDb")},
                                    {"value", -6.0}};
    commands.selectTrack("lead-vocal");
    const auto processorOp = QVariantMap{
        {"action", "set_processor_enabled"},
        {"target",
         QVariantMap{{"kind", "processor"}, {"id", "lead-vocal-channel-eq"}, {"routeId", "lead-vocal"}}},
        {"expected", true},
        {"value", false}};
    check(commands.submitStructuredCommand(envelope("apply", {moveOp, sendOp, processorOp})),
          "region send and processor edits apply as one physical batch");
    check(!commands.selectedTrack().value("devices").toList().first().toMap().value("enabled").toBool() &&
              !commands.selectedTrack().value("inserts").toList().first().toMap().value("enabled").toBool(),
          "structured processor enable updates its insert mirror");
    commands.selectClip("piano", "piano-clip-1");
    check(commands.selectedClip().value("startBar") == 12 &&
              commands.lastCommandResult().value("changes").toList().size() == 3,
          "structured beat position converts into arrangement bars");
    check(commands.undo() && commands.project() == beforePhysicalBatch,
          "one undo restores region send and processor batch");

    Session captureSession;
    captureSession.loadFixture(fixturePath);
    QString emptyScene, emptyTrack;
    for (const auto &rawScene : captureSession.project().value("launcherScenes").toList()) {
        const auto scene = rawScene.toMap();
        for (const auto &rawSlot : scene.value("slots").toList()) {
            const auto slot = rawSlot.toMap();
            if (slot.value("clipId").toString().isEmpty()) {
                emptyScene = scene.value("id").toString();
                emptyTrack = slot.value("trackId").toString();
                break;
            }
        }
        if (!emptyScene.isEmpty())
            break;
    }
    check(!emptyScene.isEmpty() && captureSession.createLauncherMidiClip(emptyScene, emptyTrack),
          "empty launcher slot creates a MIDI source");
    const QString newSourceId = captureSession.selectedClipId();
    const auto beforeBadNote = captureSession.project();
    check(!captureSession.addMidiNote({{"bar", 0}, {"beat", 0}, {"pitch", 128}, {"length", 1}}) &&
              captureSession.project() == beforeBadNote,
          "invalid MIDI note rejects atomically");
    check(captureSession.addMidiNote(
              {{"bar", 0}, {"beat", 0}, {"pitch", 60}, {"length", 1}, {"velocity", 100}}),
          "MIDI note adds to launcher source");
    const auto sourceAfterNote = captureSession.selectedClip();
    check(captureSession.placeLauncherClip(emptyScene, emptyTrack, 17) &&
              captureSession.selectionOrigin() == "arrangement" &&
              captureSession.selectedClip().value("sourceId") == newSourceId &&
              captureSession.selectedClipId() != newSourceId,
          "launcher source creates independent arrangement placement");
    check(captureSession.selectedClip().value("notes") == sourceAfterNote.value("notes"),
          "arrangement placement carries captured MIDI notes");
    captureSession.moveClip(emptyTrack, captureSession.selectedClipId(), 18);
    check(captureSession.selectLauncherSlot(emptyScene, emptyTrack) &&
              captureSession.selectedClip() == sourceAfterNote,
          "arrangement move preserves independent launcher source");
    check(captureSession.submitAi("transpose up 2 semitones") && captureSession.applyAi() &&
              captureSession.selectedClip().value("notes").toList().first().toMap().value("pitch") == 62,
          "launcher source supports validated natural transposition");
    check(captureSession.undo() && captureSession.selectedClip() == sourceAfterNote,
          "launcher source transposition undo restores source");
    Session receipts;
    receipts.loadFixture(fixturePath); receipts.selectTrack("lead-vocal");
    auto sameIdPreview = planAiCommand("lower gain by 3 dB", receipts.selectedResolvedContext()).value("request").toMap();
    check(receipts.submitStructuredCommand(sameIdPreview), "receipt regression previews a command");
    auto sameIdApply = sameIdPreview; sameIdApply["phase"] = "apply";
    check(receipts.submitStructuredCommand(sameIdApply), "preview and apply may share one command ID");
    const auto receiptProject = receipts.project(), receiptResult = receipts.lastCommandResult();
    check(!receipts.submitStructuredCommand(sameIdPreview) && receipts.lastCommandResult().value("error").toMap().value("code") == "CONFLICT",
          "preview retry revalidates current revision instead of replaying a cached diff");
    check(receipts.submitStructuredCommand(sameIdApply) && receipts.project() == receiptProject && receipts.lastCommandResult() == receiptResult,
          "phase-specific apply receipt remains idempotent after preview rejection");
    receipts.undo(); receipts.selectTrack("fx-bus");
    const auto routingBefore = receipts.project();
    check(!receipts.submitAi("route to Vocal Bus") && receipts.project() == routingBefore && receipts.lastCommandResult().value("error").toMap().value("code") == "CONFLICT",
          "routing detects feedback through a send and return");
    receipts.selectTrack("drums");
    check(!receipts.submitAi("route to Music Bus") && receipts.project() == routingBefore && receipts.lastCommandResult().value("error").toMap().value("code") == "UNSUPPORTED_OPERATION",
          "routing refuses an unresolved send destination instead of guessing");
    receipts.selectTrack("lead-vocal");
    check(receipts.submitAi("route to Drum Bus") && receipts.stagedAiPlan().first().toMap().value("property") == "outputRoute" && receipts.stagedAiPlan().first().toMap().value("afterDisplay") == "Drum Bus",
          "routing diff uses declared property and authoritative destination name");
    qInfo().noquote() << QString("Session self-test: %1 checks, %2 failures").arg(checks).arg(failures);
    return failures ? 1 : 0;
}
