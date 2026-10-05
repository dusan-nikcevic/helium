#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>
class ArdourBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(qint64 transportSamples READ transportSamples NOTIFY changed)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY changed)
    Q_PROPERTY(QString sessionDirectory READ sessionDirectory NOTIFY changed)
    Q_PROPERTY(QString sessionName READ sessionName NOTIFY changed)
    Q_PROPERTY(double barPosition READ barPosition NOTIFY changed)
    Q_PROPERTY(double bpm READ bpm NOTIFY changed)
    Q_PROPERTY(int beatsPerBar READ beatsPerBar NOTIFY changed)
    Q_PROPERTY(double cpuLoad READ cpuLoad NOTIFY changed)
    Q_PROPERTY(QString timeDisplay READ timeDisplay NOTIFY changed)
    Q_PROPERTY(QString bbtLabel READ bbtLabel NOTIFY changed)
    Q_PROPERTY(QString timeSignature READ timeSignature NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit ArdourBridge(QObject *parent = nullptr);
    ~ArdourBridge() override;
    QVariantList channels() const { return m_channels; }
    bool playing() const { return m_playing; }
    qint64 transportSamples() const { return m_transport; }
    int sampleRate() const { return m_sampleRate; }
    QString sessionDirectory() const { return m_directory; }
    QString sessionName() const { return m_name; }
    QString error() const { return m_error; }
    double barPosition() const { return m_barPosition; }
    double bpm() const { return m_bpm; }
    int beatsPerBar() const { return m_beatsPerBar; }
    double cpuLoad() const { return m_cpuLoad; }
    QString timeDisplay() const { return m_timeDisplay; }
    QString bbtLabel() const { return m_timeDisplay; }
    QString timeSignature() const { return m_timeSignature; }
    Q_INVOKABLE bool createSession(const QString &directory, const QString &name);
    Q_INVOKABLE bool openSession(const QString &directory, const QString &name);
    Q_INVOKABLE bool saveSession();
    Q_INVOKABLE void closeSession();
    Q_INVOKABLE bool addAudioTrack(const QString &name, int channels = 1);
    Q_INVOKABLE bool setGainDb(const QString &routeId, double db);
    Q_INVOKABLE bool setMuted(const QString &routeId, bool value);
    Q_INVOKABLE bool setSoloed(const QString &routeId, bool value);
    Q_INVOKABLE bool setPan(const QString &routeId, double value);
    Q_INVOKABLE bool requestPlayback(bool value);
    Q_INVOKABLE bool seekSamples(qint64 value);
    Q_INVOKABLE bool seekBar(double value);
Q_SIGNALS:
    void changed();
    void errorChanged();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    QVariantList m_channels;
    QString m_directory, m_name, m_error;
    bool m_playing = false;
    qint64 m_transport = 0;
    int m_sampleRate = 0, m_beatsPerBar = 4;
    double m_barPosition = 1, m_bpm = 120, m_cpuLoad = 0;
    QString m_timeDisplay = "1:1:0000", m_timeSignature = "4/4";
    bool load(const QString &, const QString &, bool create);
    bool reject(const QString &);
    bool accepted();
    void refresh();
};
