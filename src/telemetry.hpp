#pragma once
#include "dock.hpp"
#include <QJsonObject>
namespace lufs {
QJsonObject telemetrySnapshot(const std::vector<RowData>& masters, const std::vector<RowData>& sources,
                             QJsonObject metadata, unsigned rate);
bool writeTelemetry(const QString& path, const QJsonObject& snapshot, QString* error = nullptr);
}
