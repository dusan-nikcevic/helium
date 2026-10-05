#include "mixhistorycheck.h"
#include "../mixhistory.h"
#include <QDebug>
#include <QDateTime>
#include <cstdio>

int runMixHistoryChecks() {
    int failures = 0, checks = 0;
    auto check = [&](bool condition, const char *message) {
        ++checks;
        if (!condition) { ++failures; qCritical() << "MixHistory FAIL:" << message; }
    };
    const QVariantMap device{{"id", "eq"}, {"name", "EQ"}, {"enabled", true}, {"insertId", "eq-slot"},
        {"params", QVariantList{QVariantMap{{"id", "freq"}, {"label", "Frequency"}, {"value", .2}, {"display", "200 Hz"}}}}};
    const QVariantMap track{{"id", "vocal"}, {"name", "Vocal"}, {"volumeDb", -6}, {"pan", 0},
        {"muted", false}, {"soloed", false}, {"output", "Vocal Bus"}, {"busId", "vocal-bus"}, {"outputRouteId", "vocal-bus"}, {"routeLabel", "Vocal Bus"}, {"meterL", .5},
        {"devices", QVariantList{device}}, {"inserts", QVariantList{QVariantMap{{"id", "eq-slot"}, {"deviceId", "eq"}, {"enabled", true}}}},
        {"sends", QVariantList{QVariantMap{{"id", "plate"}, {"amount", 10}, {"targetId", "return"}}}},
        {"clips", QVariantList{QVariantMap{{"id", "clip"}, {"startBar", 1}}}}};
    const QVariantMap original{{"tracks", QVariantList{track}}, {"selectedTrackId", "vocal"},
        {"master", QVariantMap{{"id", "master"}, {"name", "Master"}, {"volumeDb", -1}}},
        {"aiHistory", QVariantList{QVariantMap{{"text", "Reference row"}}}}};
    const auto state = MixHistory::capture(original);
    check(state.size() == 2 && !state.value("vocal").toMap().contains("clips") &&
          !state.value("vocal").toMap().contains("meterL") && !state.contains("selectedTrackId"), "capture excludes arrangement, meters and selection");
    MixHistory history;
    QString error;
    check(history.entries().isEmpty(), "reference rows do not become real history");
    check(history.saveSnapshot(original, " ", &error).isEmpty() && !error.isEmpty(), "empty snapshot name rejected");
    check(history.saveSnapshot(original, QString(81, 'x'), &error).isEmpty(), "long snapshot name rejected");
    check(history.saveSnapshot({}, "Empty", &error).isEmpty(), "empty mixer rejected");
    const QString snapshotId = history.saveSnapshot(original, "  Before AI  ", &error);
    check(!snapshotId.isEmpty() && error.isEmpty() && history.snapshots().first().toMap().value("name") == "Before AI", "snapshot named and assigned ID");
    check(QDateTime::fromString(history.snapshots().first().toMap().value("timestamp").toString(), Qt::ISODateWithMs).isValid(), "snapshot timestamp parseable");
    check(!history.snapshots().first().toMap().contains("state") && history.snapshotState(snapshotId) == state, "QML metadata excludes saved state");
    auto editedTrack = track;
    editedTrack["volumeDb"] = -3;
    auto edited = original;
    edited["tracks"] = QVariantList{editedTrack};
    const QString manualId = history.record(original, edited);
    check(!manualId.isEmpty() && history.entries().size() == 1 && history.entries().first().toMap().value("changes").toList().size() == 1, "manual gain recorded once");
    check(history.record(edited, edited).isEmpty() && history.entries().size() == 1, "no-op creates no history");
    auto finalTrack = editedTrack;
    finalTrack["volumeDb"] = -2;
    auto final = edited;
    final["tracks"] = QVariantList{finalTrack};
    check(history.record(edited, final, "manual", {}, true) == manualId && history.entries().size() == 1, "gesture merges into one transaction");
    const auto gainChange = history.entries().first().toMap().value("changes").toList().first().toMap();
    check(gainChange.value("before") == -6 && gainChange.value("after") == -2, "gesture keeps original before and final after");
    auto changedDevice = device;
    changedDevice["enabled"] = false;
    changedDevice["aiEdited"] = true;
    changedDevice["params"] = QVariantList{QVariantMap{{"id", "freq"}, {"label", "Frequency"}, {"value", .8}, {"display", "800 Hz"}, {"aiEdited", true}}};
    auto aiTrack = finalTrack;
    aiTrack["devices"] = QVariantList{changedDevice};
    aiTrack["inserts"] = QVariantList{QVariantMap{{"id", "eq-slot"}, {"deviceId", "eq"}, {"enabled", false}}};
    aiTrack["sends"] = QVariantList{QVariantMap{{"id", "plate"}, {"amount", 45}, {"targetId", "return"}}};
    aiTrack["pan"] = .25;
    aiTrack["muted"] = true;
    aiTrack["soloed"] = true;
    aiTrack["output"] = "Master";
    aiTrack["busId"] = "master";
    aiTrack["outputRouteId"] = "master";
    aiTrack["routeLabel"] = "Master";
    auto ai = final;
    ai["tracks"] = QVariantList{aiTrack};
    const QString aiId = history.record(final, ai, "ai", "Balance the vocal");
    const auto aiEntry = history.entries().first().toMap();
    check(!aiId.isEmpty() && history.entries().size() == 2 && aiEntry.value("source") == "ai" &&
          aiEntry.value("changes").toList().size() == 10, "AI physical changes share one inspectable entry without duplicate insert switch");
    bool displaySaved = false;
    for (const auto &item : aiEntry.value("changes").toList()) {
        const auto change = item.toMap();
        if (change.value("kind") == "param") displaySaved = change.value("beforeDisplay") == "200 Hz" && change.value("afterDisplay") == "800 Hz";
    }
    check(displaySaved, "parameter history preserves units");
    check(history.markUndone(aiId) && history.entries().first().toMap().value("undone").toBool() && history.entries().size() == 2, "undo remains inspectable");
    check(!history.markUndone("missing"), "unknown history ID rejected");
    auto metadata = original;
    auto metadataTrack = track;
    metadataTrack["aiEdited"] = true;
    metadataTrack["meterL"] = .9;
    metadataTrack["name"] = "Renamed";
    metadata["tracks"] = QVariantList{metadataTrack};
    check(MixHistory::changes(original, metadata).isEmpty(), "meters, names and AI decoration are not physical edits");
    metadataTrack["clips"] = QVariantList{QVariantMap{{"id", "clip"}, {"startBar", 8}}};
    metadata["tracks"] = QVariantList{metadataTrack};
    const auto clipMove = MixHistory::changes(original, metadata);
    check(clipMove.size() == 1 && clipMove.first().toMap().value("kind") == "clip" && clipMove.first().toMap().value("before") == 1 &&
          clipMove.first().toMap().value("after") == 8, "clip position changes remain inspectable in history");
    aiTrack["clips"] = metadataTrack.value("clips");
    aiTrack["meterL"] = .9;
    aiTrack["name"] = "Renamed";
    ai["tracks"] = QVariantList{aiTrack};
    ai["selectedTrackId"] = "master";
    const auto preserved = ai;
    check(history.restoreSnapshot(snapshotId, ai, &error) && error.isEmpty(), "snapshot restore accepted");
    const auto restored = ai.value("tracks").toList().first().toMap();
    check(restored.value("volumeDb") == -6 && restored.value("pan") == 0 && !restored.value("muted").toBool() &&
          !restored.value("soloed").toBool() && restored.value("devices") == track.value("devices") &&
          restored.value("inserts") == track.value("inserts") && restored.value("sends") == track.value("sends") &&
          restored.value("output") == "Vocal Bus" && restored.value("busId") == "vocal-bus" && restored.value("outputRouteId") == "vocal-bus" &&
          restored.value("routeLabel") == "Vocal Bus", "snapshot restores channel, processor, send and routing state");
    check(restored.value("clips") == metadataTrack.value("clips") && restored.value("meterL") == .9 && restored.value("name") == "Renamed" &&
          ai.value("selectedTrackId") == "master", "restore preserves arrangement, meters, channel name and selection");
    const auto beforeInvalid = ai;
    check(!history.restoreSnapshot("missing", ai, &error) && ai == beforeInvalid, "invalid snapshot restores nothing");
    auto missing = preserved;
    missing["tracks"] = QVariantList{};
    const auto missingBefore = missing;
    check(!history.restoreSnapshot(snapshotId, missing, &error) && missing == missingBefore, "missing channel rejects restore atomically");
    const QString sameName = history.saveSnapshot(original, "Before AI");
    check(sameName != snapshotId && history.snapshots().size() == 2, "duplicate names keep distinct snapshot IDs");
    check(!history.removeSnapshot("missing") && history.removeSnapshot(snapshotId) && history.snapshotState(snapshotId).isEmpty(), "snapshot removal uses ID");
    auto topology = original;
    auto addedTrack = track;
    auto devices = addedTrack.value("devices").toList();
    devices.append(QVariantMap{{"id", "new"}, {"enabled", true}, {"params", QVariantList{}}});
    addedTrack["devices"] = devices;
    topology["tracks"] = QVariantList{addedTrack};
    check(MixHistory::changes(original, topology).size() == 1, "processor addition produces a physical change");
    check(history.restoreSnapshot(sameName, topology) && topology == original, "snapshot restores processor inventory");
    auto midiBefore = original;
    const QVariantMap source{{"id", "midi-source"}, {"trackId", "vocal"}, {"lengthBars", 4},
        {"notes", QVariantList{QVariantMap{{"pitch", 60}, {"bar", 0}, {"length", 1}}}}};
    midiBefore["clipSources"] = QVariantList{source};
    auto midiAfter = midiBefore;
    auto transposed = source;
    transposed["notes"] = QVariantList{QVariantMap{{"pitch", 72}, {"bar", 0}, {"length", 1}}};
    midiAfter["clipSources"] = QVariantList{transposed};
    const QString transposeId = history.record(midiBefore, midiAfter, "ai", "Transpose source");
    const auto noteChange = history.entries().first().toMap().value("changes").toList();
    check(!transposeId.isEmpty() && noteChange.size() == 1 && noteChange.first().toMap().value("kind") == "clipSource", "MIDI transpose creates one AI history entry");
    check(noteChange.first().toMap().value("beforeDisplay").toString().contains("60") &&
          noteChange.first().toMap().value("afterDisplay").toString().contains("72"), "MIDI history exposes before and after pitches");
    check(!MixHistory::capture(midiAfter).contains("clipSources") && MixHistory::capture(midiAfter) == state, "snapshot projection still excludes MIDI sources");
    check(history.restoreSnapshot(sameName, midiAfter) && midiAfter.value("clipSources") == QVariantList{transposed}, "snapshot restore preserves edited MIDI notes");
    auto withNewSource = midiBefore;
    withNewSource["clipSources"] = QVariantList{source, QVariantMap{{"id", "new-source"}, {"trackId", "vocal"}, {"notes", QVariantList{}}}};
    const auto sourceAdded = MixHistory::changes(midiBefore, withNewSource);
    check(sourceAdded.size() == 1 && sourceAdded.first().toMap().value("field") == "presence", "new captured source creates an audit change");
    auto launcherBefore = midiBefore;
    launcherBefore["launcherScenes"] = QVariantList{QVariantMap{{"id", "verse"}, {"slots", QVariantList{QVariantMap{{"trackId", "vocal"}, {"clipId", "midi-source"}}}}}};
    auto launcherAfter = launcherBefore;
    launcherAfter["launcherScenes"] = QVariantList{QVariantMap{{"id", "verse"}, {"slots", QVariantList{QVariantMap{{"trackId", "vocal"}, {"clipId", "new-source"}}}}}};
    const auto slotChange = MixHistory::changes(launcherBefore, launcherAfter);
    check(slotChange.size() == 1 && slotChange.first().toMap().value("kind") == "launcher" &&
          slotChange.first().toMap().value("before") == "midi-source" && slotChange.first().toMap().value("after") == "new-source", "launcher replacement exposes source IDs");
    history.clear();
    check(history.entries().isEmpty() && history.snapshots().isEmpty(), "new session clears history and snapshots");
    std::printf("MixHistory checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}

#ifdef MIX_HISTORY_CHECK_MAIN
int main() { return runMixHistoryChecks(); }
#endif
