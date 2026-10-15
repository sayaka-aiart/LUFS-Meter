#include "telemetry.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cmath>
namespace lufs {
static QJsonValue number(double value, bool available = true) {
  return available && std::isfinite(value) ? QJsonValue(value) : QJsonValue();
}
QJsonObject telemetrySnapshot(const std::vector<RowData>& masters, const std::vector<RowData>& sources,
                             QJsonObject metadata, unsigned rate) {
  const auto now = QDateTime::currentDateTimeUtc();
  metadata["schemaVersion"] = 1;
  metadata["app"] = "LUFS Meter";
  metadata["updatedAtUnixMs"] = static_cast<double>(now.toMSecsSinceEpoch());
  metadata["updatedAtUtc"] = now.toString(Qt::ISODateWithMs);
  metadata["sampleRateHz"] = static_cast<int>(rate);
  auto encode = [rate](const std::vector<RowData>& rows) {
    QJsonArray result;
    for (const auto& r : rows) {
      const bool measuring = r.fresh && r.active && !r.muted && !r.error && rate > 0;
      const bool m = measuring && r.frames >= static_cast<quint64>(rate * .4);
      const bool s = measuring && r.frames >= static_cast<quint64>(rate) * 3;
      QString status = r.error ? "error" : r.muted ? "muted" : !r.active ? "inactive" : !r.fresh ? "stale" : !m ? "warming_up" : !std::isfinite(r.momentary) ? "silence" : "ready";
      QJsonObject row{{"id", r.id}, {"name", r.name}, {"status", status}, {"state", r.state}, {"note", r.note},
        {"fresh", r.fresh}, {"active", r.active}, {"muted", r.muted}, {"error", r.error}, {"routedToStream", r.routed},
        {"ageMs", r.ageMs < 0 ? QJsonValue() : QJsonValue(static_cast<double>(r.ageMs))},
        {"frames", static_cast<double>(r.frames)}, {"resets", static_cast<double>(r.resets)}, {"dropped", static_cast<double>(r.dropped)},
        {"volumeGain", number(r.volumeGain)}, {"targetLufs", number(r.target)},
        {"lufs", QJsonObject{{"momentary", number(r.momentary, m)}, {"shortTerm", number(r.shortTerm, s)}, {"integrated", number(r.integrated, m)}}},
        {"lufsReady", QJsonObject{{"momentary", m}, {"shortTerm", s}, {"integrated", m}}},
        {"truePeakDbtp", number(r.truePeak, measuring && r.truePeakAvailable)},
        {"truePeakAvailable", measuring && r.truePeakAvailable}, {"truePeakMaxDbtp", number(r.truePeakMax)}};
      result.append(row);
    }
    return result;
  };
  metadata["masters"] = encode(masters); metadata["sources"] = encode(sources);
  return metadata;
}
bool writeTelemetry(const QString& path, const QJsonObject& snapshot, QString* error) {
  QSaveFile file(path); file.setDirectWriteFallback(false);
  const auto bytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
    if (error) *error = file.errorString();
    return false;
  }
  if (error) error->clear();
  return true;
}
}
