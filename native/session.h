#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantMap>
#ifdef ZEPHYR_ENGINE
class ArdourBridge;
#endif

class Session : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap project READ project NOTIFY changed)
    Q_PROPERTY(QString selectedTrackId READ selectedTrackId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedClipId READ selectedClipId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedTrack READ selectedTrack NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedClip READ selectedClip NOTIFY changed)
    Q_PROPERTY(QString selectionOrigin READ selectionOrigin NOTIFY selectionChanged)
    Q_PROPERTY(QString activeSceneId READ activeSceneId NOTIFY changed)
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(double playheadBar READ playheadBar NOTIFY changed)
    Q_PROPERTY(bool loopEnabled READ loopEnabled NOTIFY changed)
    Q_PROPERTY(bool snapEnabled READ snapEnabled NOTIFY changed)
    Q_PROPERTY(bool automationVisible READ automationVisible NOTIFY changed)
    Q_PROPERTY(QString aiState READ aiState NOTIFY changed)
    Q_PROPERTY(QVariantList aiHistory READ aiHistory NOTIFY changed)
    Q_PROPERTY(QVariantList stagedAiPlan READ stagedAiPlan NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QString requestText READ requestText WRITE setRequestText NOTIFY changed)
    Q_PROPERTY(QString lastCommand READ lastCommand NOTIFY changed)
    Q_PROPERTY(bool engineConnected READ engineConnected NOTIFY changed)
public:
    explicit Session(QObject *parent = nullptr);
    bool loadFixture(const QString &path);
    QVariantMap project() const { return m_project; }
    QString selectedTrackId() const { return m_trackId; }
    QString selectedClipId() const { return m_clipId; }
    QVariantMap selectedTrack() const;
    QVariantMap selectedClip() const;
    QString selectionOrigin() const { return m_origin; }
    QString activeSceneId() const { return m_sceneId; }
    QString view() const { return m_view; }
    bool playing() const { return m_playing; }
    double playheadBar() const { return m_bar; }
    bool loopEnabled() const { return m_loop; }
    bool snapEnabled() const { return m_snap; }
    bool automationVisible() const { return m_automation; }
    QString aiState() const { return m_aiState; }
    QVariantList aiHistory() const { return m_history; }
    QVariantList stagedAiPlan() const { return m_plan; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    QString notice() const { return m_notice; }
    QString requestText() const { return m_request; }
    QString lastCommand() const { return m_lastCommand; }
    bool engineConnected() const { return m_engineConnected; }
#ifdef ZEPHYR_ENGINE
    void attachEngine(ArdourBridge *engine);
#endif
    void setRequestText(const QString &text);
    Q_INVOKABLE void setView(const QString &view);
    Q_INVOKABLE bool selectTrack(const QString &id);
    Q_INVOKABLE bool selectClip(const QString &trackId, const QString &clipId);
    Q_INVOKABLE bool setTrackValue(const QString &id, const QString &key, const QVariant &value);
    Q_INVOKABLE bool moveClip(const QString &trackId, const QString &clipId, double startBar);
    Q_INVOKABLE bool setDeviceParam(const QString &id, const QString &deviceId, const QString &paramId, double value);
    Q_INVOKABLE bool setDeviceEnabled(const QString &id, const QString &deviceId, bool enabled);
    Q_INVOKABLE bool setInsertEnabled(const QString &id, const QString &insertId, bool enabled);
    Q_INVOKABLE bool setSendAmount(const QString &id, const QString &sendId, double amount);
    Q_INVOKABLE bool addDevice(const QString &id, const QString &kind);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool seek(double bar);
    Q_INVOKABLE void toggleLoop();
    Q_INVOKABLE void toggleSnap();
    Q_INVOKABLE void toggleAutomation();
    Q_INVOKABLE bool selectLauncherSlot(const QString &sceneId, const QString &trackId);
    Q_INVOKABLE bool launchScene(const QString &sceneId);
    Q_INVOKABLE bool previewAi();
    Q_INVOKABLE bool applyAi();
    Q_INVOKABLE void discardAi();
    Q_INVOKABLE bool submitAi(const QString &text);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE void beginGesture();
    Q_INVOKABLE void endGesture();
signals:
    void changed();
    void selectionChanged();
    void clipSelected();
    void operationRejected(const QString &message);
private:
    struct Snapshot {
        QVariantMap project;
        QVariantList history, plan;
        QString aiState, lastCommand;
    };
    QVariantMap m_project;
    QVariantList m_history, m_plan;
    QList<Snapshot> m_undo;
    QString m_trackId, m_clipId, m_sceneId, m_origin = "arrangement";
    QString m_view = "arrange", m_aiState = "ready", m_request, m_lastCommand;
    QString m_notice = "Fixture session. No audio engine or AI service is connected.";
    bool m_playing = false, m_loop = true, m_snap = true, m_automation = true;
    bool m_engineConnected = false;
#ifdef ZEPHYR_ENGINE
    ArdourBridge *m_engine = nullptr;
    void refreshEngine();
#endif
    bool m_gestureActive = false, m_gestureSaved = false;
    double m_bar = 17;
    QTimer m_timer;
    QElapsedTimer m_clock;
    bool reject(const QString &message);
    void notify(const QString &message = {});
    Snapshot snapshot() const;
    void commit(const QVariantMap &project);
    static QVariantMap channel(const QVariantMap &project, const QString &id);
    static bool replaceChannel(QVariantMap &project, const QString &id, const QVariantMap &channel);
    bool edit(QVariantMap &project, const QString &id, const QString &kind,
              const QString &itemId, const QString &paramId, const QVariant &value,
              const QString &display = {}, bool ai = false);
    QVariant valueForChange(const QVariantMap &project, const QVariantMap &change) const;
    bool buildAiCandidate(QVariantMap &candidate);
    bool stagePlan(const QVariant &plan);
    double endBar() const;
};
