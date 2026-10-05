#pragma once
#include <QString>
#include <QVariantMap>

// Reads resolved canonical values. The command executor must preview the returned request.
QVariantMap planAiCommand(const QString &text, const QVariantMap &context);
