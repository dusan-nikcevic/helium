#pragma once
#include <QObject>
#include <QVariantMap>
#include <memory>
namespace ARDOUR { class Session; }
namespace PBD { class EventLoop; }
class CommandExecutor : public QObject {
    Q_OBJECT
public:
    CommandExecutor(ARDOUR::Session &, PBD::EventLoop &, QObject *parent);
    ~CommandExecutor() override;
    QString sessionId() const;
    qulonglong revision() const;
    QVariantList history() const;
    bool busy() const;
    bool submit(const QVariantMap &);
    void poll();
    void observe();
    void cancel();
    bool beginGesture();
    bool endGesture();
Q_SIGNALS:
    void finished(const QVariantMap &);
    void stateChanged();
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
