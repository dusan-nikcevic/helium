#include "session.h"
#ifdef ZEPHYR_ENGINE
#include "engine/ardourbridge.h"
#endif
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>
#include <cmath>

namespace {
int indexOf(const QVariantList &list, const QString &id) {
    for (int i = 0; i < list.size(); ++i)
        if (list[i].toMap().value("id").toString() == id) return i;
    return -1;
}
bool numberIn(const QVariant &value, double min, double max) {
    if (value.typeId() == QMetaType::QString || value.typeId() == QMetaType::Bool) return false;
    bool ok = false;
    const double n = value.toDouble(&ok);
    return ok && std::isfinite(n) && n >= min && n <= max;
}
QString displayValue(const QVariantMap &param, double value) {
    const QString unit = param.value("unit").toString();
    if (param.contains("min") && param.contains("max")) {
        const double physical = param.value("min").toDouble() + value *
            (param.value("max").toDouble() - param.value("min").toDouble());
        return QString::number(physical, 'f', param.value("decimals", 1).toInt()) +
            (unit.isEmpty() ? QString() : unit == "%" ? unit : " " + unit);
    }
    return QString::number(qRound(value * 100)) + "%";
}
}

Session::Session(QObject *parent) : QObject(parent) {
    m_timer.setInterval(30);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const double elapsed = m_clock.restart() / 1000.0;
        m_bar += elapsed * m_project.value("bpm", 140).toDouble() / 240.0;
        if (m_bar >= endBar()) {
            if (m_loop) {
                const double start = m_project.value("startBar", 9).toDouble();
                m_bar = start + std::fmod(m_bar - start, endBar() - start);
            } else { m_bar = endBar(); stop(); }
        }
        emit changed();
    });
}
#ifdef ZEPHYR_ENGINE
void Session::attachEngine(ArdourBridge *engine) {
    m_timer.stop(); m_undo.clear(); m_plan.clear(); m_history.clear();
    m_trackId.clear(); m_clipId.clear(); m_sceneId.clear();
    m_origin = "arrangement"; m_aiState = "ready"; m_loop = false;
    m_engine = engine; m_engineConnected = true;
    connect(engine, &ArdourBridge::changed, this, &Session::refreshEngine);
    connect(engine, &ArdourBridge::errorChanged, this, [this] {
        if (!m_engine->error().isEmpty()) reject(m_engine->error());
    });
    refreshEngine();
    notify("libardour Dummy backend. Mixer and transport are live; audio device output, clip/plugin edits, AI and undo are not connected.");
}
void Session::refreshEngine() {
    QVariantList tracks, buses, groups, trackIds;
    QVariantMap master{{"id", "master"}, {"name", "No master"}, {"color", "master"},
        {"engineId", ""}, {"volumeDb", 0.0}, {"pan", 0.0}, {"meterL", 0.0}, {"meterR", 0.0},
        {"devices", QVariantList{}}, {"inserts", QVariantList{}}, {"sends", QVariantList{}}};
    for (const auto &raw : m_engine->channels()) {
        const auto source = raw.toMap();
        const QString id = source.value("id").toString();
        const bool isMaster = source.value("isMaster").toBool();
        auto meter = [](const QVariant &value) {
            const double db = value.toDouble();
            return std::isfinite(db) ? qBound(0.0, (db + 60.0) / 60.0, 1.0) : 0.0;
        };
        QVariantMap channel{{"id", isMaster ? "master" : id}, {"engineId", id},
            {"name", source.value("name")}, {"volumeDb", source.value("gainDb")},
            {"pan", source.value("pan", .5).toDouble() * 2 - 1}, {"panAvailable", source.value("panAvailable", false)},
            {"muted", source.value("muted")}, {"soloed", source.value("soloed")}, {"recordArmed", false},
            {"color", isMaster ? "master" : "vocal"}, {"type", source.value("isTrack").toBool() ? "AUDIO" : "BUS"},
            {"meterL", meter(source.value("meterLDb", source.value("meterDb", -120)))},
            {"meterR", meter(source.value("meterRDb", source.value("meterDb", -120)))},
            {"output", "Engine routing"}, {"routeLabel", "Engine routing"},
            {"clips", QVariantList{}}, {"devices", QVariantList{}}, {"inserts", QVariantList{}}, {"sends", QVariantList{}}};
        if (isMaster) master = channel;
        else if (source.value("isTrack").toBool()) { tracks.append(channel); trackIds.append(id); }
        else {
            buses.append(channel);
            groups.append(QVariantMap{{"id", "group-" + id}, {"name", source.value("name")},
                {"color", "vocal"}, {"busId", id}, {"trackIds", QVariantList{}}});
        }
    }
    if (!trackIds.isEmpty()) groups.prepend(QVariantMap{{"id", "engine-tracks"}, {"name", "Tracks"},
        {"color", "vocal"}, {"busId", ""}, {"trackIds", trackIds}});
    const double rate = m_engine->sampleRate();
    QVariantMap project{{"name", QFileInfo(m_engine->sessionDirectory()).fileName()}, {"section", "Engine"},
        {"key", "--"}, {"bpm", m_engine->bpm()}, {"timeSig", m_engine->timeSignature()}, {"bbtLabel", m_engine->bbtLabel()},
        {"sampleRate", QString::number(rate / 1000.0) + " kHz"}, {"engine", "libardour Dummy"},
        {"totalBars", 16}, {"startBar", 1}, {"sections", QVariantList{}}, {"tracks", tracks},
        {"buses", buses}, {"returns", QVariantList{}}, {"groups", groups}, {"master", master},
        {"browserInstruments", QVariantList{}}, {"launcherScenes", QVariantList{}}, {"clipSources", QVariantList{}},
        {"suggestionChips", QVariantList{}}, {"chat", QVariantList{QVariantMap{{"text", "AI planning is not connected to the engine session."}}}},
        {"diff", QVariantMap{{"title", "No engine AI plan"}, {"section", "Unavailable"}, {"changes", QVariantList{}}}},
        {"cpuLoad", m_engine->cpuLoad()}, {"elapsedSeconds", rate > 0 ? m_engine->transportSamples() / rate : 0.0}};
    const bool selectionMissing = channel(project, m_trackId).isEmpty();
    const bool changed = m_project != project || m_playing != m_engine->playing() || m_bar != m_engine->barPosition();
    m_project = project; m_playing = m_engine->playing(); m_bar = m_engine->barPosition();
    if (selectionMissing) {
        m_trackId = !tracks.isEmpty() ? tracks.front().toMap().value("id").toString() : "master";
        emit selectionChanged();
    }
    if (changed) emit this->changed();
}
#endif
bool Session::loadFixture(const QString &path) {
    if (m_engineConnected) return reject("Close the engine session before loading a fixture.");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return reject("Cannot open fixture: " + path);
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return reject("Invalid fixture JSON: " + error.errorString());
    const auto project = doc.toVariant().toMap();
    if (project.value("tracks").toList().isEmpty() ||
        !numberIn(project.value("totalBars"), 1, 100000) ||
        !numberIn(project.value("startBar"), 0, 100000) ||
        !numberIn(project.value("bpm"), 1, 1000)) return reject("Fixture lacks valid tracks or transport values.");
    m_project = project;
    m_project["referenceDiff"] = project.value("diff");
    // Fixture rows have controls even when only insert names are supplied.
    for (const QString &listKey : {QString("tracks"), QString("buses"), QString("returns")}) {
        auto list = m_project.value(listKey).toList();
        for (auto &entry : list) {
            auto c = entry.toMap();
            if (!c.contains("devices")) {
                QVariantList devices;
                auto inserts = c.value("inserts").toList();
                for (auto &raw : inserts) {
                    auto insert = raw.toMap();
                    const QString name = insert.value("name").toString();
                    const QString kind = name.contains("EQ", Qt::CaseInsensitive) ? "eq" :
                        name.contains("Comp", Qt::CaseInsensitive) ? "dynamics" : "fx";
                    const QString id = insert.value("deviceId", "device-" + insert.value("id").toString()).toString();
                    insert["deviceId"] = id;
                    raw = insert;
                    devices.append(QVariantMap{{"id", id}, {"insertId", insert.value("id")}, {"name", name}, {"kind", kind},
                        {"enabled", insert.value("enabled", true)}, {"params", QVariantList{
                            QVariantMap{{"id", id + "-mix"}, {"label", "Mix"}, {"value", .5}, {"display", "50%"}}}}});
                }
                c["devices"] = devices;
                c["inserts"] = inserts;
            }
            entry = c;
        }
        m_project[listKey] = list;
    }
    auto master = m_project.value("master").toMap();
    master["id"] = "master";
    master["color"] = "master";
    if (!master.contains("pan")) master["pan"] = 0;
    if (master.value("devices").toList().isEmpty()) {
        QVariantList devices;
        auto inserts = master.value("inserts").toList();
        for (auto &raw : inserts) {
            auto insert = raw.toMap();
            const QString id = insert.value("deviceId", "device-" + insert.value("id").toString()).toString();
            insert["deviceId"] = id;
            raw = insert;
            devices.append(QVariantMap{{"id", id}, {"insertId", insert.value("id")}, {"name", insert.value("name")}, {"kind", "utility"},
                {"enabled", insert.value("enabled", true)}, {"params", QVariantList{QVariantMap{
                    {"id", id + "-mix"}, {"label", "Mix"}, {"value", .5}, {"display", "50%"}}}}});
        }
        master["devices"] = devices;
        master["inserts"] = inserts;
    }
    if (!master.contains("sends")) master["sends"] = QVariantList{};
    m_project["master"] = master;
    m_history.clear();
    for (const auto &entry : project.value("aiHistory").toList()) {
        auto item = entry.toMap();
        item["status"] = "reference";
        item["text"] = "Reference: " + item.value("text").toString();
        m_history.append(item);
    }
    m_project["aiHistory"] = m_history;
    m_undo.clear();
    endGesture();
    m_plan = project.value("aiPlan").toList();
    if (m_plan.isEmpty()) m_plan = project.value("aiPlans").toMap().value("default").toList();
    if (m_plan.isEmpty()) m_plan = project.value("aiPlans").toMap().value("vocal").toList();
    m_aiState = "ready";
    m_lastCommand.clear();
    m_trackId.clear(); m_clipId.clear(); m_sceneId.clear(); m_origin = "arrangement";
    const auto tracks = m_project.value("tracks").toList();
    m_trackId = tracks.front().toMap().value("id").toString();
    for (const auto &entry : tracks) {
        const auto track = entry.toMap();
        for (const auto &c : track.value("clips").toList()) {
            if (c.toMap().value("selected").toBool()) {
                m_trackId = track.value("id").toString();
                m_clipId = c.toMap().value("id").toString();
                break;
            }
        }
        if (!m_clipId.isEmpty()) break;
    }
    m_bar = qMin(17.0, endBar());
    notify("Fixture session. No audio engine or AI service is connected.");
    emit selectionChanged();
    return true;
}
QVariantMap Session::channel(const QVariantMap &project, const QString &id) {
    if (id.isEmpty()) return {};
    if (id == "master") return project.value("master").toMap();
    for (const QString &key : {QString("tracks"), QString("buses"), QString("returns")}) {
        const auto list = project.value(key).toList();
        const int index = indexOf(list, id);
        if (index >= 0) return list[index].toMap();
    }
    return {};
}
bool Session::replaceChannel(QVariantMap &project, const QString &id, const QVariantMap &c) {
    if (id == "master" && !project.value("master").toMap().isEmpty()) { project["master"] = c; return true; }
    for (const QString &key : {QString("tracks"), QString("buses"), QString("returns")}) {
        auto list = project.value(key).toList();
        const int index = indexOf(list, id);
        if (index >= 0) { list[index] = c; project[key] = list; return true; }
    }
    return false;
}
QVariantMap Session::selectedTrack() const { return channel(m_project, m_trackId); }
QVariantMap Session::selectedClip() const {
    if (m_origin == "launcher" && m_project.contains("clipSources")) {
        const auto sources = m_project.value("clipSources").toList();
        const int index = indexOf(sources, m_clipId);
        if (index < 0) return {};
        const auto source = sources[index].toMap();
        return source.value("trackId").toString() == m_trackId ? source : QVariantMap{};
    }
    const auto clips = selectedTrack().value("clips").toList();
    const int index = indexOf(clips, m_clipId);
    return index < 0 ? QVariantMap{} : clips[index].toMap();
}
bool Session::reject(const QString &message) {
    m_notice = message;
    emit operationRejected(message);
    emit changed();
    return false;
}
void Session::notify(const QString &message) {
    if (!message.isEmpty()) m_notice = message;
    emit changed();
}
Session::Snapshot Session::snapshot() const { return {m_project, m_history, m_plan, m_aiState, m_lastCommand}; }
void Session::commit(const QVariantMap &project) {
    if (project == m_project) return;
    // ponytail: snapshots retain 64 fixture edits; use engine commands for real sessions.
    if (!m_gestureActive || !m_gestureSaved) {
        if (m_undo.size() == 64) m_undo.removeFirst();
        m_undo.append(snapshot());
        if (m_gestureActive) m_gestureSaved = true;
    }
    m_project = project;
    notify("Fixture edit. No audio processing is connected.");
}
void Session::beginGesture() {
    if (m_gestureActive) return;
    m_gestureActive = true;
    m_gestureSaved = false;
}
void Session::endGesture() { m_gestureActive = false; m_gestureSaved = false; }
void Session::setRequestText(const QString &text) {
    if (text == m_request) return;
    m_request = text;
    emit changed();
}
void Session::setView(const QString &view) {
    if (!QStringList{"arrange", "mix", "split", "session"}.contains(view)) { reject("Unknown workspace view."); return; }
    m_view = view;
    emit changed();
}
bool Session::selectTrack(const QString &id) {
    if (channel(m_project, id).isEmpty()) return reject("Unknown channel: " + id);
    if (m_trackId != id || (m_origin == "launcher" && m_project.contains("clipSources"))) m_clipId.clear();
    m_trackId = id;
    m_origin = "arrangement";
    emit selectionChanged(); emit changed();
    return true;
}
bool Session::selectClip(const QString &id, const QString &clipId) {
    const auto clips = channel(m_project, id).value("clips").toList();
    if (indexOf(clips, clipId) < 0) return reject("Clip does not belong to this track.");
    m_trackId = id; m_clipId = clipId; m_origin = "arrangement";
    emit selectionChanged(); emit clipSelected(); emit changed();
    return true;
}
bool Session::edit(QVariantMap &project, const QString &id, const QString &kind,
                   const QString &itemId, const QString &paramId, const QVariant &value,
                   const QString &display, bool ai) {
    auto c = channel(project, id);
    if (c.isEmpty()) return reject("Unknown channel: " + id);
    if (kind == "track") {
        if (QStringList{"muted", "soloed", "recordArmed"}.contains(itemId)) {
            if (value.typeId() != QMetaType::Bool) return reject("Channel switch requires a boolean.");
        } else if (itemId == "volumeDb") {
            if (!numberIn(value, -60, 6)) return reject("Volume must be between -60 and +6 dB.");
        } else if (itemId == "pan") {
            if (!numberIn(value, -1, 1)) return reject("Pan must be between -1 and 1.");
        } else return reject("Unknown editable channel key: " + itemId);
        c[itemId] = value;
    } else {
        const QString listKey = kind == "param" || kind == "device" ? "devices" :
            kind == "insert" ? "inserts" : kind == "send" ? "sends" : kind == "clip" ? "clips" : QString();
        if (listKey.isEmpty()) return reject("Unknown edit kind.");
        auto list = c.value(listKey).toList();
        const int index = indexOf(list, itemId);
        if (index < 0) return reject("Unknown " + kind + " id: " + itemId);
        auto item = list[index].toMap();
        if (kind == "param") {
            if (!numberIn(value, 0, 1)) return reject("Device parameter must be between 0 and 1.");
            auto params = item.value("params").toList();
            const int pi = indexOf(params, paramId);
            if (pi < 0) return reject("Unknown device parameter: " + paramId);
            auto param = params[pi].toMap();
            const bool scaled = param.contains("min") && param.contains("max");
            if (scaled && (!numberIn(param.value("min"), -1e12, 1e12) ||
                !numberIn(param.value("max"), -1e12, 1e12) ||
                param.value("max").toDouble() <= param.value("min").toDouble() ||
                !numberIn(param.value("decimals", 1), 0, 6)))
                return reject("Fixture parameter has an invalid display scale.");
            const QString existingDisplay = param.value("display").toString();
            if (!ai && !scaled && ((!existingDisplay.isEmpty() && !existingDisplay.endsWith('%')) ||
                                  !param.value("unit").toString().isEmpty()))
                return reject("Fixture parameter needs min/max to preserve its display unit.");
            param["value"] = value;
            param["display"] = display.isEmpty() ? displayValue(param, value.toDouble()) : display;
            if (ai) param["aiEdited"] = true;
            params[pi] = param; item["params"] = params;
        } else if (kind == "send") {
            if (!numberIn(value, 0, 100)) return reject("Send amount must be between 0 and 100 percent.");
            item["amount"] = value;
        } else if (kind == "clip") {
            if (!numberIn(value, 0, endBar())) return reject("Clip start exceeds the fixture timeline.");
            item["startBar"] = m_snap ? std::round(value.toDouble() * 4) / 4 : value;
        } else {
            if (value.typeId() != QMetaType::Bool) return reject("Device switch requires a boolean.");
            item["enabled"] = value;
            const QString counterpartKey = kind == "device" ? "inserts" : "devices";
            const QString mappedId = item.value(kind == "device" ? "insertId" : "deviceId", itemId).toString();
            auto counterparts = c.value(counterpartKey).toList();
            for (auto &entry : counterparts) {
                auto counterpart = entry.toMap();
                const QString reverseId = counterpart.value(kind == "device" ? "deviceId" : "insertId").toString();
                if (counterpart.value("id").toString() != mappedId && reverseId != itemId) continue;
                counterpart["enabled"] = value;
                entry = counterpart;
            }
            c[counterpartKey] = counterparts;
        }
        if (ai) item["aiEdited"] = true;
        list[index] = item; c[listKey] = list;
    }
    if (ai) c["aiEdited"] = true;
    return replaceChannel(project, id, c);
}
bool Session::setTrackValue(const QString &id, const QString &key, const QVariant &value) {
#ifdef ZEPHYR_ENGINE
    if (m_engine) {
        const auto target = channel(m_project, id);
        if (target.isEmpty()) return reject("Unknown engine channel.");
        const QString engineId = target.value("engineId", id).toString();
        bool accepted = false;
        if (key == "volumeDb" && numberIn(value, -60, 6)) accepted = m_engine->setGainDb(engineId, value.toDouble());
        else if (key == "pan" && numberIn(value, -1, 1)) accepted = m_engine->setPan(engineId, (value.toDouble() + 1) / 2);
        else if (key == "muted" && value.typeId() == QMetaType::Bool) accepted = m_engine->setMuted(engineId, value.toBool());
        else if (key == "soloed" && value.typeId() == QMetaType::Bool) accepted = m_engine->setSoloed(engineId, value.toBool());
        else return reject("This control is unavailable in the engine shell proof.");
        if (!accepted) return reject(m_engine->error());
        refreshEngine();
        return true;
    }
#endif
    auto candidate = m_project;
    if (!edit(candidate, id, "track", key, {}, value)) return false;
    commit(candidate); return true;
}
bool Session::moveClip(const QString &id, const QString &clipId, double value) {
    if (m_engineConnected) return reject("Engine clip editing is not connected.");
    auto candidate = m_project;
    if (!edit(candidate, id, "clip", clipId, {}, value)) return false;
    commit(candidate); return true;
}
bool Session::setDeviceParam(const QString &id, const QString &deviceId, const QString &paramId, double value) {
    if (m_engineConnected) return reject("Engine plugin editing is not connected.");
    auto candidate = m_project;
    if (!edit(candidate, id, "param", deviceId, paramId, value)) return false;
    commit(candidate); return true;
}
bool Session::setDeviceEnabled(const QString &id, const QString &deviceId, bool value) {
    if (m_engineConnected) return reject("Engine plugin editing is not connected.");
    auto candidate = m_project;
    if (!edit(candidate, id, "device", deviceId, {}, value)) return false;
    commit(candidate); return true;
}
bool Session::setInsertEnabled(const QString &id, const QString &insertId, bool value) {
    if (m_engineConnected) return reject("Engine plugin editing is not connected.");
    auto candidate = m_project;
    if (!edit(candidate, id, "insert", insertId, {}, value)) return false;
    commit(candidate); return true;
}
bool Session::setSendAmount(const QString &id, const QString &sendId, double value) {
    if (m_engineConnected) return reject("Engine send editing is not connected.");
    auto candidate = m_project;
    if (!edit(candidate, id, "send", sendId, {}, value)) return false;
    commit(candidate); return true;
}
bool Session::addDevice(const QString &id, const QString &kind) {
    if (m_engineConnected) return reject("Engine plugin loading is not connected.");
    const QMap<QString, QString> names{{"eq", "Channel EQ"}, {"dynamics", "Compressor"},
        {"saturation", "Saturation"}, {"fx", "Reverb"}, {"instrument", "Instrument"}, {"utility", "Utility"}};
    if (!names.contains(kind)) return reject("Unknown fixture device kind: " + kind);
    auto c = channel(m_project, id);
    if (c.isEmpty()) return reject("Unknown channel: " + id);
    const QString deviceId = "dev-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto devices = c.value("devices").toList();
    devices.append(QVariantMap{{"id", deviceId}, {"insertId", deviceId}, {"name", names.value(kind)}, {"kind", kind},
        {"enabled", true}, {"preset", "Default"}, {"graph", kind == "eq" ? "eq" : kind == "dynamics" ? "comp" : ""},
        {"params", QVariantList{QVariantMap{{"id", deviceId + "-mix"}, {"label", "Mix"}, {"value", .5}, {"display", "50%"}}}}});
    c["devices"] = devices;
    auto inserts = c.value("inserts").toList();
    inserts.append(QVariantMap{{"id", deviceId}, {"deviceId", deviceId}, {"name", names.value(kind)}, {"enabled", true}});
    c["inserts"] = inserts;
    auto candidate = m_project;
    replaceChannel(candidate, id, c);
    commit(candidate); return true;
}
double Session::endBar() const { return m_project.value("startBar", 9).toDouble() + m_project.value("totalBars", 14).toDouble(); }
void Session::togglePlayback() {
#ifdef ZEPHYR_ENGINE
    if (m_engine) {
        if (!m_engine->requestPlayback(!m_engine->playing())) reject(m_engine->error());
        else refreshEngine();
        return;
    }
#endif
    if (m_playing) { stop(); return; }
    if (m_bar >= endBar()) m_bar = m_project.value("startBar", 9).toDouble();
    m_playing = true; m_clock.start(); m_timer.start();
    notify("Fixture transport moves the playhead. No sound is generated.");
}
void Session::stop() {
#ifdef ZEPHYR_ENGINE
    if (m_engine) {
        if (!m_engine->requestPlayback(false)) reject(m_engine->error());
        else refreshEngine();
        return;
    }
#endif
    m_playing = false; m_timer.stop(); emit changed();
}
bool Session::seek(double bar) {
#ifdef ZEPHYR_ENGINE
    if (m_engine) {
        if (!m_engine->seekBar(bar)) return reject(m_engine->error());
        refreshEngine(); return true;
    }
#endif
    if (!numberIn(bar, 0, endBar())) return reject("Seek position exceeds the fixture timeline.");
    m_bar = bar; if (m_playing) m_clock.restart(); emit changed(); return true;
}
void Session::toggleLoop() {
    if (m_engineConnected) { reject("Engine loop control is not connected."); return; }
    m_loop = !m_loop; emit changed();
}
void Session::toggleSnap() { m_snap = !m_snap; emit changed(); }
void Session::toggleAutomation() { m_automation = !m_automation; emit changed(); }
bool Session::selectLauncherSlot(const QString &sceneId, const QString &trackId) {
    const auto scenes = m_project.value("launcherScenes").toList();
    const int si = indexOf(scenes, sceneId);
    if (si < 0) return reject("Unknown launcher scene.");
    for (const auto &entry : scenes[si].toMap().value("slots").toList()) {
        const auto slot = entry.toMap();
        if (slot.value("trackId").toString() != trackId) continue;
        const QString clipId = slot.value("clipId").toString();
        const auto track = channel(m_project, trackId);
        if (track.isEmpty()) return reject("Unknown launcher track.");
        if (!clipId.isEmpty()) {
            if (m_project.contains("clipSources")) {
                const auto sources = m_project.value("clipSources").toList();
                const int index = indexOf(sources, clipId);
                if (index < 0 || sources[index].toMap().value("trackId").toString() != trackId)
                    return reject("Launcher source does not belong to this track.");
            } else if (indexOf(track.value("clips").toList(), clipId) < 0)
                return reject("Launcher clip does not belong to this track.");
        }
        m_trackId = trackId; m_clipId = clipId; m_origin = "launcher";
        emit selectionChanged();
        if (!clipId.isEmpty()) emit clipSelected();
        emit changed(); return true;
    }
    return reject("No launcher slot exists for this track.");
}
bool Session::launchScene(const QString &sceneId) {
    if (m_engineConnected) return reject("Engine launcher scheduling is not connected.");
    const auto scenes = m_project.value("launcherScenes").toList();
    if (indexOf(scenes, sceneId) < 0) return reject("Unknown launcher scene.");
    m_sceneId = sceneId;
    notify("Fixture scene selected. No clips produce sound.");
    return true;
}
QVariant Session::valueForChange(const QVariantMap &project, const QVariantMap &change) const {
    const auto c = channel(project, change.value("targetId").toString());
    if (c.isEmpty()) return {};
    if (change.contains("key")) return c.value(change.value("key").toString());
    const bool send = change.contains("sendId");
    const auto list = c.value(send ? "sends" : "devices").toList();
    const int i = indexOf(list, change.value(send ? "sendId" : "deviceId").toString());
    if (i < 0) return {};
    const auto item = list[i].toMap();
    if (send) return item.value("amount");
    const auto params = item.value("params").toList();
    const int pi = indexOf(params, change.value("paramId").toString());
    return pi < 0 ? QVariant() : params[pi].toMap().value("value");
}
bool Session::buildAiCandidate(QVariantMap &candidate) {
    if (m_plan.isEmpty()) return reject("No fixture command is staged.");
    candidate = m_project;
    for (const auto &entry : m_plan) {
        const auto change = entry.toMap();
        const auto current = valueForChange(m_project, change);
        if (!current.isValid() || !change.contains("before") || current != change.value("before"))
            return reject("Fixture plan conflicts with a manual edit. Undo the edit or discard this plan.");
        const bool track = change.contains("key"), send = change.contains("sendId");
        if (!edit(candidate, change.value("targetId").toString(), track ? "track" : send ? "send" : "param",
                  change.value(track ? "key" : send ? "sendId" : "deviceId").toString(),
                  change.value("paramId").toString(), change.value("after"), change.value("afterDisplay").toString(), true)) return false;
    }
    return true;
}
bool Session::stagePlan(const QVariant &plan) {
    auto changes = plan.toList();
    if (changes.isEmpty()) changes = plan.toMap().value("changes").toList();
    if (changes.isEmpty()) return reject("This fixture command has no editable parameters.");
    const auto previous = m_plan;
    m_plan = changes;
    QVariantMap candidate;
    if (!buildAiCandidate(candidate)) { m_plan = previous; emit changed(); return false; }
    m_aiState = "ready";
    return true;
}
bool Session::previewAi() {
    if (m_engineConnected) return reject("AI preview requires an engine command adapter.");
    if (m_aiState == "applied") return reject("This fixture command is already applied.");
    if (m_aiState == "preview") {
        m_aiState = "ready";
        notify("Fixture preview closed. Parameters remain unchanged."); return true;
    }
    QVariantMap candidate;
    if (!buildAiCandidate(candidate)) return false;
    m_aiState = "preview";
    notify("Fixture preview stages parameter changes. No audio audition is available.");
    return true;
}
bool Session::applyAi() {
    if (m_engineConnected) return reject("AI apply requires an engine command adapter.");
    if (m_aiState == "applied") return true;
    QVariantMap candidate;
    if (!buildAiCandidate(candidate)) return false;
    endGesture();
    commit(candidate);
    m_aiState = "applied";
    m_history.prepend(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"text", m_lastCommand.isEmpty() ? "Applied reference fixture parameters" : m_lastCommand}, {"time", "now"}, {"status", "applied"}});
    m_project["aiHistory"] = m_history;
    notify("Fixture parameters applied in one undo step. No AI service is connected.");
    return true;
}
void Session::discardAi() {
    m_plan.clear(); m_aiState = "ready";
    notify("Fixture command discarded. Parameters remain unchanged.");
}
bool Session::submitAi(const QString &text) {
    if (m_engineConnected) return reject("AI planning is not connected to this engine session.");
    const QString command = text.simplified().toLower();
    QString key;
    if (QStringList{"clean up the vocal chain but keep it natural.", "clean up the vocal chain but keep it natural", "clean vocal chain"}.contains(command)) key = "vocal";
    else if (QStringList{"make the chorus wider", "widen chorus", "widen the chorus"}.contains(command)) key = "width";
    else { discardAi(); return reject("Unsupported fixture request. Try 'Clean vocal chain' or 'Make the chorus wider'."); }
    const auto selected = selectedTrack();
    const QString name = selected.value("name").toString().toLower();
    if ((key == "vocal" && !name.contains("vocal")) || (key == "width" && !name.contains("guitar"))) {
        discardAi(); return reject(key == "vocal" ? "Select a vocal channel for this fixture command." : "Select Guitars for this fixture command.");
    }
    const auto plan = m_project.value("aiPlans").toMap().value(key);
    m_plan.clear(); m_aiState = "ready";
    if (!stagePlan(plan)) return false;
    m_request = text; m_lastCommand = text;
    auto diff = m_project.value("diff").toMap();
    if (key == "width") {
        QVariantList changes;
        for (const auto &entry : diff.value("changes").toList())
            if (entry.toMap().value("description").toString().contains("width", Qt::CaseInsensitive)) changes.append(entry);
        diff["changes"] = changes;
        diff["title"] = "1 change";
    } else {
        diff = m_project.value("referenceDiff", m_project.value("diff")).toMap();
    }
    m_project["diff"] = diff;
    notify("Built-in fixture command staged. Review the parameters before applying.");
    return true;
}
bool Session::undo() {
    if (m_engineConnected) return reject("Engine undo is not connected in the shell proof.");
    endGesture();
    if (m_undo.isEmpty()) return reject("There is no fixture edit to undo.");
    const auto state = m_undo.takeLast();
    m_project = state.project; m_history = state.history; m_plan = state.plan;
    m_aiState = state.aiState; m_lastCommand = state.lastCommand;
    notify("Fixture edit undone.");
    return true;
}
