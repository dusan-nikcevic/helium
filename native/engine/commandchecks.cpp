#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/rc_configuration.h"
#include "ardour/io.h"
#include "ardour/port.h"
#include "ardourbridge.h"
#include <QEventLoop>
#include <QTimer>
#include <QUuid>
#include <cstdio>
#include <cmath>
#include <limits>

void runEngineCommandChecks(ArdourBridge &bridge, const QString &rid, int &checks, int &failures) {
    auto check = [&](bool value, const char *label) { ++checks; if (!value) { ++failures; std::fprintf(stderr,"ENGINE FAIL: command %s\n",label); } };
    QVariantMap result;
    const auto connection = QObject::connect(&bridge, &ArdourBridge::commandFinished, &bridge, [&](const QVariantMap &r) { result = r; });
    const QVariantMap target{{"kind", "route"}, {"id", rid}};
    auto operation = [&](QString control, QVariant expected, QVariant value) { return QVariantMap{{"action", "set_route_control"}, {"target", target}, {"control", control}, {"expected", expected}, {"value", value}}; };
    auto command = [&](QString phase, QVariantList operations = {}) {
        QVariantMap r{{"schemaVersion", 1}, {"commandId", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"sessionId", bridge.engineSessionId()}, {"phase", phase}, {"source", "ai"}, {"expectedRevision", bridge.revision()}, {"selection", QVariantList{target}}, {"groupMode", "independent"}};
        if (phase != "undo") r["operations"] = operations;
        return r;
    };
    auto submit = [&](const QVariantMap &request) {
        result.clear();
        QEventLoop wait;
        const auto completion = QObject::connect(&bridge, &ArdourBridge::commandFinished, &wait, &QEventLoop::quit);
        bridge.submitCommand(request);
        if (result.isEmpty()) { QTimer::singleShot(1500, &wait, &QEventLoop::quit); wait.exec(); }
        QObject::disconnect(completion);
        return result;
    };
    auto rejected = [&](const QVariantMap &r, const QString &code) { return r.value("status") == "rejected" && r.value("error").toMap().value("code") == code && r.value("changes").toList().isEmpty(); };
    auto value = [&](QString control) {
        for (const auto &entry : bridge.channels()) if (entry.toMap().value("id") == rid) return entry.toMap().value(control == "panPosition" ? "pan" : control);
        return QVariant();
    };
    const auto initialRevision = bridge.revision();
    auto request = command("preview", {operation("gainDb", -6.0, -9.0), operation("muted", true, false), operation("soloed", true, false), operation("panPosition", 0.25, 0.4)});
    auto preview = submit(request);
    check(preview.value("status") == "previewed" && preview.value("changes").toList().size() == 4, "preview measured diffs");
    check(bridge.revision() == initialRevision && bridge.commandHistory().isEmpty() && std::abs(value("gainDb").toDouble() + 6) < .0001, "preview preserves state and history");
    auto invalid = request; invalid["unknown"] = true;
    check(rejected(submit(invalid), "INVALID_COMMAND"), "unknown envelope rejected");
    invalid = request; invalid["source"] = "system";
    check(rejected(submit(invalid), "INVALID_COMMAND"), "source rejected");
    invalid = request; invalid["schemaVersion"] = 2;
    check(rejected(submit(invalid), "UNSUPPORTED_VERSION"), "version rejected");
    invalid = request; invalid["groupMode"] = "linked";
    check(rejected(submit(invalid), "INVALID_COMMAND"), "group mode rejected");
    invalid = request; invalid["expectedRevision"] = 0.5;
    check(rejected(submit(invalid), "INVALID_COMMAND"), "fractional revision rejected");
    invalid = request; invalid["sessionId"] = "another";
    check(rejected(submit(invalid), "UNKNOWN_SESSION"), "session mismatch rejected");
    invalid = request; invalid["selection"] = QVariantList{target,target};
    check(rejected(submit(invalid), "INVALID_COMMAND"), "duplicate selection rejected");
    invalid = request; invalid["operations"] = QVariantList{operation("gainDb", -6., std::numeric_limits<double>::quiet_NaN())};
    check(rejected(submit(invalid), "INVALID_COMMAND"), "nonfinite value rejected");
    invalid = request; invalid["operations"] = QVariantList{operation("gainDb", -6., 13.)};
    check(rejected(submit(invalid), "VALUE_OUT_OF_RANGE"), "range rejected");
    invalid = request; invalid["operations"] = QVariantList{operation("gainDb", -6., -9.), operation("gainDb", -6., -12.)};
    check(rejected(submit(invalid), "INVALID_COMMAND"), "duplicate writes rejected");
    invalid = request; invalid["operations"] = QVariantList{operation("gainDb", -5., -9.)};
    check(rejected(submit(invalid), "CONFLICT"), "prior value rejected");
    invalid = request; auto extra = operation("muted", true, false); extra["unit"] = "unitless"; invalid["operations"] = QVariantList{extra};
    check(rejected(submit(invalid), "INVALID_COMMAND"), "unknown control fields rejected");
    invalid = request; extra = operation("recordArmed", false, true); invalid["operations"] = QVariantList{extra};
    check(rejected(submit(invalid), "UNSUPPORTED_OPERATION"), "unadvertised control rejected");
    invalid = command("apply", {operation("gainDb", -6., -9.), operation("panPosition", 0.25, 2.)});
    check(rejected(submit(invalid), "VALUE_OUT_OF_RANGE") && std::abs(value("gainDb").toDouble() + 6) < .0001, "whole batch validates before mutation");
    request["phase"] = "apply";
    const auto applied = submit(request);
    check(applied.value("status") == "applied" && applied.value("changes").toList().size() == 4 && !bridge.commandBusy(), "native atomic apply completes");
    // The bridge publishes controls after command completion; read engine controls directly here.
    {
        const auto route = ARDOUR::AudioEngine::instance()->session()->route_by_id(PBD::ID(rid.toStdString()));
        check(std::abs(20 * std::log10(route->gain_control()->get_value()) + 9) < .001 && !route->muted_by_self() && !route->self_soloed() && !ARDOUR::AudioEngine::instance()->session()->soloing(), "actual controls and global solo complete");
    }
    check(bridge.revision() == initialRevision + 1 && bridge.commandHistory().size() == 1, "apply one revision and history step");
    check(submit(request) == applied && bridge.revision() == initialRevision + 1, "exact apply retry receipt");
    auto altered = request; altered["label"] = "different";
    check(rejected(submit(altered), "IDEMPOTENCY_CONFLICT"), "retry payload conflict");
    check(rejected(submit(command("preview", {operation("gainDb", -6., -12.)})), "CONFLICT"), "stale prior after apply rejected");
    auto undo = command("undo"); undo["transactionId"] = applied.value("transactionId");
    const auto undone = submit(undo);
    check(undone.value("status") == "undone" && std::abs(undone.value("changes").toList().first().toMap().value("before").toDouble() + 9) < .0001 && std::abs(undone.value("changes").toList().first().toMap().value("after").toDouble() + 6) < .0001, "undo actual inverse diff");
    check(bridge.commandHistory().last().toMap().value("undone").toBool() && submit(undo) == undone, "undo receipt and history");
    {
        const bool exclusive = ARDOUR::Config->get_exclusive_solo(), listen = ARDOUR::Config->get_solo_control_is_listen_control();
        ARDOUR::Config->set_exclusive_solo(true);
        auto restricted = command("apply", {operation("soloed", true, false)});
        submit(restricted); restricted["commandId"] = QUuid::createUuid().toString(QUuid::WithoutBraces); restricted["expectedRevision"] = bridge.revision();
        check(rejected(submit(restricted), "UNSUPPORTED_OPERATION"), "exclusive solo rejects before mutation");
        ARDOUR::Config->set_exclusive_solo(false); ARDOUR::Config->set_solo_control_is_listen_control(true);
        restricted = command("apply", {operation("soloed", true, false)});
        submit(restricted); restricted["commandId"] = QUuid::createUuid().toString(QUuid::WithoutBraces); restricted["expectedRevision"] = bridge.revision();
        check(rejected(submit(restricted), "UNSUPPORTED_OPERATION"), "listen mode rejects before mutation");
        ARDOUR::Config->set_exclusive_solo(exclusive); ARDOUR::Config->set_solo_control_is_listen_control(listen);
        QEventLoop wait; QTimer::singleShot(90, &wait, &QEventLoop::quit); wait.exec();
    }
    {
        auto *session = ARDOUR::AudioEngine::instance()->session();
        auto buses = session->new_audio_route(2, 2, {}, 1, "Solo check bus", ARDOUR::PresentationInfo::AudioBus, ARDOUR::PresentationInfo::max_order);
        if (!buses.empty()) {
            auto bus = buses.front();
            const auto track = session->route_by_id(PBD::ID(rid.toStdString()));
            track->output()->disconnect();
            track->output()->connect(track->output()->nth(0), bus->input()->nth(0)->name());
            track->output()->connect(track->output()->nth(1), bus->input()->nth(1)->name());
            bridge.setSoloed(rid,false);
            QEventLoop wait; QTimer::singleShot(180, &wait, &QEventLoop::quit); wait.exec();
            bridge.setSoloed(rid,true);
            QEventLoop propagation; QTimer::singleShot(180, &propagation, &QEventLoop::quit); propagation.exec();
            check(bus->solo_control()->get_value() == 1 && !bus->self_soloed(), "routed bus has effective solo without self solo");
            auto busOperation = operation("soloed", false, true);
            const QVariantMap busTarget{{"kind", "route"}, {"id", QString::fromStdString(bus->id().to_s())}};
            busOperation["target"] = busTarget;
            const auto busResult = submit(command("apply", {busOperation}));
            check(busResult.value("status") == "applied" && bus->self_soloed(), "effective solo uses expected self state");
            undo = command("undo"); undo["transactionId"] = busResult.value("transactionId");
            check(submit(undo).value("status") == "undone" && !bus->self_soloed() && bus->solo_control()->get_value() == 1, "bus inverse restores self state with effective solo intact");
            session->remove_route(bus);
            track->output()->connect(track->output()->nth(0), session->master_out()->input()->nth(0)->name());
            track->output()->connect(track->output()->nth(1), session->master_out()->input()->nth(1)->name());
            QEventLoop deletion; QTimer::singleShot(90, &deletion, &QEventLoop::quit); deletion.exec();
        } else check(false, "create routed bus for self-solo regression");
    }
    const int auditBeforeGesture = bridge.commandHistory().size();
    check(bridge.beginGesture(), "begin gesture");
    auto first = submit(command("apply", {operation("gainDb", -6., -7.)}));
    auto second = submit(command("apply", {operation("gainDb", -7., -8.)}));
    check(first.value("status") == "applied" && second.value("status") == "applied" && first.value("transactionId") == second.value("transactionId") && bridge.commandHistory().size() == auditBeforeGesture + 1, "gesture updates one history step");
    check(bridge.endGesture(), "end gesture");
    undo = command("undo"); undo["transactionId"] = second.value("transactionId");
    check(submit(undo).value("status") == "undone", "gesture undo");
    auto floor = submit(command("apply", {operation("gainDb", -6., -120.)}));
    check(floor.value("status") == "applied" && floor.value("changes").toList().first().toMap().value("after").toDouble() == -120, "gain floor writes silence");
    undo = command("undo"); undo["transactionId"] = floor.value("transactionId"); check(submit(undo).value("status") == "undone", "silence inverse");
    const auto staged = command("preview", {operation("gainDb", -6., -10.)});
    const auto revision = bridge.revision();
    bridge.setGainDb(rid,-5.);
    check(bridge.revision() > revision && !bridge.commandHistory().last().toMap().value("undoable").toBool(), "external edits invalidate history and revision");
    check(rejected(submit(staged), "CONFLICT"), "staged preview invalidated");
    bridge.setGainDb(rid,-6.);
    {
        const auto route = ARDOUR::AudioEngine::instance()->session()->route_by_id(PBD::ID(rid.toStdString()));
        route->gain_control()->set_automation_state(ARDOUR::Play);
        auto automated = command("apply", {operation("gainDb", -6., -8.)});
        submit(automated);
        // Observing the mode first advances the revision. Retry with that current revision.
        automated["commandId"] = QUuid::createUuid().toString(QUuid::WithoutBraces); automated["expectedRevision"] = bridge.revision();
        check(rejected(submit(automated), "AUTOMATION_CONFLICT"), "automation mode rejected");
        route->gain_control()->set_automation_state(ARDOUR::Off);
    }
    // A native solo-safe route refuses the second write. The first gain write must roll back.
    {
        const auto route = ARDOUR::AudioEngine::instance()->session()->route_by_id(PBD::ID(rid.toStdString()));
        route->solo_safe_control()->set_value(1., PBD::Controllable::NoGroup);
        QEventLoop wait; QTimer::singleShot(90, &wait, &QEventLoop::quit); wait.exec();
        const auto revisionBefore = bridge.revision();
        const auto rollback = submit(command("apply", {operation("gainDb", -6., -10.), operation("soloed", true, false)}));
        check(rejected(rollback, "ENGINE_FAILURE") && rollback.value("stateValid").toBool(), "native refused write reports restored state");
        check(std::abs(20 * std::log10(route->gain_control()->get_value()) + 6) < .0001 && route->self_soloed() && bridge.revision() == revisionBefore, "failed transaction rolls back actual gain and preserves revision");
        route->solo_safe_control()->set_value(0., PBD::Controllable::NoGroup);
    }
    QObject::disconnect(connection);
}

void runEngineCommandFailureChecks(ArdourBridge &bridge, const QString &rid, int &checks, int &failures) {
    auto check = [&](bool value, const char *label) { ++checks; if (!value) { ++failures; std::fprintf(stderr,"ENGINE FAIL: command %s\n",label); } };
    QVariantMap result;
    const auto connection = QObject::connect(&bridge, &ArdourBridge::commandFinished, &bridge, [&](const QVariantMap &r) { result = r; });
    const QVariantMap target{{"kind", "route"}, {"id", rid}};
    auto request = [&](const QVariantList &operations) { return QVariantMap{{"schemaVersion", 1}, {"commandId", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"sessionId", bridge.engineSessionId()}, {"phase", "apply"}, {"source", "ui"}, {"expectedRevision", bridge.revision()}, {"selection", QVariantList{target}}, {"groupMode", "independent"}, {"operations", operations}}; };
    auto operation = [&](const QString &control, const QVariant &expected, const QVariant &value) { return QVariantMap{{"action", "set_route_control"}, {"target", target}, {"control", control}, {"expected", expected}, {"value", value}}; };
    const QString directory = bridge.sessionDirectory(), name = bridge.sessionName();
    {
        const auto route = ARDOUR::AudioEngine::instance()->session()->route_by_id(PBD::ID(rid.toStdString()));
        route->solo_safe_control()->set_value(1., PBD::Controllable::NoGroup);
        QEventLoop safeWait; QTimer::singleShot(90, &safeWait, &QEventLoop::quit); safeWait.exec();
        PBD::ScopedConnection trigger;
        // Change native automation during the first write so its inverse becomes unwritable.
        route->gain_control()->Changed.connect_same_thread(trigger, [route](bool, PBD::Controllable::GroupControlDisposition) { route->gain_control()->set_automation_state(ARDOUR::Play); });
        result.clear(); bridge.submitCommand(request({operation("gainDb", -6., -10.), operation("soloed", true, false)}));
        QEventLoop wait; QTimer::singleShot(150, &wait, &QEventLoop::quit); wait.exec();
        trigger.disconnect();
        check(result.value("status") == "rejected" && result.value("error").toMap().value("code") == "ENGINE_FAILURE" && !result.value("stateValid").toBool(), "unwritable inverse reports invalid engine state");
        result.clear(); bridge.submitCommand(request({operation("gainDb", -10., -12.)}));
        check(result.value("error").toMap().value("code") == "ENGINE_FAILURE" && !result.value("stateValid").toBool(), "invalid state blocks further commands");
    }
    check(bridge.openSession(directory,name), "reload clears invalid transaction state");
    ARDOUR::AudioEngine::instance()->stop();
    result.clear(); bridge.submitCommand(request({operation("gainDb", -6., -8.)}));
    QEventLoop timeout; QTimer::singleShot(2200, &timeout, &QEventLoop::quit); timeout.exec();
    check(result.value("error").toMap().value("code") == "ENGINE_FAILURE" && !result.value("stateValid").toBool() && !bridge.commandBusy(), "stopped engine completes timeout with invalid state");
    check(bridge.openSession(directory,name), "reload after timeout");
    QObject::disconnect(connection);
}
