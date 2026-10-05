#include "fixturecommands.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace {
bool fail(QVariantMap *error, const QString &code, const QString &message, const QString &path = {}) {
    *error = {{"code", code}, {"message", message}, {"recoverable", true}, {"path", path}};
    return false;
}
bool keys(const QVariantMap &m, const QStringList &required, const QStringList &optional = {}) {
    for (const auto &k : required)
        if (!m.contains(k))
            return false;
    for (auto it = m.begin(); it != m.end(); ++it)
        if (!required.contains(it.key()) && !optional.contains(it.key()))
            return false;
    return true;
}
bool mapValue(const QVariant &v) {
    return v.typeId() == QMetaType::QVariantMap;
}
bool listValue(const QVariant &v) {
    return v.typeId() == QMetaType::QVariantList;
}
bool numeric(const QVariant &v, double lo, double hi, bool integer = false) {
    const int type = v.typeId();
    if (type != QMetaType::Double && type != QMetaType::Float && type != QMetaType::Int &&
        type != QMetaType::UInt && type != QMetaType::LongLong && type != QMetaType::ULongLong)
        return false;
    bool ok = false;
    const double n = v.toDouble(&ok);
    return ok && std::isfinite(n) && n >= lo && n <= hi && (!integer || std::floor(n) == n);
}
bool id(const QVariant &v) {
    static const QRegularExpression re("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$");
    return v.typeId() == QMetaType::QString && re.match(v.toString()).hasMatch();
}
bool target(const QVariant &v) {
    if (!mapValue(v))
        return false;
    const auto t = v.toMap();
    const auto kind = t.value("kind").toString();
    QStringList required{"kind", "id"};
    if (kind != "route")
        required.append("routeId");
    if (kind == "region")
        required.append("playlistId");
    if (t.value("kind").typeId() != QMetaType::QString ||
        !QStringList{"route", "region", "processor", "send"}.contains(kind) || !keys(t, required))
        return false;
    for (const auto &k : required)
        if (k != "kind" && !id(t.value(k)))
            return false;
    return true;
}
bool timeValue(const QVariant &v) {
    const auto t = v.toMap();
    const auto domain = t.value("domain").toString();
    return mapValue(v) && keys(t, {"domain", "value"}) && t.value("domain").typeId() == QMetaType::QString &&
           (domain == "beats" || domain == "samples") &&
           numeric(t.value("value"), 0, domain == "beats" ? 1e9 : 9007199254740991.0, domain == "samples");
}
bool notes(const QVariant &v) {
    if (!listValue(v) || v.toList().isEmpty() || v.toList().size() > 4096)
        return false;
    for (const auto &raw : v.toList()) {
        const auto n = raw.toMap();
        if (!mapValue(raw) ||
            !keys(n, {"bar", "beat", "pitch", "length"}, {"velocity", "channel", "id", "previewRow"}) ||
            !numeric(n.value("bar"), 0, 1e9) || !numeric(n.value("beat"), 0, 1e9) ||
            !numeric(n.value("pitch"), 0, 127, true) ||
            (!numeric(n.value("length"), 0, 1e9) || n.value("length").toDouble() <= 0) ||
            (n.contains("velocity") && !numeric(n.value("velocity"), 0, 127, true)) ||
            (n.contains("channel") && !numeric(n.value("channel"), 0, 15, true)) ||
            (n.contains("previewRow") && !numeric(n.value("previewRow"), 0, 127, true)) ||
            (n.contains("id") && !id(n.value("id"))))
            return false;
    }
    return true;
}
bool same(const QVariant &a, const QVariant &b) {
    return QJsonDocument::fromVariant(QVariantList{a}).toJson(QJsonDocument::Compact) ==
           QJsonDocument::fromVariant(QVariantList{b}).toJson(QJsonDocument::Compact);
}
int indexOf(const QVariantList &list, const QString &id) {
    for (int i = 0; i < list.size(); ++i)
        if (list[i].toMap().value("id").toString() == id)
            return i;
    return -1;
}
void replaceRoute(QVariantMap &p, const QString &id, const QVariantMap &r) {
    if (id == "master") {
        p["master"] = r;
        return;
    }
    for (const QString &k : {QString("tracks"), QString("buses"), QString("returns")}) {
        auto list = p.value(k).toList();
        const int i = indexOf(list, id);
        if (i >= 0) {
            list[i] = r;
            p[k] = list;
            return;
        }
    }
}
QString display(const QVariant &v, const QString &unit) {
    if (v.typeId() == QMetaType::Bool)
        return v.toBool() ? "On" : "Off";
    if (listValue(v)) {
        int low = 127, high = 0;
        for (const auto &raw : v.toList()) {
            const int pitch = raw.toMap().value("pitch").toInt();
            low = qMin(low, pitch);
            high = qMax(high, pitch);
        }
        return QString("%1 notes, pitch %2..%3").arg(v.toList().size()).arg(low).arg(high);
    }
    if (mapValue(v))
        return v.toMap().contains("routeId") ? v.toMap().value("routeId").toString()
                                             : QString::number(v.toMap().value("value").toDouble()) + " " +
                                                   v.toMap().value("domain").toString();
    return QString::number(v.toDouble(), 'g', 8) + (unit.isEmpty() ? QString() : " " + unit);
}
bool routingGraph(const QVariantMap &project, const QSet<QString> &sources, QVariantMap *error) {
    QSet<QString> visiting, finished;
    auto visit = [&](auto &&self, const QString &routeId) -> bool {
        if (visiting.contains(routeId)) return fail(error, "CONFLICT", "Output routing would create feedback through an output or send.");
        if (finished.contains(routeId) || routeId == "master") return true;
        const auto route = FixtureCommands::route(project, routeId);
        if (route.isEmpty()) return fail(error, "UNSUPPORTED_OPERATION", "Routing references an unavailable destination.");
        for (const QString &key : {QString("routing"), QString("connections"), QString("sidechains")})
            if (route.contains(key))
                return fail(error, "UNSUPPORTED_OPERATION", "Custom connections cannot be validated by the fixture routing adapter.");
        if (visiting.size() >= 256) return fail(error, "UNSUPPORTED_OPERATION", "Routing graph exceeds the fixture traversal limit.");
        visiting.insert(routeId);
        const QString output = FixtureCommands::outputRouteId(project, route);
        if (output.isEmpty() || !self(self, output)) return false;
        for (const auto &raw : route.value("sends").toList()) {
            const auto send = raw.toMap();
            QString destination = send.value("targetId").toString();
            if (destination.isEmpty()) {
                for (const QString &listKey : {QString("buses"), QString("returns")}) {
                    for (const auto &candidate : project.value(listKey).toList()) {
                        const auto target = candidate.toMap();
                        if (target.value("name").toString().compare(send.value("name").toString(), Qt::CaseInsensitive) != 0) continue;
                        if (!destination.isEmpty()) return fail(error, "UNSUPPORTED_OPERATION", "Send destination name is ambiguous.");
                        destination = target.value("id").toString();
                    }
                }
            }
            if (destination.isEmpty()) return fail(error, "UNSUPPORTED_OPERATION", "A send requires a resolved destination before routing changes.");
            if (!self(self, destination)) return false;
        }
        visiting.remove(routeId); finished.insert(routeId); return true;
    };
    for (const auto &source : sources) if (!visit(visit, source)) return false;
    return true;
}
} // namespace
bool FixtureCommands::validate(const QVariantMap &r, QVariantMap *error) {
    if (!keys(r,
              {"schemaVersion", "commandId", "sessionId", "phase", "source", "expectedRevision", "selection",
               "groupMode"},
              {"label", "operations", "transactionId"}))
        return fail(error, "INVALID_COMMAND", "Command fields do not match schema v1.");
    if (!numeric(r.value("schemaVersion"), 1, 1, true))
        return fail(error, "UNSUPPORTED_VERSION", "Only command schema v1 is supported.", "/schemaVersion");
    const auto phase = r.value("phase").toString();
    if (r.value("phase").typeId() != QMetaType::QString || r.value("source").typeId() != QMetaType::QString ||
        r.value("groupMode").typeId() != QMetaType::QString || !id(r.value("commandId")) ||
        !id(r.value("sessionId")) || !QStringList{"preview", "apply", "undo"}.contains(phase) ||
        !QStringList{"ui", "ai"}.contains(r.value("source").toString()) ||
        r.value("groupMode").toString() != "independent" ||
        !numeric(r.value("expectedRevision"), 0, 9007199254740991.0, true) ||
        !listValue(r.value("selection")) || r.value("selection").toList().size() > 256 ||
        (r.contains("label") &&
         (r.value("label").typeId() != QMetaType::QString || r.value("label").toString().size() > 200)))
        return fail(error, "INVALID_COMMAND", "Invalid command envelope.");
    QSet<QByteArray> selected;
    for (const auto &t : r.value("selection").toList()) {
        const auto encoded = QJsonDocument::fromVariant(t).toJson(QJsonDocument::Compact);
        if (!target(t) || selected.contains(encoded))
            return fail(error, "INVALID_COMMAND", "Invalid or repeated selection target.", "/selection");
        selected.insert(encoded);
    }
    if (phase == "undo")
        return (!r.contains("operations") && id(r.value("transactionId"))) ||
               fail(error, "INVALID_COMMAND", "Undo requires a transaction ID and no operations.");
    if (r.contains("transactionId") || !listValue(r.value("operations")) ||
        r.value("operations").toList().isEmpty() || r.value("operations").toList().size() > 256)
        return fail(error, "INVALID_COMMAND", "Preview and apply require 1 to 256 operations.",
                    "/operations");
    int i = 0;
    for (const auto &raw : r.value("operations").toList()) {
        const auto op = raw.toMap();
        const auto action = op.value("action").toString(),
                   kind = op.value("target").toMap().value("kind").toString();
        QStringList required{"action", "target", "expected", "value"};
        if (action == "set_route_control")
            required.append("control");
        if (action == "set_parameter")
            required.append({"parameterId", "unit"});
        if (action == "transpose_notes")
            required.append("semitones");
        bool valid = mapValue(raw) && keys(op, required) &&
                     op.value("action").typeId() == QMetaType::QString && target(op.value("target"));
        const auto before = op.value("expected"), after = op.value("value");
        if (action == "set_route_control") {
            const auto control = op.value("control").toString();
            valid &= kind == "route" && op.value("control").typeId() == QMetaType::QString;
            if (control == "gainDb")
                valid &= numeric(before, -120, 12) && numeric(after, -120, 12);
            else if (control == "panPosition")
                valid &= numeric(before, 0, 1) && numeric(after, 0, 1);
            else
                valid &= QStringList{"muted", "soloed", "recordArmed"}.contains(control) &&
                         before.typeId() == QMetaType::Bool && after.typeId() == QMetaType::Bool;
        } else if (action == "set_parameter")
            valid &= kind == "processor" && id(op.value("parameterId")) &&
                     op.value("unit").typeId() == QMetaType::QString &&
                     !op.value("unit").toString().isEmpty() && op.value("unit").toString().size() <= 32 &&
                     numeric(before, -1e300, 1e300) && numeric(after, -1e300, 1e300);
        else if (action == "set_processor_enabled")
            valid &= kind == "processor" && before.typeId() == QMetaType::Bool &&
                     after.typeId() == QMetaType::Bool;
        else if (action == "set_send_gain")
            valid &= kind == "send" && numeric(before, -120, 12) && numeric(after, -120, 12);
        else if (action == "move_region")
            valid &= kind == "region" && timeValue(before) && timeValue(after);
        else if (action == "set_route_output")
            valid &= kind == "route" && mapValue(before) && mapValue(after) &&
                     keys(before.toMap(), {"routeId"}) && keys(after.toMap(), {"routeId"}) &&
                     id(before.toMap().value("routeId")) && id(after.toMap().value("routeId"));
        else if (action == "transpose_notes")
            valid &= kind == "region" && notes(before) && notes(after) &&
                     numeric(op.value("semitones"), -127, 127, true);
        else
            valid = false;
        if (!valid)
            return fail(error, "INVALID_COMMAND", "Operation fields or values do not match schema v1.",
                        QString("/operations/%1").arg(i));
        ++i;
    }
    return true;
}
QVariantMap FixtureCommands::route(const QVariantMap &p, const QString &id) {
    if (id == "master")
        return p.value("master").toMap();
    for (const QString &k : {QString("tracks"), QString("buses"), QString("returns")}) {
        const auto list = p.value(k).toList();
        const int i = indexOf(list, id);
        if (i >= 0)
            return list[i].toMap();
    }
    return {};
}
QString FixtureCommands::outputRouteId(const QVariantMap &p, const QVariantMap &r) {
    if (r.contains("outputRouteId"))
        return r.value("outputRouteId").toString();
    const QString output = r.value("output").toString();
    for (const QString &k : {QString("buses"), QString("returns")})
        for (const auto &raw : p.value(k).toList()) {
            const auto c = raw.toMap();
            if (c.value("name").toString() == output)
                return c.value("id").toString();
        }
    if (output.contains("master", Qt::CaseInsensitive) || output.contains("stereo", Qt::CaseInsensitive))
        return "master";
    return r.value("busId", "master").toString();
}
QVariantMap FixtureCommands::result(const QVariantMap &r, quint64 revision, const QString &status,
                                    const QVariantList &changes, const QVariantMap &error,
                                    const QString &transactionId) {
    QVariantMap v{{"schemaVersion", 1},
                  {"commandId", id(r.value("commandId")) ? r.value("commandId") : QVariant("invalid")},
                  {"sessionId", id(r.value("sessionId")) ? r.value("sessionId") : QVariant("unknown")},
                  {"phase", QStringList{"preview", "apply", "undo"}.contains(r.value("phase").toString())
                                ? r.value("phase")
                                : QVariant("apply")},
                  {"status", status},
                  {"revision", QVariant::fromValue(revision)},
                  {"stateValid", true},
                  {"changes", changes}};
    if (!error.isEmpty())
        v["error"] = error;
    if (!transactionId.isEmpty())
        v["transactionId"] = transactionId;
    return v;
}
bool FixtureCommands::candidate(const QVariantMap &project, const QVariantMap &request,
                                QVariantMap *candidate, QVariantList *changes, QVariantMap *error) {
    if (!validate(request, error))
        return false;
    *candidate = project;
    changes->clear();
    int oi = 0;
    QSet<QByteArray> edited;
    QSet<QString> rerouted;
    for (const auto &raw : request.value("operations").toList()) {
        const auto op = raw.toMap(), t = op.value("target").toMap();
        const auto action = op.value("action").toString(), kind = t.value("kind").toString();
        const auto routeId = t.value(kind == "route" ? "id" : "routeId").toString();
        auto c = route(*candidate, routeId);
        const QString path = QString("/operations/%1").arg(oi);
        if (c.isEmpty())
            return fail(error, "TARGET_NOT_FOUND", "Route does not belong to this session.",
                        path + "/target");
        QVariant actual;
        QString property, unit;
        const auto value = op.value("value");
        if (action == "set_route_control") {
            property = op.value("control").toString();
            const QString key = property == "gainDb"        ? "volumeDb"
                                : property == "panPosition" ? "pan"
                                                            : property;
            if (!c.contains(key))
                return fail(error, "CONTROL_NOT_FOUND", "Route does not expose this control.", path);
            actual = property == "panPosition" ? QVariant((c.value(key).toDouble() + 1) / 2) : c.value(key);
            c[key] = property == "panPosition" ? QVariant(value.toDouble() * 2 - 1) : value;
            unit = property == "gainDb" ? "dB" : "";
        } else if (action == "set_route_output") {
            property = "outputRoute";
            actual = QVariantMap{{"routeId", outputRouteId(*candidate, c)}};
            const QString destination = value.toMap().value("routeId").toString();
            const auto dest = route(*candidate, destination);
            if (dest.isEmpty())
                return fail(error, "TARGET_NOT_FOUND", "Output destination does not belong to this session.",
                            path);
            if (routeId == "master" || routeId == destination ||
                indexOf(candidate->value("tracks").toList(), destination) >= 0)
                return fail(error, "UNSUPPORTED_OPERATION",
                            "Outputs require a bus, return, or master without feedback.", path);
            rerouted.insert(routeId);
            c["outputRouteId"] = destination;
            c["output"] = dest.value("name");
            c["routeLabel"] = dest.value("name");
        } else {
            const QString listKey = kind == "processor" ? "devices" : kind == "send" ? "sends" : "clips";
            auto list = c.value(listKey).toList();
            int index = indexOf(list, t.value("id").toString());
            bool sourceTarget = false;
            if (index < 0 && kind == "region") {
                list = candidate->value("clipSources").toList();
                index = indexOf(list, t.value("id").toString());
                sourceTarget = index >= 0 && list[index].toMap().value("trackId").toString() == routeId;
                if (!sourceTarget)
                    index = -1;
            }
            if (index < 0)
                return fail(error, "TARGET_NOT_FOUND", "Target does not belong to the specified route.",
                            path + "/target");
            auto item = list[index].toMap();
            if (action == "set_parameter") {
                auto params = item.value("params").toList();
                const int pi = indexOf(params, op.value("parameterId").toString());
                if (pi < 0)
                    return fail(error, "CONTROL_NOT_FOUND", "Processor does not expose this parameter.",
                                path);
                auto p = params[pi].toMap();
                const bool scaled = p.contains("min") && p.contains("max");
                unit = p.value("unit", scaled ? "" : "%").toString();
                if (unit.isEmpty() && !scaled)
                    unit = "%";
                if (unit != op.value("unit").toString())
                    return fail(error, "VALUE_OUT_OF_RANGE",
                                "Parameter unit does not match its physical unit.", path + "/unit");
                const double lo = scaled ? p.value("min").toDouble() : 0,
                             hi = scaled ? p.value("max").toDouble() : 100;
                if ((scaled &&
                     (!numeric(p.value("min"), -1e12, 1e12) || !numeric(p.value("max"), -1e12, 1e12))) ||
                    !(hi > lo) || !numeric(value, lo, hi))
                    return fail(error, "VALUE_OUT_OF_RANGE", "Parameter exceeds its physical range.",
                                path + "/value");
                actual = lo + p.value("value").toDouble() * (hi - lo);
                property = op.value("parameterId").toString();
                p["value"] = (value.toDouble() - lo) / (hi - lo);
                p["display"] = display(value, unit);
                p["aiEdited"] = request.value("source").toString() == "ai";
                params[pi] = p;
                item["params"] = params;
            } else if (action == "set_processor_enabled") {
                property = "enabled";
                actual = item.value("enabled", true);
                item["enabled"] = value;
                auto inserts = c.value("inserts").toList();
                for (auto &rawInsert : inserts) {
                    auto insert = rawInsert.toMap();
                    if (insert.value("deviceId").toString() == item.value("id").toString() ||
                        insert.value("id").toString() == item.value("insertId").toString()) {
                        insert["enabled"] = value;
                        rawInsert = insert;
                    }
                }
                c["inserts"] = inserts;
            } else if (action == "set_send_gain") {
                property = "gainDb";
                unit = "dB";
                const double amount = item.value("amount").toDouble();
                actual = amount > 0 ? qMax(-120.0, 20 * std::log10(amount / 100)) : -120.0;
                const double amountAfter =
                    value.toDouble() <= -120 ? 0 : 100 * std::pow(10, value.toDouble() / 20);
                if (amountAfter > 100)
                    return fail(error, "VALUE_OUT_OF_RANGE", "Fixture sends cannot exceed 0 dB.", path);
                item["amount"] = amountAfter;
            } else if (action == "move_region" || action == "transpose_notes") {
                if (t.value("playlistId").toString() !=
                    c.value("playlistId", routeId + "-playlist").toString())
                    return fail(error, "TARGET_NOT_FOUND", "Region does not belong to this playlist.", path);
                if (action == "move_region") {
                    if (sourceTarget)
                        return fail(error, "UNSUPPORTED_OPERATION",
                                    "A launcher source has no arrangement position.", path);
                    property = "position";
                    actual =
                        QVariantMap{{"domain", "beats"}, {"value", item.value("startBar").toDouble() * 4}};
                    if (value.toMap().value("domain") != "beats")
                        return fail(error, "UNSUPPORTED_OPERATION",
                                    "Fixture regions support beat positions only.", path);
                    const double end =
                        candidate->value("startBar").toDouble() + candidate->value("totalBars").toDouble();
                    if (value.toMap().value("value").toDouble() / 4 > end)
                        return fail(error, "VALUE_OUT_OF_RANGE",
                                    "Region position exceeds the fixture timeline.", path);
                    item["startBar"] = value.toMap().value("value").toDouble() / 4;
                } else {
                    property = "notes";
                    actual = item.value("notes");
                    const auto before = actual.toList(), after = value.toList();
                    if (item.value("type").toString().compare("midi", Qt::CaseInsensitive) != 0 ||
                        before.isEmpty())
                        return fail(error, "UNSUPPORTED_OPERATION",
                                    "Transposition requires a MIDI region with stored notes.", path);
                    if (before.size() != after.size())
                        return fail(error, "INVALID_COMMAND", "Transposition must preserve the note vector.",
                                    path);
                    for (int ni = 0; ni < before.size(); ++ni) {
                        auto n = before[ni].toMap();
                        n["pitch"] = n.value("pitch").toInt() + op.value("semitones").toInt();
                        if (!same(n, after[ni]))
                            return fail(error, "INVALID_COMMAND",
                                        "Transposition may change only each note pitch by semitones.", path);
                    }
                    item["notes"] = value;
                }
            } else
                return fail(error, "UNSUPPORTED_OPERATION", "Fixture action is unsupported.", path);
            list[index] = item;
            if (sourceTarget)
                (*candidate)["clipSources"] = list;
            else
                c[listKey] = list;
        }
        const auto editKey =
            QJsonDocument::fromVariant(QVariantList{t, action, property}).toJson(QJsonDocument::Compact);
        if (edited.contains(editKey))
            return fail(error, "INVALID_COMMAND", "A batch cannot write the same target property twice.",
                        path);
        edited.insert(editKey);
        if (!same(actual, op.value("expected")))
            return fail(error, "CONFLICT", "Expected value differs from the current target value.",
                        path + "/expected");
        replaceRoute(*candidate, routeId, c);
        changes->append(QVariantMap{{"operationIndex", oi},
                                    {"target", t},
                                    {"property", property},
                                    {"before", actual},
                                    {"after", value},
                                    {"label", c.value("name").toString() + " " + property},
                                    {"beforeDisplay", property == "outputRoute" ? route(*candidate, actual.toMap().value("routeId").toString()).value("name").toString() : display(actual, unit)},
                                    {"afterDisplay", property == "outputRoute" ? route(*candidate, value.toMap().value("routeId").toString()).value("name").toString() : display(value, unit)}});
        ++oi;
    }
    if (!routingGraph(*candidate, rerouted, error)) return false;
    return true;
}
