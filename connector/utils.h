#ifndef DATA_UTILS_H
#define DATA_UTILS_H

#include "./global.h"

#include <c++utilities/conversion/types.h>
#include <c++utilities/misc/traits.h>

#include <QJsonValue>
#include <QStringList>

#include <limits>
#include <vector>

QT_FORWARD_DECLARE_CLASS(QUrl)
QT_FORWARD_DECLARE_CLASS(QJsonObject)

namespace ChronoUtilities {
class DateTime;
}

namespace Data {

struct SyncthingDir;

QString LIB_SYNCTHING_CONNECTOR_EXPORT agoString(ChronoUtilities::DateTime dateTime);
QString LIB_SYNCTHING_CONNECTOR_EXPORT trafficString(uint64 total, double rate);
QString LIB_SYNCTHING_CONNECTOR_EXPORT directoryStatusString(quint64 files, quint64 dirs, quint64 size);
QString LIB_SYNCTHING_CONNECTOR_EXPORT syncCompleteString(const std::vector<SyncthingDir *> &completedDirs);
bool LIB_SYNCTHING_CONNECTOR_EXPORT isLocal(const QUrl &url);
bool LIB_SYNCTHING_CONNECTOR_EXPORT setDirectoriesPaused(QJsonObject &syncthingConfig, const QStringList &dirIds, bool paused);
bool LIB_SYNCTHING_CONNECTOR_EXPORT setDevicesPaused(QJsonObject &syncthingConfig, const QStringList &dirs, bool paused);

template <typename IntType = quint64, Traits::EnableIf<std::is_integral<IntType>>...>
inline IntType LIB_SYNCTHING_CONNECTOR_EXPORT jsonValueToInt(const QJsonValue &value, double defaultValue = 0.0)
{
    return static_cast<IntType>(value.toDouble(defaultValue));
}

constexpr int LIB_SYNCTHING_CONNECTOR_EXPORT trQuandity(quint64 quandity)
{
    return quandity > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : static_cast<int>(quandity);
}

template <class Objects> QStringList LIB_SYNCTHING_CONNECTOR_EXPORT ids(const Objects &objects)
{
    QStringList ids;
    ids.reserve(objects.size());
    for (const auto &object : objects) {
        ids << object.id;
    }
    return ids;
}
} // namespace Data

#endif // DATA_UTILS_H
