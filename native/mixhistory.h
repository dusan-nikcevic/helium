#pragma once
#include <QVariantMap>
#include <QVariantList>

class MixHistory {
public:
    static QVariantMap capture(const QVariantMap &project);
    static QVariantList changes(const QVariantMap &before, const QVariantMap &after);
    static bool restore(QVariantMap &project, const QVariantMap &state, QString *error = nullptr);
    QString record(const QVariantMap &before, const QVariantMap &after,
                   const QString &source = "manual", const QString &label = {}, bool merge = false);
    bool markUndone(const QString &id);
    QVariantList entries() const;
    QString saveSnapshot(const QVariantMap &project, const QString &name, QString *error = nullptr);
    QVariantMap snapshotState(const QString &id) const;
    bool restoreSnapshot(const QString &id, QVariantMap &project, QString *error = nullptr) const;
    bool removeSnapshot(const QString &id);
    QVariantList snapshots() const;
    void clear();
private:
    struct Transaction { QVariantMap before, after, entry; };
    QList<Transaction> m_entries;
    QVariantList m_snapshots;
};
