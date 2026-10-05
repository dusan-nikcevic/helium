#include "aiplanner.h"
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace {
bool number(const QVariant &value, double lower, double upper, bool integer = false) {
    if (!value.isValid() || value.typeId() == QMetaType::QString || value.typeId() == QMetaType::Bool) return false;
    bool ok = false;
    const double n = value.toDouble(&ok);
    return ok && std::isfinite(n) && n >= lower && n <= upper && (!integer || n == std::floor(n));
}
bool id(const QVariant &value) {
    static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$");
    return value.typeId() == QMetaType::QString && pattern.match(value.toString()).hasMatch();
}
bool targetValid(const QVariantMap &target) {
    const QString kind = target.value("kind").toString();
    if (!id(target.value("id")) || !QStringList{"route", "region", "processor", "send"}.contains(kind)) return false;
    const QSet<QString> keys = kind == "route" ? QSet<QString>{"kind", "id"} : kind == "region" ?
        QSet<QString>{"kind", "id", "routeId", "playlistId"} : QSet<QString>{"kind", "id", "routeId"};
    for (auto it = target.begin(); it != target.end(); ++it) if (!keys.contains(it.key())) return false;
    return kind == "route" || (id(target.value("routeId")) && (kind != "region" || id(target.value("playlistId"))));
}
QVariantMap rejected(const QString &code, const QString &message) {
    return {{"ok", false}, {"provider", "local-rules"}, {"error", QVariantMap{{"code", code}, {"message", message}}}};
}
QRegularExpressionMatch match(const QString &pattern, const QString &text) {
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption).match(text);
}
QString noteDisplay(const QVariantList &notes) {
    int low = 127, high = 0;
    for (const auto &raw : notes) { const int pitch = raw.toMap().value("pitch").toInt(); low = qMin(low, pitch); high = qMax(high, pitch); }
    return QString("%1 notes, pitch %2..%3").arg(notes.size()).arg(low).arg(high);
}
}

QVariantMap planAiCommand(const QString &input, const QVariantMap &context) {
    QString text = input.simplified();
    if (text.endsWith('.')) text.chop(1);
    if (text.isEmpty() || input.size() > 2000) return rejected("INVALID_COMMAND", "Use one command with at most 2000 characters.");
    if (!id(context.value("sessionId")) || !number(context.value("revision"), 0, 9007199254740991.0, true))
        return rejected("INVALID_COMMAND", "Planning requires a session ID and a valid revision.");
    const auto selection = context.value("selection").toList();
    if (selection.size() != 1) return rejected("TARGET_NOT_FOUND", "Select one track or clip before planning a command.");
    const auto selected = selection.front().toMap();
    const auto route = context.value("selectedRoute").toMap();
    if (!targetValid(selected) || !id(route.value("id")) ||
        selected.value(selected.value("kind").toString() == "route" ? "id" : "routeId") != route.value("id"))
        return rejected("TARGET_NOT_FOUND", "The resolved selection does not belong to the selected channel.");
    QVariantMap target{{"kind", "route"}, {"id", route.value("id")}};
    QVariantMap operation{{"action", "set_route_control"}, {"target", target}};
    QString property, unit, beforeDisplay, afterDisplay, intent;
    QVariant before, after;
    const QString routeName = route.value("name", route.value("id")).toString().left(100);
    const QString amount = "([+-]?(?:[0-9]+(?:\\.[0-9]+)?|\\.[0-9]+))";
    const auto setGain = match("^set (?:gain|volume)(?: of (?:the )?selected (?:track|channel))? to " + amount + " d[bB]$", text);
    const auto deltaGain = match("^(lower|raise) (?:gain|volume)(?: of (?:the )?selected (?:track|channel))? by " + amount + " d[bB]$", text);
    const auto pan = match("^pan(?: (?:the )?selected (?:track|channel))? (left|right|cent(?:er|re))$", text);
    const auto toggle = match("^(mute|unmute|solo|unsolo)(?: (?:the )?selected (?:track|channel))?$", text);
    const auto transpose = match("^transpose(?: (?:the )?selected (?:clip|notes))? (?:by )?(?:(up|down) )?" + amount + " semitones?$", text);
    const auto routing = match("^route(?: (?:the )?selected (?:track|channel))? to (.+)$", text);
    if (setGain.hasMatch() || deltaGain.hasMatch()) {
        if (!number(route.value("gainDb"), -120, 12)) return rejected("UNSUPPORTED_OPERATION", "Selected channel has no readable gain control.");
        before = route.value("gainDb");
        double value;
        if (setGain.hasMatch()) value = setGain.captured(1).toDouble();
        else {
            const double delta = deltaGain.captured(2).toDouble();
            if (delta < 0) return rejected("INVALID_COMMAND", "Lower and raise require a nonnegative dB amount.");
            value = before.toDouble() + (deltaGain.captured(1).compare("lower", Qt::CaseInsensitive) == 0 ? -delta : delta);
        }
        if (!std::isfinite(value) || value < -120 || value > 12) return rejected("VALUE_OUT_OF_RANGE", "Gain must remain between -120 and 12 dB.");
        after = value; property = "gainDb"; unit = "dB";
        beforeDisplay = QString::number(before.toDouble(), 'f', 2) + " dB";
        afterDisplay = QString::number(value, 'f', 2) + " dB";
    } else if (pan.hasMatch()) {
        if (!number(route.value("panPosition"), 0, 1) || (route.contains("panAvailable") && !route.value("panAvailable").toBool()))
            return rejected("UNSUPPORTED_OPERATION", "Selected channel has no supported stereo position control.");
        before = route.value("panPosition");
        const QString direction = pan.captured(1).toLower();
        after = direction == "left" ? 0.0 : direction == "right" ? 1.0 : 0.5;
        property = "panPosition"; unit = "unitless";
        beforeDisplay = QString::number(before.toDouble(), 'f', 3);
        afterDisplay = direction.startsWith("cent") ? "Center" : direction == "left" ? "Left" : "Right";
    } else if (toggle.hasMatch()) {
        const QString action = toggle.captured(1).toLower();
        property = action.endsWith("mute") ? "muted" : "soloed";
        if (route.value(property).typeId() != QMetaType::Bool || (property == "soloed" && route.value("role").toString() == "master"))
            return rejected("UNSUPPORTED_OPERATION", "Selected channel does not support that control.");
        before = route.value(property); after = !action.startsWith("un");
        beforeDisplay = before.toBool() ? "On" : "Off"; afterDisplay = after.toBool() ? "On" : "Off";
    } else if (transpose.hasMatch()) {
        const auto region = context.value("selectedRegion").toMap();
        target = region.value("target").toMap();
        if (selected.value("kind").toString() != "region" || target != selected || region.value("type").toString() != "midi")
            return rejected("UNSUPPORTED_OPERATION", "Select one MIDI arrangement clip with readable notes.");
        const double raw = transpose.captured(2).toDouble();
        const QString direction = transpose.captured(1).toLower();
        if (!std::isfinite(raw) || raw != std::floor(raw) || raw < -127 || raw > 127 || (!direction.isEmpty() && raw < 0))
            return rejected("VALUE_OUT_OF_RANGE", "Transpose requires an integer semitone amount between -127 and 127.");
        const int semitones = int(direction == "down" ? -raw : raw);
        const auto notes = region.value("notes").toList();
        if (notes.isEmpty() || notes.size() > 4096) return rejected("UNSUPPORTED_OPERATION", "Select a MIDI clip containing 1 through 4096 notes.");
        QVariantList shifted;
        const QSet<QString> noteKeys{"bar", "beat", "pitch", "length", "velocity", "channel", "id", "previewRow"};
        for (const auto &entry : notes) {
            auto note = entry.toMap();
            for (auto it = note.begin(); it != note.end(); ++it)
                if (!noteKeys.contains(it.key())) return rejected("UNSUPPORTED_OPERATION", "The note contains fields outside the transpose contract.");
            if (!number(note.value("pitch"), 0, 127, true) || !number(note.value("bar"), 0, 1000000000) ||
                !number(note.value("beat"), 0, 1000000000) || !number(note.value("length"), 0, 1000000000) || note.value("length").toDouble() == 0 ||
                (note.contains("velocity") && !number(note.value("velocity"), 0, 127, true)) ||
                (note.contains("channel") && !number(note.value("channel"), 0, 15, true)) ||
                (note.contains("previewRow") && !number(note.value("previewRow"), 0, 127, true)) || (note.contains("id") && !id(note.value("id"))))
                return rejected("INVALID_COMMAND", "Selected MIDI notes have invalid fields.");
            const int pitch = note.value("pitch").toInt() + semitones;
            if (pitch < 0 || pitch > 127) return rejected("VALUE_OUT_OF_RANGE", "Transpose would move a note outside MIDI pitches 0 through 127.");
            note["pitch"] = pitch; shifted.append(note);
        }
        property = "notes"; before = notes; after = shifted;
        beforeDisplay = noteDisplay(notes); afterDisplay = noteDisplay(shifted);
        operation = {{"action", "transpose_notes"}, {"target", target}, {"semitones", semitones}};
        intent = QString("Transpose %1 by %2 semitones").arg(region.value("name", target.value("id")).toString().left(100)).arg(semitones);
    } else if (routing.hasMatch()) {
        const auto routes = context.value("routes").toList();
        if (routes.isEmpty() || routes.size() > 4096) return rejected("UNSUPPORTED_OPERATION", "Routing requires 1 through 4096 resolved routes.");
        QVariantMap destination;
        for (const auto &entry : routes) {
            const auto candidate = entry.toMap();
            if (candidate.value("name").toString().simplified().compare(routing.captured(1).simplified(), Qt::CaseInsensitive) != 0) continue;
            if (!destination.isEmpty()) return rejected("TARGET_NOT_FOUND", "Several routes have that name. Choose a unique destination.");
            destination = candidate;
        }
        if (destination.isEmpty() || !id(destination.value("id"))) return rejected("TARGET_NOT_FOUND", "No existing route has that destination name.");
        if (!QStringList{"bus", "return", "master"}.contains(destination.value("role").toString()) || route.value("role").toString() == "master" || destination.value("id") == route.value("id"))
            return rejected("UNSUPPORTED_OPERATION", "Route to an existing bus, return or master without routing the master itself.");
        if (!id(route.value("outputRouteId"))) return rejected("UNSUPPORTED_OPERATION", "Selected channel has no single internal output destination.");
        QString next = destination.value("id").toString();
        QSet<QString> visited;
        // ponytail: O(n²) output-chain lookup; index route IDs if large sessions need routing plans.
        while (!next.isEmpty()) {
            if (next == route.value("id").toString() || visited.contains(next)) return rejected("INVALID_COMMAND", "This output route would create feedback.");
            visited.insert(next);
            QVariantMap current;
            for (const auto &entry : routes) if (entry.toMap().value("id").toString() == next) { current = entry.toMap(); break; }
            if (current.isEmpty()) return rejected("UNSUPPORTED_OPERATION", "Routing context lacks a destination in the output chain.");
            next = current.value("role").toString() == "master" ? QString() : current.value("outputRouteId").toString();
        }
        property = "outputRoute"; before = QVariantMap{{"routeId", route.value("outputRouteId")}};
        after = QVariantMap{{"routeId", destination.value("id")}};
        beforeDisplay = route.value("outputRouteId").toString();
        for (const auto &entry : routes) if (entry.toMap().value("id") == route.value("outputRouteId")) beforeDisplay = entry.toMap().value("name").toString();
        afterDisplay = destination.value("name").toString().left(100);
        operation["action"] = "set_route_output";
    } else return rejected("INVALID_COMMAND", "Use set/lower/raise gain, pan left/right/center, mute/unmute, solo/unsolo, transpose semitones, or route to an existing bus.");
    operation["expected"] = before; operation["value"] = after;
    if (operation.value("action").toString() == "set_route_control") operation["control"] = property;
    if (intent.isEmpty()) intent = QString("Set %1 %2 to %3").arg(routeName, property, afterDisplay);
    const QVariantMap request{{"schemaVersion", 1}, {"commandId", "local-" + QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"sessionId", context.value("sessionId")}, {"phase", "preview"}, {"source", "ai"}, {"groupMode", "independent"},
        {"expectedRevision", context.value("revision")}, {"selection", selection}, {"label", intent.left(200)}, {"operations", QVariantList{operation}}};
    QVariantMap change{{"operationIndex", 0}, {"target", target}, {"property", property}, {"before", before}, {"after", after},
        {"label", (routeName + " " + property).left(200)}, {"beforeDisplay", beforeDisplay.left(100)}, {"afterDisplay", afterDisplay.left(100)}};
    if (!unit.isEmpty()) change["unit"] = unit;
    return {{"ok", true}, {"provider", "local-rules"}, {"intent", intent}, {"request", request}, {"changes", QVariantList{change}}};
}
