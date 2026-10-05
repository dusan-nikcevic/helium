#include "aiplannercheck.h"
#include "../aiplanner.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QVariantList>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
QVariantList generatedRequests;
QVariantMap context() {
    const QVariantMap route{{"id", "piano"}, {"name", "Piano"}, {"role", "track"}, {"gainDb", -4.5},
        {"panPosition", 0.42}, {"muted", false}, {"soloed", false}, {"outputRouteId", "master"}};
    return {{"sessionId", "session-1"}, {"revision", 7}, {"selection", QVariantList{QVariantMap{{"kind", "route"}, {"id", "piano"}}}},
        {"selectedRoute", route}, {"routes", QVariantList{route,
            QVariantMap{{"id", "music-bus"}, {"name", "Music Bus"}, {"role", "bus"}, {"outputRouteId", "master"}},
            QVariantMap{{"id", "master"}, {"name", "Master"}, {"role", "master"}}}}};
}
QVariantMap midiContext() {
    auto value = context();
    const QVariantMap target{{"kind", "region"}, {"id", "piano-clip-0"}, {"routeId", "piano"}, {"playlistId", "piano-playlist"}};
    value["selection"] = QVariantList{target};
    value["selectedRegion"] = QVariantMap{{"target", target}, {"name", "Piano clip"}, {"type", "midi"},
        {"notes", QVariantList{QVariantMap{{"bar", 0.0}, {"beat", 0.0}, {"pitch", 67}, {"length", 2.0}, {"previewRow", 5}, {"velocity", 96}},
                              QVariantMap{{"bar", 0.5}, {"beat", 0.0}, {"pitch", 60}, {"length", 1.0}, {"channel", 0}}}}};
    return value;
}
}
int runAiPlannerChecks() {
    generatedRequests.clear();
    int checks = 0, failures = 0;
    auto check = [&](bool condition, const char *label) { ++checks; if (!condition) { ++failures; std::fprintf(stderr, "PLANNER FAIL: %s\n", label); } };
    auto plan = [&](const QString &command, const QVariantMap &ctx) {
        const auto result = planAiCommand(command, ctx);
        if (result.value("ok").toBool()) generatedRequests.append(result.value("request"));
        return result;
    };
    for (const QString &command : {QString("set gain to -6 dB"), QString("lower volume by 3 dB"), QString("raise gain by 2 dB"),
             QString("pan left"), QString("pan right"), QString("pan center"), QString("mute selected track"), QString("unmute"), QString("solo"), QString("unsolo"), QString("route to Music Bus")}) {
        const auto result = plan(command, context());
        check(result.value("ok").toBool() && result.value("provider") == "local-rules", "supported commands use local rules");
        const auto request = result.value("request").toMap();
        check(request.value("phase") == "preview" && request.value("source") == "ai" && request.value("expectedRevision").toInt() == 7, "planner only prepares preview requests");
    }
    auto gain = plan("lower gain by 3 dB", context()).value("request").toMap().value("operations").toList().front().toMap();
    check(gain.value("expected").toDouble() == -4.5 && gain.value("value").toDouble() == -7.5, "gain delta uses authoritative prior value");
    for (const QString &command : {QString("set gain to +13 dB"), QString("set gain to -121 dB"), QString("lower gain by -3 dB"), QString("mute and solo"), QString("run rm -rf /"), QString("pan center then apply"), QString("route to Missing")})
        check(!plan(command, context()).value("ok").toBool(), "malformed or out-of-range command rejects");
    check(!plan(QString(2001, 'x'), context()).value("ok").toBool(), "oversized command rejects");
    auto bad = context(); bad["selection"] = QVariantList{};
    check(!plan("mute", bad).value("ok").toBool(), "missing selection rejects");
    bad = context(); bad["selection"] = QVariantList{QVariantMap{{"kind", "route"}, {"id", "piano"}}, QVariantMap{{"kind", "route"}, {"id", "master"}}};
    check(!plan("mute", bad).value("ok").toBool(), "ambiguous selection rejects");
    bad = context(); bad["selection"] = QVariantList{QVariantMap{{"kind", "route"}, {"id", "missing"}}};
    check(!plan("mute", bad).value("ok").toBool(), "unknown selection rejects");
    bad = context(); auto route = bad.value("selectedRoute").toMap(); route["gainDb"] = std::numeric_limits<double>::infinity(); bad["selectedRoute"] = route;
    check(!plan("set gain to -6 dB", bad).value("ok").toBool(), "non-finite context rejects");
    bad = context(); route = bad.value("selectedRoute").toMap(); route["muted"] = "false"; bad["selectedRoute"] = route;
    check(!plan("mute", bad).value("ok").toBool(), "string boolean context rejects");
    bad = context(); auto routes = bad.value("routes").toList(); routes.append(routes[1]); bad["routes"] = routes;
    check(!plan("route to Music Bus", bad).value("ok").toBool(), "duplicate destination names reject");
    bad = context(); routes = bad.value("routes").toList(); route = routes[1].toMap(); route["outputRouteId"] = "piano"; routes[1] = route; bad["routes"] = routes;
    check(!plan("route to Music Bus", bad).value("ok").toBool(), "feedback route rejects");
    bad = context(); route = bad.value("selectedRoute").toMap(); route["role"] = "master"; bad["selectedRoute"] = route;
    check(!plan("route to Music Bus", bad).value("ok").toBool(), "master rerouting rejects");
    for (const QString &command : {QString("transpose up 3 semitones"), QString("transpose by -3 semitones"), QString("transpose down 12 semitones")}) {
        const auto result = plan(command, midiContext());
        check(result.value("ok").toBool(), "MIDI transpose supports signed directions");
        const auto op = result.value("request").toMap().value("operations").toList().front().toMap();
        const auto before = op.value("expected").toList(), after = op.value("value").toList();
        auto first = before.front().toMap(), shifted = after.front().toMap();
        check(shifted.value("pitch").toInt() == first.value("pitch").toInt() + op.value("semitones").toInt(), "transpose offsets pitches");
        first.remove("pitch"); shifted.remove("pitch");
        check(first == shifted && before.size() == after.size(), "transpose preserves note timing and metadata");
    }
    for (const QString &command : {QString("transpose 1.5 semitones"), QString("transpose -128 semitones"), QString("transpose up -3 semitones"), QString("transpose 127 semitones")})
        check(!plan(command, midiContext()).value("ok").toBool(), "invalid semitones or overflowing pitches reject");
    bad = midiContext(); auto region = bad.value("selectedRegion").toMap(); region["type"] = "audio"; bad["selectedRegion"] = region;
    check(!plan("transpose 3 semitones", bad).value("ok").toBool(), "audio clip rejects MIDI transpose");
    bad = midiContext(); route = bad.value("selectedRoute").toMap(); route["kind"] = "instrument"; bad["selectedRoute"] = route;
    check(plan("transpose 3 semitones", bad).value("ok").toBool(), "instrument track MIDI clip supports transpose");
    bad = context(); route = bad.value("selectedRoute").toMap(); route["kind"] = "instrument"; bad["selectedRoute"] = route;
    check(!plan("transpose 3 semitones", bad).value("ok").toBool(), "instrument classification alone supplies no notes");
    bad = midiContext(); region = bad.value("selectedRegion").toMap(); auto notes = region.value("notes").toList(); auto note = notes.front().toMap(); note["pitch"] = 60.5; notes[0] = note; region["notes"] = notes; bad["selectedRegion"] = region;
    check(!plan("transpose 3 semitones", bad).value("ok").toBool(), "fractional source pitch rejects");
    std::fprintf(stderr, "AI PLANNER %s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
#ifdef AI_PLANNER_CHECK_MAIN
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const int result = runAiPlannerChecks();
    if (!result && argc == 2) {
        QFile output(QString::fromLocal8Bit(argv[1]));
        if (!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument::fromVariant(generatedRequests).toJson()) < 0) return 1;
    }
    return result;
}
#endif
