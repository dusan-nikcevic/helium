// Ardour headers precede Qt headers because Qt defines emit.
#include "ardour/audioengine.h"
#include "enginesession.h"
#include "ardour/slavable_automation_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/session.h"
#include "ardour/rc_configuration.h"
#include "ardour/track.h"
#include "ardour/panner.h"
#include "pbd/event_loop.h"
#include "commandexecutor.h"
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <atomic>
#include <cmath>
#include <vector>

namespace {
bool string(const QVariant &v) { return v.metaType().id() == QMetaType::QString; }
bool number(const QVariant &v) {
    const auto id = v.metaType().id();
    return id == QMetaType::Double || id == QMetaType::Float || id == QMetaType::Int || id == QMetaType::UInt || id == QMetaType::LongLong || id == QMetaType::ULongLong;
}
bool id(const QVariant &v) {
    static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$");
    return string(v) && v.toString().size() <= 128 && pattern.match(v.toString()).hasMatch();
}
bool fields(const QVariantMap &m, const QStringList &required, const QStringList &allowed) {
    for (const auto &key : required) if (!m.contains(key)) return false;
    for (auto i = m.cbegin(); i != m.cend(); ++i) if (!allowed.contains(i.key())) return false;
    return true;
}
bool linked(const std::shared_ptr<ARDOUR::AutomationControl> &control) {
    const auto slavable = std::dynamic_pointer_cast<ARDOUR::SlavableAutomationControl>(control);
    return slavable && slavable->slaved();
}
bool targetShape(const QVariant &v) {
    if (v.metaType().id() != QMetaType::QVariantMap) return false;
    const auto t = v.toMap();
    if (!fields(t, {"kind", "id"}, {"kind", "id", "routeId", "playlistId"}) || !id(t.value("id"))) return false;
    const auto kind = t.value("kind").toString();
    if (kind == "route") return t.size() == 2;
    if (kind != "processor" && kind != "send" && kind != "region") return false;
    if (!id(t.value("routeId"))) return false;
    return kind == "region" ? id(t.value("playlistId")) : !t.contains("playlistId");
}
double gainDb(double value) { return value <= 0 ? -120.0 : std::max(-120.0, 20 * std::log10(value)); }
bool equal(double a, double b) { return std::abs(a-b) <= 0.0001; }
bool rawEqual(double a, double b) { return std::abs(a-b) <= 1e-12 + 1e-7 * std::max(std::abs(a), std::abs(b)); }
struct Write {
    std::shared_ptr<ARDOUR::AutomationControl> control;
    QVariantMap target;
    QString property, name;
    double before = 0, after = 0;
    int index = 0;
    std::weak_ptr<ARDOUR::Route> route;
    bool soloControl = false, muteControl = false;
    double read() const {
        if (const auto live = route.lock()) {
            if (soloControl) return live->self_soloed() ? 1.0 : 0.0;
            if (muteControl) return live->muted_by_self() ? 1.0 : 0.0;
        }
        return control->get_value();
    }
    QVariant displayValue(double raw) const {
        if (property == "muted" || property == "soloed") return bool(raw != 0);
        return property == "gainDb" ? QVariant(gainDb(raw)) : QVariant(raw);
    }
};
QString display(const QString &property, const QVariant &value) {
    if (value.metaType().id() == QMetaType::Bool) return value.toBool() ? "On" : "Off";
    return QString::number(value.toDouble(), 'f', property == "gainDb" ? 2 : 4) + (property == "gainDb" ? " dB" : "");
}
QVariantList diff(const std::vector<Write> &writes, bool inverse = false) {
    QVariantList values;
    for (const auto &w : writes) {
        const auto before = w.displayValue(inverse ? w.after : w.before), after = w.displayValue(inverse ? w.before : w.after);
        QVariantMap change{{"operationIndex", w.index}, {"target", w.target}, {"property", w.property}, {"before", before}, {"after", after},
            {"label", (w.name + " " + w.property).left(200)}, {"beforeDisplay", display(w.property, before)}, {"afterDisplay", display(w.property, after)}};
        if (w.property == "gainDb") change.insert("unit", "dB");
        values.append(change);
    }
    return values;
}
struct ObservedControl {
    std::shared_ptr<ARDOUR::AutomationControl> control;
    double value;
    ARDOUR::AutoState mode;
    bool writable;
    bool linked;
};
struct Snapshot {
    QVariantMap semantic;
    std::vector<ObservedControl> controls;
};
struct Pending {
    QVariantMap request;
    std::vector<Write> writes;
    Snapshot snapshot;
    std::vector<std::unique_ptr<ARDOUR::SessionEvent>> applyEvents, inverseEvents;
    bool undo = false;
    std::atomic<bool> done{false};
    QElapsedTimer elapsed;
    ARDOUR::SessionEvent *returnEvent = nullptr;
    int failure = 0, failureIndex = -1;
    double actual = 0;
    bool stateValid = true;
};
struct RunTransaction {
    std::shared_ptr<Pending> pending;
    void operator()() const {
        // This code runs in Ardour's process thread. Qt values remain untouched here.
        for (const auto &entry : pending->snapshot.controls) if (!rawEqual(entry.control->get_value(), entry.value) || entry.control->automation_state() != entry.mode || entry.control->writable() != entry.writable || linked(entry.control) != entry.linked) { pending->failure = 1; break; }
        for (size_t i = 0; !pending->failure && i < pending->writes.size(); ++i) {
            const auto &w = pending->writes[i];
            if (w.route.expired() || (w.soloControl && (ARDOUR::Config->get_exclusive_solo() || ARDOUR::Config->get_solo_control_is_listen_control()))) { pending->failure = 1; pending->failureIndex = int(i); break; }
            if (!w.control->writable() || w.control->automation_state() != ARDOUR::Off) { pending->failure = 2; pending->failureIndex = int(i); break; }
            if (!rawEqual(w.read(), w.before)) { pending->failure = 1; pending->failureIndex = int(i); pending->actual = w.read(); break; }
        }
        size_t written = 0;
        try {
            for (; !pending->failure && written < pending->writes.size(); ++written) {
                const auto &w = pending->writes[written];
                pending->applyEvents[written]->rt_slot();
                if (!rawEqual(w.read(), w.after)) { pending->failureIndex = int(written); ++written; pending->failure = 3; break; }
            }
        } catch (...) { pending->failure = 3; pending->failureIndex = int(written); ++written; }
        if (pending->failure == 3) {
            while (written) {
                const auto &w = pending->writes[--written];
                try { pending->inverseEvents[written]->rt_slot(); if (!rawEqual(w.read(), w.before)) pending->stateValid = false; }
                catch (...) { pending->stateValid = false; }
            }
        }

    }
};
struct Receipt { QByteArray payload; QVariantMap result; };
struct Transaction { QString id, label, source; std::vector<Write> writes; };
}
struct CommandExecutor::Impl {
    ARDOUR::Session &session;
    QString uuid;
    qulonglong revision = 0;
    Snapshot observed;
    bool valid = true, gesture = false;
    QString gestureId;
    std::shared_ptr<Pending> pending;
    std::vector<Transaction> history;
    QVariantList audit;
    QMap<QString, Receipt> receipts;
    Impl(ARDOUR::Session &s) : session(s), uuid(QString::fromStdString(s.uuid())) {}
    void invalidateHistory() {
        history.clear(); gesture = false; gestureId.clear();
        for (auto &entry : audit) { auto map = entry.toMap(); if (map.value("undoable").toBool()) { map["undoable"] = false; map["status"] = "invalidated"; entry = map; } }
    }
    Snapshot snapshot() const {
        Snapshot result;
        result.semantic["exclusiveSolo"] = ARDOUR::Config->get_exclusive_solo();
        result.semantic["listenMode"] = ARDOUR::Config->get_solo_control_is_listen_control();
        for (const auto &route : *session.get_routes()) {
            const QString rid = QString::fromStdString(route->id().to_s());
            QVariantMap values{{"name", QString::fromStdString(route->name())}, {"inputs", route->n_inputs().n_audio()}};
            auto add = [&](const QString &name, std::shared_ptr<ARDOUR::AutomationControl> control) {
                if (!control) return;
                const double value = control->get_value();
                values[name] = value; values[name + "Mode"] = int(control->automation_state());
                values[name + "Writable"] = control->writable();
                if (const auto slaved = std::dynamic_pointer_cast<ARDOUR::SlavableAutomationControl>(control)) values[name + "Linked"] = slaved->slaved();
                result.controls.push_back({control, value, control->automation_state(), control->writable(), linked(control)});
            };
            add("gainDb", route->gain_control()); add("muted", route->mute_control()); add("soloed", route->solo_control());
            add("panPosition", route->pan_azimuth_control());
            add("soloSafe", route->solo_safe_control()); add("soloIsolated", route->solo_isolate_control());
            add("recordArmed", route->rec_enable_control());
            result.semantic[rid] = values;
        }
        return result;
    }
    QVariantMap base(const QVariantMap &request) const {
        const auto phase = request.value("phase").toString();
        return {{"schemaVersion", 1}, {"commandId", id(request.value("commandId")) ? request.value("commandId") : QVariant("invalid")},
            {"sessionId", id(request.value("sessionId")) ? request.value("sessionId") : QVariant(uuid)},
            {"phase", QStringList{"preview", "apply", "undo"}.contains(phase) ? phase : "apply"},
            {"status", "rejected"}, {"revision", revision}, {"stateValid", valid}, {"changes", QVariantList{}}};
    }
    QVariantMap failure(const QVariantMap &request, QString code, QString message, QString path, int index = -1, QVariant expected = {}, QVariant actual = {}) const {
        auto result = base(request);
        QVariantMap error{{"code", code}, {"message", message.left(500)}, {"recoverable", code == "CONFLICT" || code == "AUTOMATION_CONFLICT" || code == "UNDO_CONFLICT"}, {"path", path}};
        if (index >= 0) { error["operationIndex"] = index; error["target"] = request.value("operations").toList().value(index).toMap().value("target"); }
        if (expected.isValid()) error["expected"] = expected;
        if (actual.isValid()) error["actual"] = actual;
        result["error"] = error;
        return result;
    }
};
CommandExecutor::CommandExecutor(ARDOUR::Session &s, PBD::EventLoop &, QObject *parent) : QObject(parent), d(std::make_unique<Impl>(s)) {
    d->observed = d->snapshot();
}
CommandExecutor::~CommandExecutor() = default;
QString CommandExecutor::sessionId() const { return d->uuid; }
qulonglong CommandExecutor::revision() const { return d->revision; }
bool CommandExecutor::busy() const { return bool(d->pending); }
QVariantList CommandExecutor::history() const { return d->audit; }

void CommandExecutor::observe() {
    if (busy()) return;
    PBD::Mutex::Lock lock(ARDOUR::AudioEngine::instance()->process_lock());
    auto current = d->snapshot();
    lock.release();
    if (current.semantic != d->observed.semantic) {
        d->observed = std::move(current); ++d->revision; d->invalidateHistory();
        Q_EMIT stateChanged();
    }
}
bool CommandExecutor::beginGesture() {
    if (busy() || d->gesture || !d->valid) return false;
    observe(); d->gesture = true; d->gestureId = QUuid::createUuid().toString(QUuid::WithoutBraces); return true;
}
bool CommandExecutor::endGesture() {
    if (busy() || !d->gesture) return false;
    d->gesture = false; d->gestureId.clear(); Q_EMIT stateChanged(); return true;
}
bool CommandExecutor::submit(const QVariantMap &request) {
    auto reject = [&](QString code, QString message, QString path, int index = -1, QVariant expected = {}, QVariant actual = {}) {
        Q_EMIT finished(d->failure(request, code, message, path, index, expected, actual)); return false;
    };
    if (!d->valid) return reject("ENGINE_FAILURE", "Engine state requires reload.", "");
    if (QJsonDocument::fromVariant(request).toJson(QJsonDocument::Compact).size() > 262144) return reject("INVALID_COMMAND", "Command exceeds 256 KiB.", "");
    if (!fields(request, {"schemaVersion", "commandId", "sessionId", "phase", "source", "expectedRevision", "selection", "groupMode"},
            {"schemaVersion", "commandId", "sessionId", "phase", "source", "expectedRevision", "selection", "groupMode", "label", "operations", "transactionId"}))
        return reject("INVALID_COMMAND", "Required or unknown envelope fields.", "");
    if (!number(request.value("schemaVersion")) || request.value("schemaVersion").toDouble() != 1) return reject("UNSUPPORTED_VERSION", "Only schema version 1 is supported.", "/schemaVersion");
    if (!id(request.value("commandId")) || !id(request.value("sessionId"))) return reject("INVALID_COMMAND", "Invalid command or session ID.", "/commandId");
    const QString phase = request.value("phase").toString();
    if (!string(request.value("phase")) || !QStringList{"preview", "apply", "undo"}.contains(phase)) return reject("INVALID_COMMAND", "Invalid phase.", "/phase");
    if (!string(request.value("source")) || !QStringList{"ui", "ai"}.contains(request.value("source").toString())) return reject("INVALID_COMMAND", "Source must be ui or ai.", "/source");
    if (!string(request.value("groupMode")) || request.value("groupMode") != "independent") return reject("INVALID_COMMAND", "Only independent group mode is supported.", "/groupMode");
    const auto rv = request.value("expectedRevision");
    if (!number(rv) || !std::isfinite(rv.toDouble()) || rv.toDouble() < 0 || rv.toDouble() > 9007199254740991.0 || std::floor(rv.toDouble()) != rv.toDouble())
        return reject("INVALID_COMMAND", "Revision must be a nonnegative safe integer.", "/expectedRevision");
    if (request.contains("label") && (!string(request.value("label")) || request.value("label").toString().size() > 200)) return reject("INVALID_COMMAND", "Invalid label.", "/label");
    if (request.value("selection").metaType().id() != QMetaType::QVariantList || request.value("selection").toList().size() > 256) return reject("INVALID_COMMAND", "Invalid selection.", "/selection");
    QSet<QByteArray> selected;
    int selectionIndex = 0;
    for (const auto &entry : request.value("selection").toList()) {
        const auto encoded = QJsonDocument::fromVariant(entry).toJson(QJsonDocument::Compact);
        if (!targetShape(entry) || selected.contains(encoded)) return reject("INVALID_COMMAND", "Invalid or duplicate selection target.", "/selection/" + QString::number(selectionIndex));
        selected.insert(encoded); ++selectionIndex;
    }
    if (phase == "undo" ? (!id(request.value("transactionId")) || request.contains("operations")) : (request.contains("transactionId") || request.value("operations").metaType().id() != QMetaType::QVariantList || request.value("operations").toList().isEmpty() || request.value("operations").toList().size() > 256))
        return reject("INVALID_COMMAND", "Invalid phase fields.", "");
    if (request.value("sessionId") != d->uuid) return reject("UNKNOWN_SESSION", "Command names another session.", "/sessionId");
    const QString key = phase + ":" + request.value("commandId").toString();
    const auto payload = QJsonDocument::fromVariant(request).toJson(QJsonDocument::Compact);
    if (phase != "preview" && d->receipts.contains(key)) {
        const auto &receipt = d->receipts[key];
        if (payload != receipt.payload) return reject("IDEMPOTENCY_CONFLICT", "Command ID already has another payload.", "/commandId");
        Q_EMIT finished(receipt.result); return true;
    }
    if (busy()) return reject("CONFLICT", "Another engine transaction is pending.", "/expectedRevision", -1, rv, d->revision);
    observe();
    PBD::Mutex::Lock lock(ARDOUR::AudioEngine::instance()->process_lock());
    auto fresh = d->snapshot();
    if (fresh.semantic != d->observed.semantic) { d->observed = fresh; ++d->revision; d->invalidateHistory(); }
    if (rv.toULongLong() != d->revision) { lock.release(); Q_EMIT stateChanged(); return reject("CONFLICT", "Engine revision changed.", "/expectedRevision", -1, rv, d->revision); }
    auto pending = std::make_shared<Pending>(); pending->request = request; pending->undo = phase == "undo"; pending->snapshot = std::move(fresh);
    for (const auto &entry : request.value("selection").toList()) {
        const auto target = entry.toMap();
        if (target.value("kind") != "route") { lock.release(); return reject("UNSUPPORTED_OPERATION", "Selection kind is not supported by this adapter.", "/selection"); }
        if (!d->session.route_by_id(PBD::ID(target.value("id").toString().toStdString()))) { lock.release(); return reject("TARGET_NOT_FOUND", "Selection route does not exist.", "/selection"); }
    }
    if (pending->undo) {
        if (d->gesture || d->history.empty() || d->history.back().id != request.value("transactionId")) { lock.release(); return reject("UNDO_CONFLICT", "Only the latest completed transaction can be undone.", "/transactionId"); }
        pending->writes = d->history.back().writes;
        for (auto &w : pending->writes) std::swap(w.before, w.after);
    } else {
        QSet<QString> duplicate;
        const auto operations = request.value("operations").toList();
        for (int index = 0; index < operations.size(); ++index) {
            const auto operation = operations[index].toMap();
            const QString path = "/operations/" + QString::number(index);
            if (operations[index].metaType().id() != QMetaType::QVariantMap || !fields(operation, {"action", "target", "expected", "value"}, {"action", "target", "expected", "value", "control", "parameterId", "unit", "semitones"}) || !targetShape(operation.value("target"))) {
                lock.release(); return reject("INVALID_COMMAND", "Invalid operation shape.", path);
            }
            if (!string(operation.value("action")) || !QStringList{"set_route_control", "set_parameter", "set_processor_enabled", "set_send_gain", "move_region", "set_route_output", "transpose_notes"}.contains(operation.value("action").toString())) { lock.release(); return reject("INVALID_COMMAND", "Unknown action.", path + "/action"); }
            if (operation.value("action") != "set_route_control") { lock.release(); return reject("UNSUPPORTED_OPERATION", "Adapter supports route controls only.", path + "/action"); }
            const auto target = operation.value("target").toMap();
            const QString property = operation.value("control").toString();
            if (target.value("kind") != "route" || !string(operation.value("control")) || !QStringList{"gainDb", "panPosition", "muted", "soloed", "recordArmed"}.contains(property) || operation.contains("parameterId") || operation.contains("unit") || operation.contains("semitones")) {
                lock.release(); return reject("INVALID_COMMAND", "Invalid route control fields.", path);
            }
            if (property == "soloed" && (ARDOUR::Config->get_exclusive_solo() || ARDOUR::Config->get_solo_control_is_listen_control())) { lock.release(); return reject("UNSUPPORTED_OPERATION", "Solo requires nonexclusive solo mode, with listen mode off.", path + "/control", index); }
            if (property == "recordArmed") { lock.release(); return reject("UNSUPPORTED_OPERATION", "Record arm is not supported.", path + "/control"); }
            const QString rid = target.value("id").toString(), controlKey = rid + ":" + property;
            if (duplicate.contains(controlKey)) { lock.release(); return reject("INVALID_COMMAND", "Duplicate target and property.", path); }
            duplicate.insert(controlKey);
            const bool toggle = property == "muted" || property == "soloed";
            for (const auto &field : {QString("expected"), QString("value")}) {
                const auto value = operation.value(field);
                if (toggle ? value.metaType().id() != QMetaType::Bool : !number(value) || !std::isfinite(value.toDouble())) { lock.release(); return reject("INVALID_COMMAND", "Wrong or nonfinite control value.", path + "/" + field); }
                if (!toggle && (value.toDouble() < (property == "gainDb" ? -120 : 0) || value.toDouble() > (property == "gainDb" ? 12 : 1))) { lock.release(); return reject("VALUE_OUT_OF_RANGE", "Control value is outside its range.", path + "/" + field); }
            }
            auto route = d->session.route_by_id(PBD::ID(rid.toStdString()));
            if (!route) { lock.release(); return reject("TARGET_NOT_FOUND", "Route does not exist.", path + "/target", index); }
            std::shared_ptr<ARDOUR::AutomationControl> control;
            if (property == "gainDb") control = route->gain_control();
            if (property == "muted") control = route->mute_control();
            if (property == "soloed" && !route->is_master()) control = route->solo_control();
            if (property == "panPosition" && route->n_inputs().n_audio() == 1 && route->panner()) control = route->pan_azimuth_control();
            if (!control) { lock.release(); return reject("CONTROL_NOT_FOUND", "Route does not expose that control.", path + "/control", index); }
            if (const auto slavable = std::dynamic_pointer_cast<ARDOUR::SlavableAutomationControl>(control); slavable && slavable->slaved()) { lock.release(); return reject("UNSUPPORTED_OPERATION", "Linked controls require a contract extension.", path + "/control", index); }
            if (!control->writable() || control->automation_state() != ARDOUR::Off) { lock.release(); return reject("AUTOMATION_CONFLICT", "Control automation must be off and writable.", path + "/control", index); }
            Write w{control, target, property, QString::fromStdString(route->name()), control->get_value(), operation.value("value").toDouble(), index, route};
            w.soloControl = property == "soloed"; w.muteControl = property == "muted";
            w.before = w.read();
            if (toggle) w.after = operation.value("value").toBool() ? 1 : 0;
            else if (property == "gainDb") w.after = w.after <= -120 ? 0 : std::pow(10.0, w.after / 20.0);
            const auto actual = w.displayValue(w.before), expected = operation.value("expected");
            if (toggle ? actual.toBool() != expected.toBool() : !equal(actual.toDouble(), expected.toDouble())) { lock.release(); return reject("CONFLICT", "Expected control value changed.", path + "/expected", index, expected, actual); }
            pending->writes.push_back(std::move(w));
        }
    }
    lock.release();
    if (phase == "preview") {
        auto result = d->base(request); result["status"] = "previewed"; result["changes"] = diff(pending->writes);
        Q_EMIT finished(result); return true;
    }
    auto *engineSession = dynamic_cast<EngineSession *>(&d->session);
    if (!engineSession) return reject("ENGINE_UNAVAILABLE", "Native control event factory is unavailable.", "");
    try {
        for (const auto &write : pending->writes) {
            pending->applyEvents.push_back(engineSession->controlEvent(write.control, write.after));
            pending->inverseEvents.push_back(engineSession->controlEvent(write.control, write.before));
            if (!pending->applyEvents.back() || !pending->inverseEvents.back()) return reject("ENGINE_FAILURE", "Cannot prepare reversible native control events.", "");
        }
    } catch (...) { return reject("ENGINE_FAILURE", "Cannot prepare reversible native control events.", ""); }
    d->pending = pending;
    auto *event = new ARDOUR::SessionEvent(ARDOUR::SessionEvent::RealTimeOperation, ARDOUR::SessionEvent::Add, ARDOUR::SessionEvent::Immediate, 0, 0);
    // Native process_rtop invokes this captureless return directly, without copying a callback.
    event->event_loop = nullptr;
    event->rt_return = [](ARDOUR::SessionEvent *completed) {
        const auto &pending = completed->rt_slot.target<RunTransaction>()->pending;
        pending->returnEvent = completed;
        pending->done.store(true, std::memory_order_release);
    };
    event->rt_slot = RunTransaction{pending};
    pending->elapsed.start();
    d->session.queue_event(event);
    Q_EMIT stateChanged(); return true;
}
void CommandExecutor::poll() {
    if (d->pending && !d->pending->done.load(std::memory_order_acquire) && d->pending->elapsed.elapsed() > 2000) {
        ARDOUR::AudioEngine::instance()->stop();
        d->valid = false;
        cancel();
        return;
    }
    if (!d->pending || !d->pending->done.load(std::memory_order_acquire)) return;
    auto pending = d->pending;
    Snapshot current;
    {
        // The process lock also proves that the captureless RT return has finished.
        PBD::Mutex::Lock lock(ARDOUR::AudioEngine::instance()->process_lock());
        current = d->snapshot();
        delete pending->returnEvent; pending->returnEvent = nullptr;
    }
    if (pending->failure && current.semantic != d->observed.semantic) {
        ++d->revision; d->invalidateHistory();
    }
    auto result = d->base(pending->request);
    d->valid = pending->stateValid;
    if (pending->failure) {
        const QString code = !d->valid || pending->failure == 3 ? "ENGINE_FAILURE" : pending->failure == 2 ? "AUTOMATION_CONFLICT" : "CONFLICT";
        result = d->failure(pending->request, code, !d->valid ? "Rollback failed; reload authoritative engine state." : pending->failure == 3 ? "Engine refused a write; inverse controls restored." : "Engine rejected the transaction before mutation.", pending->undo ? "/transactionId" : "/operations");
        if (pending->failureIndex >= 0) { auto error = result.value("error").toMap(); const auto &write = pending->writes[pending->failureIndex]; error["operationIndex"] = write.index; error["target"] = write.target; result["error"] = error; }
    } else {
        ++d->revision;
        const QString transactionId = pending->undo ? pending->request.value("transactionId").toString() : d->gesture ? d->gestureId : QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (pending->undo) {
            d->history.pop_back();
            for (auto &entry : d->audit) {
                auto map = entry.toMap();
                if (map.value("transactionId") == transactionId) { map["status"] = "undone"; map["undone"] = true; map["undoable"] = false; map["undoChanges"] = diff(pending->writes); map["undoRevision"] = d->revision; entry = map; }
            }
        }
        else if (d->gesture && !d->history.empty() && d->history.back().id == transactionId) {
            auto &previous = d->history.back().writes;
            for (const auto &w : pending->writes) {
                bool merged = false;
                for (auto &old : previous) if (old.target == w.target && old.property == w.property) { old.after = w.after; merged = true; break; }
                if (!merged) previous.push_back(w);
            }
        } else d->history.push_back({transactionId, pending->request.value("label", "Route controls").toString(), pending->request.value("source").toString(), pending->writes});
        if (!pending->undo) {
            const auto &transaction = d->history.back();
            QVariantMap entry{{"transactionId", transactionId}, {"label", transaction.label}, {"source", transaction.source}, {"changes", diff(transaction.writes)}, {"status", "applied"}, {"undone", false}, {"undoable", true}, {"revision", d->revision}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
            bool replaced = false;
            for (auto &old : d->audit) if (old.toMap().value("transactionId") == transactionId) { old = entry; replaced = true; break; }
            if (!replaced) d->audit.append(entry);
        }
        result = d->base(pending->request); result["status"] = pending->undo ? "undone" : "applied"; result["transactionId"] = transactionId; result["changes"] = diff(pending->writes);
    }
    d->observed = std::move(current);
    d->pending.reset();
    const QString key = pending->request.value("phase").toString() + ":" + pending->request.value("commandId").toString();
    d->receipts[key] = {QJsonDocument::fromVariant(pending->request).toJson(QJsonDocument::Compact), result};
    pending->applyEvents.clear(); pending->inverseEvents.clear(); pending->writes.clear(); pending->snapshot.controls.clear();
    pending.reset();
    Q_EMIT stateChanged(); Q_EMIT finished(result);
}

void CommandExecutor::cancel() {
    if (!d->pending) return;
    // The owner stops the engine before cancellation, so pending RT callbacks cannot run.
    const auto pending = d->pending;
    const auto result = d->failure(pending->request, d->valid ? "ENGINE_UNAVAILABLE" : "ENGINE_FAILURE", d->valid ? "Session closed before transaction completion." : "Transaction timed out; reload engine state.", "");
    delete pending->returnEvent; pending->returnEvent = nullptr;
    pending->applyEvents.clear(); pending->inverseEvents.clear(); pending->writes.clear(); pending->snapshot.controls.clear();
    d->pending.reset(); Q_EMIT stateChanged(); Q_EMIT finished(result);
}
