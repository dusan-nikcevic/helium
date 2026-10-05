// Ardour headers precede Qt headers because Qt defines the emit keyword.
#include "ardour/ardour.h"
#include "enginesession.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/butler.h"
#include "ardour/track.h"
#include "ardour/panner.h"
#include "ardour/meter.h"
#include "temporal/tempo.h"
#include "pbd/event_loop.h"
#include "pbd/receiver.h"
#include "pbd/transmitter.h"
#include "ardourbridge.h"
#include "commandexecutor.h"
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QRegularExpression>
#include <array>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <pthread.h>
#include <thread>

namespace {
class EngineLog : public Receiver {
    void receive(Transmitter::Channel, const char *text) override {
        std::fprintf(stderr, "Ardour: %s\n", text);
    }
};
class QtEngineLoop : public PBD::EventLoop {
    pthread_t owner = pthread_self();
    PBD::RWLock invalidationLock;
    std::mutex mutex;
    std::array<std::function<void()>, 512> calls;
    size_t first = 0, size = 0;
public:
    QtEngineLoop() : PBD::EventLoop("zephyr") {}
    PBD::RWLock &slot_invalidation_rwlock() override { return invalidationLock; }
    bool call_slot(InvalidationRecord *record, const std::function<void()> &function) override {
        // This bridge polls models; queued callbacks are SessionEvent returns with no raw target.
        if (record) return false;
        if (pthread_equal(owner, pthread_self())) { function(); return true; }
        // ponytail: Dummy uses 512 queued callbacks; use PBD per-thread queues before adding hardware backends.
        std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
        if (!lock || size == calls.size()) return false;
        calls[(first + size) % calls.size()] = function;
        ++size;
        return true;
    }
    void drain() {
        for (size_t count = 0; count < calls.size(); ++count) {
            std::function<void()> function;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!size) break;
                function = std::move(calls[first]);
                calls[first] = {};
                first = (first + 1) % calls.size();
                --size;
            }
            function();
        }
    }
};
ArdourBridge *engineOwner = nullptr;
bool engineWasCleaned = false;
bool validName(const QString &name) {
    return !name.trimmed().isEmpty() && name != "." && name != ".." &&
        !name.contains('/') && !name.contains('\\') && !name.contains(QChar::Null) &&
        !name.endsWith(".ardour") && name.size() <= 120;
}
}
struct ArdourBridge::Impl {
    QtEngineLoop loop;
    EngineLog log;
    QTimer timer;
    std::unique_ptr<ARDOUR::Session> session;
    std::unique_ptr<CommandExecutor> commands;
    bool initialized = false;
    std::shared_ptr<ARDOUR::Route> route(const QString &id) const {
        if (session) for (const auto &route : *session->get_routes()) {
            if (QString::fromStdString(route->id().to_s()) == id) return route;
        }
        return {};
    }
};
ArdourBridge::ArdourBridge(QObject *parent) : QObject(parent), m_impl(std::make_unique<Impl>()) {
    m_impl->timer.setInterval(30);
    connect(&m_impl->timer, &QTimer::timeout, this, [this] { m_impl->loop.drain(); if (m_impl->commands) m_impl->commands->poll(); refresh(); });
}
ArdourBridge::~ArdourBridge() {
    m_impl->timer.stop();
    closeSession();
    if (m_impl->initialized) {
        m_impl->loop.drain();
        ARDOUR::cleanup();
        m_impl->loop.drain();
        PBD::EventLoop::set_event_loop_for_thread(nullptr);
        engineOwner = nullptr; engineWasCleaned = true;
    }
}
bool ArdourBridge::reject(const QString &message) {
    m_error = message; Q_EMIT errorChanged(); return false;
}
bool ArdourBridge::accepted() {
    if (!m_error.isEmpty()) { m_error.clear(); Q_EMIT errorChanged(); }
    refresh(); return true;
}
bool ArdourBridge::createSession(const QString &directory, const QString &name) { return load(directory, name, true); }
bool ArdourBridge::openSession(const QString &directory, const QString &name) { return load(directory, name, false); }
bool ArdourBridge::load(const QString &directory, const QString &name, bool create) {
    if (commandBusy()) return reject("Engine transaction is pending.");
    if (!validName(name) || directory.isEmpty() || directory.contains(QChar::Null) || !QFileInfo(directory).isAbsolute())
        return reject("Session requires an absolute directory and a valid snapshot name.");
    const QString path = QDir::cleanPath(directory), state = QDir(path).filePath(name + ".ardour");
    if (create && (QFileInfo::exists(state) || !QDir(path).entryList({"*.ardour"}, QDir::Files).isEmpty()))
        return reject("Session directory already contains an Ardour session.");
    if (!create && (!QFileInfo(state).isFile() || !QFileInfo(state).isReadable()))
        return reject("Session snapshot cannot be read.");
    if (create && QFileInfo::exists(path) && !QFileInfo(path).isDir()) return reject("Session directory is not a folder.");
    if (engineOwner && engineOwner != this) return reject("Another bridge owns the Ardour engine.");
    if (engineWasCleaned) return reject("The Ardour engine has shut down for this process.");
    try {
        if (!m_impl->initialized) {
            m_impl->log.listen_to(PBD::warning); m_impl->log.listen_to(PBD::error); m_impl->log.listen_to(PBD::fatal);
            if (!ARDOUR::init(false, "", false)) return reject("Ardour initialization failed.");
            m_impl->initialized = true; engineOwner = this;
            PBD::EventLoop::set_event_loop_for_thread(&m_impl->loop);
            ARDOUR::SessionEvent::create_per_thread_pool("zephyr", 2048);
        }
        float sampleRate = 48000;
        if (!create) {
            ARDOUR::SampleFormat format;
            std::string version;
            if (ARDOUR::Session::get_info_from_path(state.toStdString(), sampleRate, format, version) ||
                !std::isfinite(sampleRate) || sampleRate <= 0)
                return reject("Session snapshot has no valid sample rate.");
        }
        closeSession();
        auto *engine = ARDOUR::AudioEngine::instance();
        if (!engine->current_backend() && !engine->set_backend("None (Dummy)", "Zephyr", ""))
            return reject("Ardour Dummy backend is unavailable.");
        if (engine->set_sample_rate(sampleRate) || engine->set_buffer_size(1024) || engine->start())
            return reject("Ardour Dummy backend cannot start.");
        ARDOUR::BusProfile buses; buses.master_out_channels = 2;
        m_impl->session = std::make_unique<EngineSession>(*engine, path.toStdString(), name.toStdString(), create ? &buses : nullptr);
        m_impl->commands = std::make_unique<CommandExecutor>(*m_impl->session, m_impl->loop, this);
        connect(m_impl->commands.get(), &CommandExecutor::finished, this, [this](const QVariantMap &result) { refresh(); Q_EMIT commandFinished(result); });
        connect(m_impl->commands.get(), &CommandExecutor::stateChanged, this, &ArdourBridge::changed);
        m_directory = path; m_name = name; m_sampleRate = engine->sample_rate(); m_impl->timer.start();
        return accepted();
    } catch (const std::exception &error) { closeSession(); return reject(QString::fromUtf8(error.what())); }
    catch (...) { closeSession(); return reject("Ardour cannot load this session."); }
}
void ArdourBridge::closeSession() {
    m_impl->timer.stop();
    if (m_impl->initialized) {
        // Stop processing before draining queued callbacks that can retain routes.
        ARDOUR::AudioEngine::instance()->stop();
        if (m_impl->session) m_impl->session->butler()->stop();
        m_impl->loop.drain();
        if (m_impl->commands) m_impl->commands->poll();
        if (m_impl->commands) m_impl->commands->cancel();
        m_impl->commands.reset();
        if (m_impl->session) m_impl->session->DropReferences();
        m_impl->session.reset(); m_impl->loop.drain();
    }
    m_channels.clear(); m_directory.clear(); m_name.clear(); m_transport = 0; m_sampleRate = 0; m_playing = false;
    Q_EMIT changed();
}
bool ArdourBridge::saveSession() {
    if (commandBusy()) return reject("Engine transaction is pending.");
    if (!m_impl->session) return reject("No engine session is open.");
    if (m_impl->session->save_state()) return reject("Ardour cannot save this session.");
    return accepted();
}
bool ArdourBridge::addAudioTrack(const QString &name, int channels) {
    if (commandBusy()) return reject("Engine transaction is pending.");
    if (!m_impl->session) return reject("No engine session is open.");
    if (name.trimmed().isEmpty() || name.contains(QChar::Null) || name.size() > 120 || (channels != 1 && channels != 2))
        return reject("Audio track requires a name and one or two input channels.");
    try {
        const auto tracks = m_impl->session->new_audio_track(channels, 2, {}, 1, name.toStdString(), ARDOUR::PresentationInfo::max_order, ARDOUR::Normal, false);
        if (tracks.empty()) return reject("Ardour cannot create this track.");
        return accepted();
    } catch (const std::exception &error) { return reject(QString::fromUtf8(error.what())); }
}
bool ArdourBridge::setGainDb(const QString &id, double db) {
    if (commandBusy()) return reject("Engine transaction is pending.");
    const auto route = m_impl->route(id);
    if (!route || !std::isfinite(db) || db < -90 || db > 12) return reject("Gain requires a valid route and -90 to 12 dB.");
    if (!route->gain_control()->writable() || route->gain_control()->automation_state() != ARDOUR::Off) return reject("Gain automation must be off for manual edits.");
    route->gain_control()->set_value(std::pow(10.0, db / 20.0), PBD::Controllable::NoGroup); return accepted();
}
bool ArdourBridge::setMuted(const QString &id, bool value) {
    if (commandBusy()) return reject("Engine transaction is pending.");
    const auto route = m_impl->route(id);
    if (!route) return reject("Mute requires a valid route.");
    if (!route->mute_control()->writable() || route->mute_control()->automation_state() != ARDOUR::Off) return reject("Mute automation must be off for manual edits.");
    route->mute_control()->set_value(value ? 1.0 : 0.0, PBD::Controllable::NoGroup); return accepted();
}
bool ArdourBridge::setSoloed(const QString &id, bool value) {
    if (commandBusy()) return reject("Engine transaction is pending.");
    const auto route = m_impl->route(id);
    if (!route || route->is_master()) return reject("Solo requires a track or bus.");
    if (!route->solo_control()->writable() || route->solo_control()->automation_state() != ARDOUR::Off) return reject("Solo automation must be off for manual edits.");
    route->solo_control()->set_value(value ? 1.0 : 0.0, PBD::Controllable::NoGroup); return accepted();
}
bool ArdourBridge::setPan(const QString &id, double value) {
    if (commandBusy()) return reject("Engine transaction is pending.");
    const auto route = m_impl->route(id);
    if (!route || !std::isfinite(value) || value < 0 || value > 1 || route->n_inputs().n_audio() != 1 || !route->panner() || !route->pan_azimuth_control())
        return reject("Pan requires a mono route with a panner and a value between zero and one.");
    if (!route->pan_azimuth_control()->writable() || route->pan_azimuth_control()->automation_state() != ARDOUR::Off) return reject("Pan automation must be off for manual edits.");
    route->pan_azimuth_control()->set_value(value, PBD::Controllable::NoGroup); return accepted();
}
bool ArdourBridge::requestPlayback(bool value) {
    if (!m_impl->session) return reject("No engine session is open.");
    if (value) m_impl->session->request_roll(); else m_impl->session->request_stop();
    return accepted();
}
bool ArdourBridge::seekSamples(qint64 value) {
    if (!m_impl->session || value < 0 || value > qint64(m_sampleRate) * 24 * 60 * 60)
        return reject("Seek requires an open session and a position within 24 hours.");
    m_impl->session->request_locate(value, false, ARDOUR::MustStop); return accepted();
}
bool ArdourBridge::seekBar(double value) {
    if (!m_impl->session || !std::isfinite(value) || value < 1 || value > 100000)
        return reject("Seek requires an open session and a bar between 1 and 100000.");
    const auto map = Temporal::TempoMap::fetch();
    const int bar = int(std::floor(value));
    const Temporal::BBT_Argument start(0, Temporal::BBT_Time(bar, 1, 0));
    const auto &meter = map->meter_at(start);
    const double beats = (value - bar) * meter.divisions_per_bar();
    const int beat = int(std::floor(beats));
    const int ticks = int((beats - beat) * meter.ticks_per_grid());
    return seekSamples(map->sample_at(Temporal::BBT_Argument(0, Temporal::BBT_Time(bar, beat + 1, ticks))));
}
void ArdourBridge::refresh() {
    if (!m_impl->session) return;
    if (m_impl->commands) m_impl->commands->observe();
    PBD::Mutex::Lock lock(ARDOUR::AudioEngine::instance()->process_lock());
    QVariantList channels;
    for (const auto &route : *m_impl->session->get_routes()) {
        const double gain = route->gain_control()->get_value();
        const bool panAvailable = route->n_inputs().n_audio() == 1 && route->panner() && route->pan_azimuth_control();
        const auto track = std::dynamic_pointer_cast<ARDOUR::Track>(route);
        double meter = route->peak_meter()->meter_level(0, ARDOUR::MeterPeak);
        if (!std::isfinite(meter)) meter = -90;
        const auto meters = route->peak_meter()->input_streams().n_audio();
        double right = meters > 1 ? route->peak_meter()->meter_level(1, ARDOUR::MeterPeak) : meter;
        if (!std::isfinite(right)) right = -90;
        channels.append(QVariantMap{{"id", QString::fromStdString(route->id().to_s())}, {"name", QString::fromStdString(route->name())},
            {"gainDb", gain > 0 ? std::max(-120.0, 20.0 * std::log10(gain)) : -120}, {"muted", route->muted_by_self()}, {"soloed", route->self_soloed()},
            {"pan", panAvailable ? route->pan_azimuth_control()->get_value() : 0.5}, {"panAvailable", panAvailable},
            {"isTrack", bool(track)}, {"isMaster", route->is_master()}, {"meterDb", meter}, {"meterLDb", meter}, {"meterRDb", right}, {"meterChannels", meters}, {"clips", QVariantList{}}});
    }
    const auto transport = m_impl->session->transport_sample();
    const bool playing = m_impl->session->transport_rolling();
    const auto map = Temporal::TempoMap::fetch();
    const Temporal::timepos_t position(transport);
    const auto bbt = map->bbt_at(position);
    const auto &meter = map->meter_at(position);
    const double barPosition = bbt.bars + (bbt.beats - 1 + double(bbt.ticks) / meter.ticks_per_grid()) / meter.divisions_per_bar();
    const double bpm = map->tempo_at(position).note_types_per_minute_at_DOUBLE(position);
    const double cpu = ARDOUR::AudioEngine::instance()->get_dsp_load();
    const QString time = QString("%1:%2:%3").arg(bbt.bars).arg(bbt.beats).arg(bbt.ticks, 4, 10, QChar('0'));
    const QString signature = QString("%1/%2").arg(meter.divisions_per_bar()).arg(meter.note_value());
    lock.release();
    if (channels != m_channels || transport != m_transport || playing != m_playing || cpu != m_cpuLoad || signature != m_timeSignature || bpm != m_bpm) {
        m_barPosition = barPosition; m_bpm = bpm; m_beatsPerBar = meter.divisions_per_bar(); m_cpuLoad = cpu; m_timeDisplay = time; m_timeSignature = signature;
        m_channels = channels; m_transport = transport; m_playing = playing; Q_EMIT changed();
    }
}

bool runEngineLoopChecks() {
    QtEngineLoop loop;
    int called = 0;
    bool queued = false;
    std::thread producer([&] { queued = loop.call_slot(nullptr, [&] { ++called; }); });
    producer.join();
    if (!queued || called) return false;
    loop.drain();
    if (called != 1) return false;
    PBD::EventLoop::InvalidationRecord record;
    bool rejected = false;
    std::thread invalidated([&] { rejected = !loop.call_slot(&record, [&] { ++called; }); });
    invalidated.join(); loop.drain();
    if (!rejected || called != 1) return false;
    std::thread full([&] {
        for (int i = 0; i < 512; ++i) if (!loop.call_slot(nullptr, [&] { ++called; })) queued = false;
        if (loop.call_slot(nullptr, [&] { ++called; })) queued = false;
    });
    full.join(); loop.drain();
    return queued && called == 513;
}

QString ArdourBridge::engineSessionId() const { return m_impl->commands ? m_impl->commands->sessionId() : QString(); }
qulonglong ArdourBridge::revision() const { return m_impl->commands ? m_impl->commands->revision() : 0; }
QVariantList ArdourBridge::commandHistory() const { return m_impl->commands ? m_impl->commands->history() : QVariantList(); }
bool ArdourBridge::commandBusy() const { return m_impl->commands && m_impl->commands->busy(); }
bool ArdourBridge::beginGesture() { return m_impl->commands && m_impl->commands->beginGesture(); }
bool ArdourBridge::endGesture() { return m_impl->commands && m_impl->commands->endGesture(); }
bool ArdourBridge::submitCommand(const QVariantMap &request) {
    if (m_impl->commands) return m_impl->commands->submit(request);
    QVariantMap error{{"code", "ENGINE_UNAVAILABLE"}, {"message", "No engine session is open."}, {"recoverable", true}, {"path", ""}};
    auto receiptId = [&](const QString &field, const QString &fallback) {
        const auto value = request.value(field);
        static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$");
        return value.metaType().id() == QMetaType::QString && value.toString().size() <= 128 && pattern.match(value.toString()).hasMatch() ? value.toString() : fallback;
    };
    const QString phase = QStringList{"preview", "apply", "undo"}.contains(request.value("phase").toString()) ? request.value("phase").toString() : "apply";
    Q_EMIT commandFinished(QVariantMap{{"schemaVersion", 1}, {"commandId", receiptId("commandId", "invalid")}, {"sessionId", receiptId("sessionId", "unavailable")}, {"phase", phase}, {"status", "rejected"}, {"revision", 0}, {"stateValid", true}, {"changes", QVariantList{}}, {"error", error}});
    return false;
}
