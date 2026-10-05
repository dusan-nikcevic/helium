#include "mixhistory.h"
#include <QDateTime>
#include <QUuid>
#include <QStringList>
#include <QJsonDocument>

namespace {
const QStringList lists{"tracks", "buses", "returns"};
const QStringList fields{"volumeDb", "pan", "muted", "soloed", "input", "output", "routing", "busId", "outputRouteId", "routeLabel", "devices", "inserts", "sends"};
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
QVariantMap byId(const QVariantList &list) {
    QVariantMap result;
    for (const auto &value : list) {
        const auto item = value.toMap();
        result.insert(item.value("id").toString(), item);
    }
    return result;
}
QStringList unionKeys(const QVariantMap &a, const QVariantMap &b) {
    auto keys = a.keys();
    for (const auto &key : b.keys()) if (!a.contains(key)) keys.append(key);
    return keys;
}
QVariant clean(const QVariant &value) {
    if (value.typeId() == QMetaType::QVariantMap) {
        auto map = value.toMap();
        map.remove("aiEdited");
        for (auto i = map.begin(); i != map.end(); ++i) i.value() = clean(i.value());
        return map;
    }
    if (value.typeId() == QMetaType::QVariantList) {
        auto list = value.toList();
        for (auto &item : list) item = clean(item);
        return list;
    }
    return value;
}
QString display(const QVariant &value, const QString &field) {
    if (!value.isValid()) return "Absent";
    if (value.typeId() == QMetaType::Bool) return value.toBool() ? "On" : "Off";
    if (field == "volumeDb") return QString::number(value.toDouble(), 'f', 1) + " dB";
    if (field == "pan") return QString::number(value.toDouble(), 'f', 2);
    if (field == "amount") return QString::number(value.toDouble(), 'f', 1) + "%";
    if (value.typeId() == QMetaType::QStringList) return value.toStringList().join(", ");
    if (value.typeId() == QMetaType::QVariantList || value.typeId() == QMetaType::QVariantMap)
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    return value.toString();
}
void add(QVariantList &result, const QString &channelId, const QString &channelName,
         const QString &kind, const QString &itemId, const QString &paramId,
         const QString &field, const QVariant &before, const QVariant &after,
         const QString &label = {}, const QString &beforeDisplay = {}, const QString &afterDisplay = {}) {
    if (before == after) return;
    result.append(QVariantMap{{"channelId", channelId}, {"channelName", channelName},
        {"kind", kind}, {"itemId", itemId}, {"paramId", paramId}, {"field", field},
        {"label", label.isEmpty() ? field : label}, {"before", before}, {"after", after},
        {"beforeDisplay", beforeDisplay.isEmpty() ? display(before, field) : beforeDisplay},
        {"afterDisplay", afterDisplay.isEmpty() ? display(after, field) : afterDisplay}});
}
QVariantMap clipItems(const QVariantMap &project) {
    QVariantMap result;
    const auto channels = byId(project.value("tracks").toList());
    auto append = [&](const QVariantMap &item, const QString &channelId, const QString &kind, const QString &prefix) {
        const QString id = item.value("id", channelId).toString();
        QVariantMap values;
        for (const QString &key : {QString("notes"), QString("startBar"), QString("lengthBars"), QString("sourceId"), QString("clipId")})
            if (item.contains(key)) values[key] = clean(item.value(key));
        result[prefix + "/" + id] = QVariantMap{{"channelId", channelId},
            {"channelName", channels.value(channelId).toMap().value("name", channelId)},
            {"kind", kind}, {"itemId", id}, {"values", values}};
    };
    for (auto channel = channels.begin(); channel != channels.end(); ++channel)
        for (const auto &clip : channel.value().toMap().value("clips").toList())
            append(clip.toMap(), channel.key(), "clip", "clip/" + channel.key());
    for (const auto &source : project.value("clipSources").toList())
        append(source.toMap(), source.toMap().value("trackId").toString(), "clipSource", "source");
    for (const auto &scene : project.value("launcherScenes").toList())
        for (const auto &slot : scene.toMap().value("slots").toList())
            append(slot.toMap(), slot.toMap().value("trackId").toString(), "launcher", "scene/" + scene.toMap().value("id").toString());
    return result;
}
}

QVariantMap MixHistory::capture(const QVariantMap &project) {
    QVariantMap result;
    auto append = [&](const QVariantMap &channel) {
        const QString id = channel.value("id").toString();
        if (id.isEmpty()) return;
        QVariantMap state{{"name", channel.value("name", id)}};
        for (const auto &field : fields) if (channel.contains(field)) state[field] = clean(channel.value(field));
        result[id] = state;
    };
    for (const auto &key : lists) for (const auto &channel : project.value(key).toList()) append(channel.toMap());
    append(project.value("master").toMap());
    return result;
}

QVariantList MixHistory::changes(const QVariantMap &before, const QVariantMap &after) {
    const auto a = capture(before), b = capture(after);
    QVariantList result;
    for (const auto &id : unionKeys(a, b)) {
        const auto left = a.value(id).toMap(), right = b.value(id).toMap();
        const QString name = right.value("name", left.value("name", id)).toString();
        if (!a.contains(id) || !b.contains(id)) {
            add(result, id, name, "channel", id, {}, "presence", a.value(id), b.value(id), "Channel");
            continue;
        }
        for (const auto &field : fields) {
            if (field == "devices" || field == "inserts" || field == "sends") continue;
            const QMap<QString, QString> labels{{"volumeDb", "Gain"}, {"pan", "Pan"}, {"muted", "Mute"},
                {"soloed", "Solo"}, {"input", "Input"}, {"output", "Output"}, {"routing", "Routing"}, {"busId", "Bus"}, {"outputRouteId", "Output route"}, {"routeLabel", "Route label"}};
            add(result, id, name, "track", field, {}, field, left.value(field), right.value(field), labels.value(field));
        }
        const auto oldDevices = byId(left.value("devices").toList());
        const auto newDevices = byId(right.value("devices").toList());
        for (const QString &key : {QString("devices"), QString("inserts"), QString("sends")}) {
            const auto oldItems = byId(left.value(key).toList()), newItems = byId(right.value(key).toList());
            const QString kind = key == "devices" ? "device" : key == "inserts" ? "insert" : "send";
            QStringList oldOrder, newOrder;
            for (const auto &v : left.value(key).toList()) oldOrder.append(v.toMap().value("id").toString());
            for (const auto &v : right.value(key).toList()) newOrder.append(v.toMap().value("id").toString());
            if (oldItems.keys() == newItems.keys())
                add(result, id, name, kind, {}, {}, "order", oldOrder, newOrder, key + " order");
            for (const auto &itemId : unionKeys(oldItems, newItems)) {
                const auto oldItem = oldItems.value(itemId).toMap(), newItem = newItems.value(itemId).toMap();
                const QString label = newItem.value("name", oldItem.value("name", itemId)).toString();
                if (!oldItems.contains(itemId) || !newItems.contains(itemId)) {
                    // Inserts mirror devices in the fixture model.
                    if (key == "inserts" && (oldDevices.contains(oldItem.value("deviceId", itemId).toString()) ||
                                              newDevices.contains(newItem.value("deviceId", itemId).toString()))) continue;
                    add(result, id, name, kind, itemId, {}, "presence", oldItems.value(itemId), newItems.value(itemId), label);
                    continue;
                }
                bool mirrored = false;
                if (key == "inserts") {
                    const QString deviceId = newItem.value("deviceId", itemId).toString();
                    mirrored = oldDevices.contains(deviceId) && newDevices.contains(deviceId) &&
                        oldDevices.value(deviceId).toMap().value("enabled") != newDevices.value(deviceId).toMap().value("enabled");
                }
                if (!mirrored) add(result, id, name, kind, itemId, {}, "enabled", oldItem.value("enabled"), newItem.value("enabled"), label + " enabled");
                for (const QString &field : {QString("amount"), QString("targetId"), QString("output"), QString("routing")})
                    add(result, id, name, kind, itemId, {}, field, oldItem.value(field), newItem.value(field), label + " " + field);
                if (key != "devices") continue;
                const auto oldParams = byId(oldItem.value("params").toList()), newParams = byId(newItem.value("params").toList());
                for (const auto &paramId : unionKeys(oldParams, newParams)) {
                    const auto oldParam = oldParams.value(paramId).toMap(), newParam = newParams.value(paramId).toMap();
                    add(result, id, name, "param", itemId, paramId, "value", oldParam.value("value"), newParam.value("value"),
                        label + " / " + newParam.value("label", oldParam.value("label", paramId)).toString(),
                        oldParam.value("display").toString(), newParam.value("display").toString());
                }
            }
        }
    }
    const auto oldClips = clipItems(before), newClips = clipItems(after);
    for (const auto &key : unionKeys(oldClips, newClips)) {
        const auto oldItem = oldClips.value(key).toMap(), newItem = newClips.value(key).toMap();
        const auto item = newClips.contains(key) ? newItem : oldItem;
        const auto left = oldItem.value("values").toMap(), right = newItem.value("values").toMap();
        const QString channelId = item.value("channelId").toString(), name = item.value("channelName").toString();
        const QString kind = item.value("kind").toString(), id = item.value("itemId").toString();
        if (!oldClips.contains(key) || !newClips.contains(key)) {
            add(result, channelId, name, kind, id, {}, "presence",
                oldClips.contains(key) ? QVariant(left) : QVariant(), newClips.contains(key) ? QVariant(right) : QVariant(), kind + " " + id);
            continue;
        }
        for (const auto &field : unionKeys(left, right))
            add(result, channelId, name, kind, id, {}, field, left.value(field), right.value(field), kind + " " + id + " / " + field);
    }
    return result;
}

bool MixHistory::restore(QVariantMap &project, const QVariantMap &state, QString *error) {
    if (state.isEmpty()) return fail(error, "The mix snapshot has no channels.");
    const auto current = capture(project);
    for (auto i = state.begin(); i != state.end(); ++i)
        if (!current.contains(i.key())) return fail(error, "Snapshot channel is unavailable: " + i.key());
    auto candidate = project;
    auto apply = [&](QVariantMap channel) {
        const QString id = channel.value("id").toString();
        if (!state.contains(id)) return channel;
        const auto saved = state.value(id).toMap();
        for (const auto &field : fields) {
            if (saved.contains(field)) channel[field] = saved.value(field);
            else channel.remove(field);
        }
        channel.remove("aiEdited");
        return channel;
    };
    for (const auto &key : lists) {
        if (!candidate.contains(key)) continue;
        auto channels = candidate.value(key).toList();
        for (auto &channel : channels) channel = apply(channel.toMap());
        candidate[key] = channels;
    }
    if (candidate.contains("master")) candidate["master"] = apply(candidate.value("master").toMap());
    project = candidate;
    if (error) error->clear();
    return true;
}

QString MixHistory::record(const QVariantMap &before, const QVariantMap &after,
                          const QString &source, const QString &label, bool merge) {
    if (changes(before, after).isEmpty()) return {};
    if (merge && !m_entries.isEmpty() && !m_entries.last().entry.value("undone").toBool() &&
        m_entries.last().entry.value("source") == source && changes(m_entries.last().after, before).isEmpty()) {
        auto &entry = m_entries.last();
        entry.after = after;
        entry.entry["changes"] = changes(entry.before, after);
        return entry.entry.value("id").toString();
    }
    const QString id = newId();
    // ponytail: retain 64 transactions in memory; persist history if session documents need an audit log.
    if (m_entries.size() == 64) m_entries.removeFirst();
    m_entries.append({before, after, QVariantMap{{"id", id}, {"source", source},
        {"label", label.isEmpty() ? source == "ai" ? "Assistant mix edits" : "Manual mix edit" : label},
        {"timestamp", now()}, {"undone", false}, {"changes", changes(before, after)}}});
    return id;
}

bool MixHistory::markUndone(const QString &id) {
    for (auto &transaction : m_entries) if (transaction.entry.value("id") == id) {
        transaction.entry["undone"] = true;
        return true;
    }
    return false;
}

QVariantList MixHistory::entries() const {
    QVariantList result;
    for (auto i = m_entries.crbegin(); i != m_entries.crend(); ++i) result.append(i->entry);
    return result;
}

QString MixHistory::saveSnapshot(const QVariantMap &project, const QString &name, QString *error) {
    const QString title = name.trimmed();
    if (title.isEmpty() || title.size() > 80) { fail(error, "Name the snapshot using 1 to 80 characters."); return {}; }
    const auto state = capture(project);
    if (state.isEmpty()) { fail(error, "The mixer has no channels to save."); return {}; }
    const QString id = newId();
    m_snapshots.prepend(QVariantMap{{"id", id}, {"name", title}, {"timestamp", now()}, {"state", state}});
    if (error) error->clear();
    return id;
}

QVariantMap MixHistory::snapshotState(const QString &id) const {
    for (const auto &snapshot : m_snapshots) if (snapshot.toMap().value("id") == id)
        return snapshot.toMap().value("state").toMap();
    return {};
}

bool MixHistory::restoreSnapshot(const QString &id, QVariantMap &project, QString *error) const {
    const auto state = snapshotState(id);
    if (state.isEmpty()) return fail(error, "Unknown mix snapshot.");
    return restore(project, state, error);
}

bool MixHistory::removeSnapshot(const QString &id) {
    for (qsizetype i = 0; i < m_snapshots.size(); ++i) if (m_snapshots[i].toMap().value("id") == id) {
        m_snapshots.removeAt(i);
        return true;
    }
    return false;
}

QVariantList MixHistory::snapshots() const {
    auto result = m_snapshots;
    for (auto &snapshot : result) {
        auto map = snapshot.toMap();
        map["channelCount"] = map.value("state").toMap().size();
        map.remove("state");
        snapshot = map;
    }
    return result;
}

void MixHistory::clear() { m_entries.clear(); m_snapshots.clear(); }
