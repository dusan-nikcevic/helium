#pragma once
#include <QVariantMap>
namespace FixtureCommands {
bool validate(const QVariantMap &request, QVariantMap *error);
QVariantMap route(const QVariantMap &project, const QString &id);
QString outputRouteId(const QVariantMap &project, const QVariantMap &route);
bool candidate(const QVariantMap &project, const QVariantMap &request, QVariantMap *candidate,
               QVariantList *changes, QVariantMap *error);
QVariantMap result(const QVariantMap &request, quint64 revision, const QString &status,
                   const QVariantList &changes = {}, const QVariantMap &error = {},
                   const QString &transactionId = {});
} // namespace FixtureCommands
