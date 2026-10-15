#pragma once
#include <QWidget>
#include <QStringList>
#include <functional>
#include <limits>
#include <vector>
class QComboBox;
class QDoubleSpinBox;
class QVBoxLayout;
class QLabel;
namespace lufs {
struct RowData {
  QString id, name, note, state;
  double momentary = 0, shortTerm = 0, integrated = 0;
  bool available = false;
  double target = -14.0;
  double truePeak = std::numeric_limits<double>::quiet_NaN();
  double truePeakMax = std::numeric_limits<double>::quiet_NaN();
  bool truePeakAvailable = false;
  bool fresh = false, active = true, muted = false, error = false, routed = false;
  quint64 frames = 0, resets = 0, dropped = 0;
  qint64 ageMs = -1;
  double volumeGain = 1;
};
struct SourceOption { QString id, name; bool checked = true; double target = -14; bool customTarget = false; };
class MeterCard;
class Dock : public QWidget {
public:
  explicit Dock(QWidget* parent = nullptr);
  void updateRows(const std::vector<RowData>& masters, const std::vector<RowData>& sources, const QString& status);
  int mode() const;
  void setMode(int value);
  void setTarget(double value);
  void setOrientation(int value);
  int orientation() const { return orientation_; }
  void setMetrics(bool lufs, bool truePeak);
  bool showsLufs() const { return showLufs_; }
  bool showsTruePeak() const { return showTruePeak_; }
  void setTruePeakWarning(double value);
  double truePeakWarning() const { return truePeakWarning_; }
  void setExportEnabled(bool value);
  bool exportEnabled() const { return exportEnabled_; }
  void setExportPath(const QString& path) { exportPath_ = path; }
  void setSourceOptions(std::vector<SourceOption> options) { options_ = std::move(options); }
  void setMasterOptions(std::vector<SourceOption> options) { masterOptions_ = std::move(options); }
  std::function<void(const QStringList&)> selectionChanged;
  std::function<void()> resetClicked;
  std::function<void(int)> modeChanged;
  std::function<void(double)> targetChanged;
  std::function<void(const QString&, double)> rowTargetChanged;
  std::function<void(int)> orientationChanged;
  std::function<void(bool, bool)> metricsChanged;
  std::function<void(double)> truePeakWarningChanged;
  std::function<void(bool)> exportEnabledChanged;
private:
  void clearCards();
  void selectSources();
  QComboBox* mode_;
  QDoubleSpinBox* target_;
  QLabel* status_;
  QLabel* empty_;
  QLabel* sourceHeading_;
  QVBoxLayout *masters_, *sources_;
  std::vector<MeterCard*> masterCards_, sourceCards_;
  std::vector<SourceOption> options_;
  std::vector<SourceOption> masterOptions_;
  int orientation_ = 0;
  bool showLufs_ = true, showTruePeak_ = false;
  double truePeakWarning_ = -6;
  bool exportEnabled_ = false;
  QString exportPath_;
  std::vector<RowData> lastMasters_, lastSources_;
  QString lastStatus_;
};
}
