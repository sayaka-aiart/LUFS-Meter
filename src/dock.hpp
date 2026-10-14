#pragma once
#include <QWidget>
#include <QStringList>
#include <functional>
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
  void setSourceOptions(std::vector<SourceOption> options) { options_ = std::move(options); }
  void setMasterOptions(std::vector<SourceOption> options) { masterOptions_ = std::move(options); }
  std::function<void(const QStringList&)> selectionChanged;
  std::function<void()> resetClicked;
  std::function<void(int)> modeChanged;
  std::function<void(double)> targetChanged;
  std::function<void(const QString&, double)> rowTargetChanged;
  std::function<void(int)> orientationChanged;
private:
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
};
}
