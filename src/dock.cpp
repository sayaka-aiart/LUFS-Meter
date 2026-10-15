#include "dock.hpp"
#include "target.hpp"
#include <QComboBox>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFormLayout>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStyle>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

namespace lufs {
static QColor accent(TargetLevel level) {
  return QColor(level == TargetLevel::Over ? "#eb7779" : level == TargetLevel::Near ? "#e2c27b" : "#81ada4");
}
static QColor barAccent(bool peak, TargetLevel level) {
  return peak && level == TargetLevel::Below ? QColor("#a69bd2") : accent(level);
}
class LoudnessBar : public QWidget {
public:
  double value = -60, target = -14, warning = -6, heldPeak = std::numeric_limits<double>::quiet_NaN();
  bool valid = false;
  bool vertical_ = false;
  bool peak_ = false, compact_ = false;
  explicit LoudnessBar(QWidget* parent, bool vertical = false) : QWidget(parent), vertical_(vertical) {
    if (vertical) { setFixedWidth(40); setMinimumHeight(130); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding); }
    else setFixedHeight(16);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (vertical_) {
      const QRectF track(peak_ ? 29 : 27.5, 8, peak_ ? 3 : 6, height() - 16);
      p.setPen(Qt::NoPen); p.setBrush(QColor("#2b3037")); p.drawRoundedRect(track, 2, 2);
      if (valid) {
        const double fraction = std::clamp((value + 60.0) / 60.0, 0.0, 1.0);
        p.setBrush(barAccent(peak_, peak_ ? true_peak_level(value, true, warning) : target_level(value, target, true)));
        p.drawRoundedRect(QRectF(track.x(), track.bottom() - track.height() * fraction, track.width(), track.height() * fraction), 1.5, 1.5);
      }
      if (peak_ && std::isfinite(heldPeak)) {
        const double y = track.bottom() - track.height() * std::clamp((heldPeak + 60.0) / 60.0, 0.0, 1.0);
        p.setPen(QPen(barAccent(true, true_peak_level(heldPeak, true, warning)), 2)); p.drawLine(QPointF(26, y), QPointF(35, y));
      }
      const double ty = track.bottom() - track.height() * std::clamp((target + 60.0) / 60.0, 0.0, 1.0);
      p.setPen(QPen(QColor("#d1d4d9"), 1)); p.drawLine(QPointF(25, ty), QPointF(36, ty));
      QFont font = p.font(); font.setPixelSize(9); p.setFont(font); p.setPen(QColor("#6c727e"));
      for (int i = 0; i <= 2; ++i) p.drawText(QRectF(0, 2 + (height() - 16) * i / 2.0, 22, 12), Qt::AlignRight, QString::number(-30 * i));
      return;
    }
    const QRectF track(1, 1, width() - 2, peak_ ? 3 : 6);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#2b3037")); p.drawRoundedRect(track, 2, 2);
    if (valid) {
      const double fraction = std::clamp((value + 60.0) / 60.0, 0.0, 1.0);
      p.setBrush(barAccent(peak_, peak_ ? true_peak_level(value, true, warning) : target_level(value, target, true)));
      p.drawRoundedRect(QRectF(track.x(), track.y(), track.width() * fraction, track.height()), peak_ ? 1.5 : 2, peak_ ? 1.5 : 2);
    }
    if (peak_ && std::isfinite(heldPeak)) {
      const double x = track.x() + track.width() * std::clamp((heldPeak + 60.0) / 60.0, 0.0, 1.0);
      p.setPen(QPen(barAccent(true, true_peak_level(heldPeak, true, warning)), 2)); p.drawLine(QPointF(x, 0), QPointF(x, 7));
    }
    const double tx = track.x() + track.width() * std::clamp((target + 60.0) / 60.0, 0.0, 1.0);
    p.setPen(QPen(QColor("#d1d4d9"), 1)); p.drawLine(QPointF(tx, 0), QPointF(tx, 8));
    if (compact_) return;
    p.setPen(QColor("#6c727e")); QFont font = p.font(); font.setPixelSize(9); p.setFont(font);
    for (int i = 0; i <= 2; ++i) {
      const int x = 1 + static_cast<int>((width() - 2) * i / 2.0);
      const QString text = QString::number(-60 + 30 * i);
      const int tw = p.fontMetrics().horizontalAdvance(text);
      p.drawText(std::clamp(x - tw / 2, 0, width() - tw), 16, text);
    }
  }
};
class MeterCard : public QFrame {
public:
  QLabel *name_, *state_, *peakMax_ = nullptr;
  QLabel *loudnessNumber_ = nullptr, *peakNumber_ = nullptr;
  LoudnessBar *loudnessBar_ = nullptr, *peakBar_ = nullptr;
  bool master_, vertical_;
  explicit MeterCard(bool master, QWidget* parent, bool vertical, bool showLufs, bool showPeak)
    : QFrame(parent), master_(master), vertical_(vertical) {
    const bool both = showLufs && showPeak;
    setObjectName(master ? "masterCard" : "sourceCard");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(master ? 16 : 10, master ? 12 : 6, master ? 16 : 10, master ? 10 : 5);
    layout->setSpacing(master ? 7 : 3);
    name_ = new QLabel; name_->setTextFormat(Qt::PlainText); name_->setObjectName("sourceName"); name_->setWordWrap(true);
    state_ = new QLabel; state_->setObjectName("state");
    auto* top = new QHBoxLayout; top->setSpacing(8); top->addWidget(name_, 1); top->addWidget(state_); layout->addLayout(top);
    if (vertical) {
      setFixedWidth(both ? (master ? 132 : 120) : (master ? 102 : 78));
      setMinimumHeight(300); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
      layout->setContentsMargins(8, 12, 8, 10); layout->setSpacing(7);
      name_->setAlignment(Qt::AlignCenter); name_->setFixedHeight(20);
      top->removeWidget(state_); state_->setAlignment(Qt::AlignCenter); state_->setFixedHeight(11); layout->addWidget(state_);
    }
    auto* metrics = new QBoxLayout(vertical ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom); metrics->setSpacing(both ? (vertical ? 12 : 1) : 0); layout->addLayout(metrics, vertical ? 1 : 0);
    auto addMetric = [&](bool peak) {
      auto* panel = new QWidget(this); panel->setObjectName(peak ? "truePeakMetric" : "lufsMetric");
      auto* column = new QVBoxLayout(panel); column->setContentsMargins(0, 0, 0, 0); column->setSpacing(master ? 5 : 3);
      auto* number = new QLabel(QStringLiteral("—")); number->setProperty("metric", peak ? "truePeak" : "lufs");
      number->setObjectName(vertical ? (master ? "verticalMasterNumber" : "verticalSourceNumber") : (master ? "masterNumber" : "sourceNumber"));
      auto* unit = new QLabel(peak ? "dBTP" : "LUFS"); unit->setObjectName("unit");
      auto* bar = new LoudnessBar(panel, vertical); bar->peak_ = peak; bar->setObjectName(peak ? "truePeakBar" : "lufsBar"); bar->setAccessibleName(peak ? "True Peak" : "LUFS"); if (peak) bar->target = 0;
      QLabel* maxLabel = nullptr;
      if (peak || (vertical && both)) {
        maxLabel = new QLabel(peak ? QStringLiteral("MAX —") : QString(), panel);
        maxLabel->setObjectName(peak ? "truePeakMax" : "maxSpacer"); maxLabel->setFixedHeight(12);
        maxLabel->setStyleSheet("font-family:'Segoe UI';font-size:9px;color:#9b92b8;");
        if (peak) peakMax_ = maxLabel;
      }
      if (vertical) {
        if (both) number->setStyleSheet("font-size:18px;");
        column->addWidget(bar, 1, Qt::AlignHCenter);
        number->setAlignment(Qt::AlignCenter); number->setFixedHeight(38); column->addWidget(number);
        unit->setAlignment(Qt::AlignCenter); unit->setFixedHeight(11); column->addWidget(unit);
        if (maxLabel) { maxLabel->setAlignment(Qt::AlignCenter); column->addWidget(maxLabel); }
      } else if (both) {
        if (master) number->setStyleSheet("font-size:32px;");
        auto* line = new QHBoxLayout; line->setSpacing(8);
        unit->setFixedWidth(28); unit->setStyleSheet("padding-bottom:0px;"); line->addWidget(unit);
        bar->compact_ = true; bar->setFixedHeight(8);
        if (peak) {
          auto* trackColumn = new QVBoxLayout; trackColumn->setSpacing(0); trackColumn->addWidget(bar);
          maxLabel->setAlignment(Qt::AlignRight); trackColumn->addWidget(maxLabel); line->addLayout(trackColumn, 1);
        } else line->addWidget(bar, 1);
        number->setAlignment(Qt::AlignRight | Qt::AlignVCenter); number->setFixedWidth(master ? 90 : 64); line->addWidget(number);
        column->addLayout(line);
      } else if (!master && !both) {
        // Preserve the compact single-metric source row.
        top->addWidget(number); if (peak) top->addWidget(unit, 0, Qt::AlignBottom); else { unit->setParent(this); unit->hide(); }
        column->addWidget(bar);
        if (maxLabel) { maxLabel->setAlignment(Qt::AlignRight); column->addWidget(maxLabel); }
      } else {
        auto* numeric = new QHBoxLayout; numeric->setSpacing(5); numeric->addWidget(number); numeric->addWidget(unit, 0, Qt::AlignBottom); numeric->addStretch();
        column->addLayout(numeric); column->addWidget(bar);
        if (maxLabel) { maxLabel->setAlignment(Qt::AlignRight); column->addWidget(maxLabel); }
      }
      unit->setStyleSheet(peak ? "color:#b4a5dd; padding-bottom:0px;" : "color:#81ada4; padding-bottom:0px;");
      metrics->addWidget(panel, 1);
      if (peak) { peakNumber_ = number; peakBar_ = bar; }
      else { loudnessNumber_ = number; loudnessBar_ = bar; }
    };
    if (showLufs) addMetric(false);
    if (showPeak) addMetric(true);
  }
  void apply(const RowData& data, int mode, double warning) {
    name_->setText(data.name);
    const double value = mode == 0 ? data.shortTerm : mode == 1 ? data.momentary : data.integrated;
    auto applyMetric = [&](QLabel* number, LoudnessBar* bar, double reading, bool available, bool peak) {
      if (!number) return false;
      const bool finite = available && std::isfinite(reading);
      const bool silence = available && std::isinf(reading) && reading < 0;
      number->setText(finite ? QString::number(reading, 'f', 1) : silence ? QStringLiteral("−∞") : QStringLiteral("—"));
      const auto level = peak ? true_peak_level(reading, finite, warning) : target_level(reading, data.target, finite);
      const QString color = !finite ? "#646c79" : level == TargetLevel::Over || level == TargetLevel::Near ? accent(level).name() : "#e2e5e9";
      const bool both = loudnessNumber_ && peakNumber_;
      int pixels = vertical_ ? (both ? 18 : master_ ? 28 : 20) : master_ ? (both ? 32 : 44) : 22;
      if (peak) pixels = static_cast<int>(std::lround(pixels * 0.875));
      QFont font = number->font(); font.setPixelSize(pixels);
      const int availableWidth = number->width() - 2;
      while (pixels > 6 && availableWidth > 20 && QFontMetricsF(font).horizontalAdvance(number->text()) > availableWidth) font.setPixelSize(--pixels);
      number->setStyleSheet(QString("font-size:%1px;color:%2;").arg(pixels).arg(color));
      bar->valid = finite; bar->value = reading; bar->target = peak ? 0 : data.target; bar->warning = warning; bar->heldPeak = peak ? data.truePeakMax : std::numeric_limits<double>::quiet_NaN(); bar->update();
      bar->setToolTip(peak ? QStringLiteral("True Peak · 直近約0.4秒の最大値 · %1 dBTPから黄色、0 dBTP超で赤").arg(warning, 0, 'f', 1) : QStringLiteral("目標 %1 LUFS · 3 LU手前から黄色、超過で赤").arg(data.target, 0, 'f', 1));
      return finite || silence;
    };
    const bool lufsValid = applyMetric(loudnessNumber_, loudnessBar_, value, data.available, false);
    const bool peakValid = applyMetric(peakNumber_, peakBar_, data.truePeak, data.truePeakAvailable, true);
    const bool valid = lufsValid || peakValid;
    if (peakMax_) {
      const bool held = std::isfinite(data.truePeakMax);
      peakMax_->setText(held ? QString("MAX %1").arg(data.truePeakMax, 0, 'f', 1) : QStringLiteral("MAX —"));
      const auto level = true_peak_level(data.truePeakMax, held, warning);
      const auto color = held ? barAccent(true, level).name() : QString("#646c79");
      peakMax_->setStyleSheet("font-family:'Segoe UI';font-size:9px;color:" + color + ";");
      peakMax_->setToolTip(QStringLiteral("True Peak MAX · dBTP · 計測リセットまで保持"));
    }
    QString state;
    if (!valid) state = data.state == QStringLiteral("ミュート") ? "MUTE" : data.state == QStringLiteral("計測準備中") ? QStringLiteral("準備中") : "—";
    if (master_ && valid) {
      const auto match = QRegularExpression(QStringLiteral("トラック(\\d+)")).match(data.note);
      if (match.hasMatch()) state = "T" + match.captured(1);
    }
    state_->setText(state); state_->setVisible(vertical_ || !state.isEmpty());
    setToolTip(data.note + "\n" + data.state + (loudnessNumber_ ? QStringLiteral("\n目標 %1 LUFS").arg(data.target, 0, 'f', 1) : QString())
      + (peakNumber_ ? QStringLiteral("\nTrue Peak · dBTP · 直近約0.4秒の最大値") : QString()));
  }
};
Dock::Dock(QWidget* parent) : QWidget(parent) {
  setWindowTitle(QStringLiteral("LUFS Meter")); setMinimumWidth(260); setObjectName("lufsDock");
  setStyleSheet(QStringLiteral(
    "QWidget {font-family:'Yu Gothic UI'; font-size:11px; color:#d1d5dd;}"
    "QWidget#lufsDock,QWidget#sourcesContent {background:#111418;}"
    "QFrame#masterCard {background:#191e24; border:1px solid #353c46; border-radius:10px;}"
    "QFrame#sourceCard {background:#191d23; border:none; border-radius:8px;}"
    "QLabel {border:none; background:transparent;}"
    "QLabel#masterNumber {font-family:'Segoe UI'; font-size:44px; font-weight:300;}"
    "QLabel#sourceNumber {font-family:'Segoe UI'; font-size:22px; font-weight:400;}"
    "QLabel#verticalMasterNumber {font-family:'Segoe UI'; font-size:28px; font-weight:300;}"
    "QLabel#verticalSourceNumber {font-family:'Segoe UI'; font-size:20px; font-weight:400;}"
    "QLabel#sourceName {font-size:11px; color:#c9ced7; font-weight:500;}"
    "QLabel#unit {font-family:'Segoe UI'; font-size:9px; color:#707988; padding-bottom:5px;}"
    "QLabel#state,QLabel#smallLabel {font-family:'Segoe UI'; color:#7f8896; font-size:10px;}"
    "QLabel#brand {font-family:'Segoe UI'; font-size:13px; color:#9ca6b4; font-weight:600;}"
    "QPushButton,QComboBox,QDoubleSpinBox,QLineEdit {background:#1b2027; border:1px solid #333b47; border-radius:5px; padding:5px 7px;}"
    "QLineEdit {selection-background-color:#354252; selection-color:#e2e5e9;}"
    "QPushButton:hover {background:#29313b; border-color:#566474;}"
    "QPushButton:pressed {background:#13171c;}"
    "QPushButton#gear,QPushButton#reset {background:transparent; border:none;} QPushButton#gear:hover,QPushButton#reset:hover {background:#252c35;}"
    "QComboBox QAbstractItemView,QListWidget,QDialog,QMenu {background:#1b2027; color:#d1d5dd;}"
    "QWidget#settingsPage {background:#191e24;} QTabWidget::pane {background:#191e24; border:1px solid #333b47; border-radius:6px;}"
    "QTabBar::tab {background:#14191f; color:#8995a5; padding:7px 14px; border:1px solid #333b47; border-bottom:none;}"
    "QTabBar::tab:selected {background:#191e24; color:#d1d5dd;}"
    "QScrollArea {border:none; background:transparent;}"
    "QScrollBar:vertical {background:#111418; width:5px; margin:0;}"
    "QScrollBar::handle:vertical {background:#37404b; min-height:24px; border-radius:2px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}"
    "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:none;}"));
  auto* root = new QVBoxLayout(this); root->setContentsMargins(12, 10, 12, 12); root->setSpacing(8);
  auto* header = new QHBoxLayout; auto* brand = new QLabel("LUFS Meter"); brand->setObjectName("brand");
  QFont brandFont = brand->font(); brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 2); brand->setFont(brandFont);
  header->addWidget(brand); header->addStretch();
  mode_ = new QComboBox(this); mode_->addItems({QStringLiteral("3秒平均"), QStringLiteral("瞬間"), QStringLiteral("全体平均")}); mode_->hide();
  target_ = new QDoubleSpinBox(this); target_->setRange(-60, 0); target_->setDecimals(1); target_->setSingleStep(1); target_->setValue(-14); target_->hide();
  auto* resetButton = new QPushButton; resetButton->setObjectName("reset"); resetButton->setFixedSize(22, 22);
  resetButton->setToolTip(QStringLiteral("計測リセット（MAXもクリア）")); resetButton->setAccessibleName(QStringLiteral("計測リセット"));
  QPixmap resetIcon(20, 20); resetIcon.fill(Qt::transparent); {
    QPainter p(&resetIcon); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#929daa"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawArc(QRectF(4, 4, 12, 12), 40 * 16, 280 * 16);
    p.drawPolyline(QPolygonF{QPointF(15, 3), QPointF(15, 7), QPointF(11, 6)});
  }
  resetButton->setIcon(QIcon(resetIcon)); resetButton->setIconSize(QSize(18, 18)); header->addWidget(resetButton);
  connect(resetButton, &QPushButton::clicked, this, [this] { if (resetClicked) resetClicked(); });
  auto* gear = new QPushButton; gear->setObjectName("gear"); gear->setFixedSize(22, 22); gear->setToolTip(QStringLiteral("設定"));
  QPixmap icon(20, 20); icon.fill(Qt::transparent); { QPainter p(&icon); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(QColor("#929daa"), 1.6)); p.drawEllipse(QPointF(10, 10), 5.6, 5.6); p.drawEllipse(QPointF(10, 10), 2.2, 2.2);
    for (int i = 0; i < 8; ++i) { const double a = i * 3.141592653589793 / 4; p.drawLine(QPointF(10 + 6*std::cos(a), 10+6*std::sin(a)), QPointF(10+8.2*std::cos(a), 10+8.2*std::sin(a))); } }
  gear->setIcon(QIcon(icon)); gear->setIconSize(QSize(18, 18)); header->addWidget(gear); root->addLayout(header);
  masters_ = new QVBoxLayout; masters_->setSpacing(8); root->addLayout(masters_);
  status_ = new QLabel(this); status_->hide();
  sourceHeading_ = new QLabel("SOURCES"); sourceHeading_->hide(); sourceHeading_->setObjectName("smallLabel"); root->addWidget(sourceHeading_);
  auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->viewport()->setStyleSheet("background:#111418;");
  auto* content = new QWidget; content->setObjectName("sourcesContent");
  sources_ = new QVBoxLayout(content); sources_->setContentsMargins(0, 0, 0, 0); sources_->setSpacing(5); sources_->setAlignment(Qt::AlignTop);
  empty_ = new QLabel(QStringLiteral("ソースなし")); empty_->setObjectName("smallLabel"); empty_->setWordWrap(true); sources_->addWidget(empty_);
  scroll->setWidget(content); root->addWidget(scroll, 1);
  connect(gear, &QPushButton::clicked, this, [this] { selectSources(); });
  connect(mode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int mode) { if (modeChanged) modeChanged(mode); });
  connect(target_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) { if (targetChanged) targetChanged(value); });
}
int Dock::mode() const { return mode_->currentIndex(); }
void Dock::setMode(int value) { mode_->setCurrentIndex(std::clamp(value, 0, 2)); }
void Dock::setTarget(double value) { target_->setValue(value); }
void Dock::clearCards() {
  for (auto* card : masterCards_) { masters_->removeWidget(card); sources_->removeWidget(card); delete card; }
  for (auto* card : sourceCards_) { sources_->removeWidget(card); delete card; }
  masterCards_.clear(); sourceCards_.clear();
}
void Dock::setOrientation(int value) {
  value = std::clamp(value, 0, 1); if (value == orientation_) return;
  clearCards(); orientation_ = value;
  sourceHeading_->hide(); sources_->setDirection(value ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
  updateRows(lastMasters_, lastSources_, lastStatus_);
  if (orientationChanged) orientationChanged(value);
}
void Dock::setMetrics(bool lufs, bool truePeak) {
  if (!lufs && !truePeak) lufs = true;
  if (lufs == showLufs_ && truePeak == showTruePeak_) return;
  clearCards(); showLufs_ = lufs; showTruePeak_ = truePeak;
  updateRows(lastMasters_, lastSources_, lastStatus_);
  if (metricsChanged) metricsChanged(lufs, truePeak);
}
void Dock::setTruePeakWarning(double value) {
  value = peak_warning(value); if (value == truePeakWarning_) return;
  truePeakWarning_ = value; updateRows(lastMasters_, lastSources_, lastStatus_);
  if (truePeakWarningChanged) truePeakWarningChanged(value);
}
void Dock::setExportEnabled(bool value) {
  if (value == exportEnabled_) return;
  exportEnabled_ = value; if (exportEnabledChanged) exportEnabledChanged(value);
}
void Dock::updateRows(const std::vector<RowData>& masters, const std::vector<RowData>& sources, const QString& status) {
  lastMasters_ = masters; lastSources_ = sources; lastStatus_ = status;
  auto apply = [this](const std::vector<RowData>& data, std::vector<MeterCard*>& cards, QVBoxLayout* layout, bool master) {
    while (cards.size() > data.size()) { auto* card = cards.back(); cards.pop_back(); layout->removeWidget(card); delete card; }
    while (cards.size() < data.size()) { auto* card = new MeterCard(master, this, orientation_ == 1, showLufs_, showTruePeak_); if (master && orientation_) layout->insertWidget(static_cast<int>(cards.size()), card); else layout->addWidget(card); cards.push_back(card); }
    for (std::size_t i = 0; i < data.size(); ++i) cards[i]->apply(data[i], mode(), truePeakWarning_);
  };
  apply(masters, masterCards_, orientation_ ? sources_ : masters_, true); apply(sources, sourceCards_, sources_, false);
  for (auto* card : masterCards_) card->setToolTip(card->toolTip() + "\n" + status);
  empty_->setVisible(sources.empty());
  empty_->setText(masters.empty() && !status.isEmpty() ? QStringLiteral("設定を確認") : QStringLiteral("ソースなし")); empty_->setToolTip(status);
}
void Dock::selectSources() {
  QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("設定")); dialog.resize(380, 410);
  auto* layout = new QVBoxLayout(&dialog); auto* tabs = new QTabWidget; layout->addWidget(tabs);
  auto* general = new QWidget; general->setObjectName("settingsPage"); auto* form = new QFormLayout(general); form->setContentsMargins(16, 20, 16, 16); form->setSpacing(16);
  auto* metricRow = new QWidget; auto* metricLayout = new QHBoxLayout(metricRow); metricLayout->setContentsMargins(0, 0, 0, 0);
  auto* showLufs = new QCheckBox("LUFS"); showLufs->setObjectName("showLufs"); showLufs->setChecked(showLufs_);
  auto* showPeak = new QCheckBox("True Peak"); showPeak->setObjectName("showTruePeak"); showPeak->setChecked(showTruePeak_);
  metricLayout->addWidget(showLufs); metricLayout->addWidget(showPeak); metricLayout->addStretch(); form->addRow(QStringLiteral("表示項目"), metricRow);
  auto updateChecks = [=] { showLufs->setEnabled(showPeak->isChecked()); showPeak->setEnabled(showLufs->isChecked()); };
  connect(showLufs, &QCheckBox::toggled, &dialog, updateChecks); connect(showPeak, &QCheckBox::toggled, &dialog, updateChecks); updateChecks();
  auto* mode = new QComboBox; mode->setObjectName("lufsMode"); mode->addItems({QStringLiteral("3秒平均"), QStringLiteral("瞬間（0.4秒）"), QStringLiteral("全体平均")}); mode->setCurrentIndex(this->mode()); form->addRow(QStringLiteral("測定表示"), mode);
  auto* target = new QDoubleSpinBox; target->setObjectName("commonTarget"); target->setRange(-60, 0); target->setDecimals(1); target->setSuffix(" LUFS"); target->setValue(target_->value()); form->addRow(QStringLiteral("共通の目標"), target);
  auto* peakWarning = new QDoubleSpinBox; peakWarning->setObjectName("truePeakWarning"); peakWarning->setRange(-60, 0); peakWarning->setDecimals(1); peakWarning->setSuffix(" dBTP"); peakWarning->setValue(truePeakWarning_);
  form->addRow(QStringLiteral("True Peak 黄色"), peakWarning); peakWarning->setEnabled(showPeak->isChecked());
  connect(showPeak, &QCheckBox::toggled, peakWarning, &QWidget::setEnabled);
  auto* direction = new QComboBox; direction->setObjectName("orientation"); direction->addItems({QStringLiteral("横"), QStringLiteral("縦")}); direction->setCurrentIndex(orientation_); form->addRow(QStringLiteral("メーター"), direction);
  mode->setEnabled(showLufs->isChecked()); target->setEnabled(showLufs->isChecked());
  connect(showLufs, &QCheckBox::toggled, mode, &QWidget::setEnabled); connect(showLufs, &QCheckBox::toggled, target, &QWidget::setEnabled);
  tabs->addTab(general, QStringLiteral("表示"));
  auto* sourcePage = new QWidget; sourcePage->setObjectName("settingsPage"); auto* sourceLayout = new QVBoxLayout(sourcePage); sourceLayout->setContentsMargins(12, 12, 12, 12);
  auto* list = new QListWidget; list->setDragDropMode(QAbstractItemView::InternalMove); list->setToolTip(QStringLiteral("チェックで表示、ドラッグで並べ替え")); sourceLayout->addWidget(list);
  auto add = [&](const SourceOption& option, bool master) {
    auto* item = new QListWidgetItem(option.name, list); item->setData(Qt::UserRole, option.id);
    item->setData(Qt::UserRole + 1, option.target); item->setData(Qt::UserRole + 2, option.customTarget); item->setData(Qt::UserRole + 3, master);
    if (!master) { item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(option.checked ? Qt::Checked : Qt::Unchecked); }
    else item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
  };
  for (const auto& option : masterOptions_) add(option, true);
  for (const auto& option : options_) add(option, false);
  auto* targetRow = new QHBoxLayout; targetRow->addWidget(new QLabel(QStringLiteral("目標")));
  auto* sourceTarget = new QDoubleSpinBox; sourceTarget->setRange(-60, 0); sourceTarget->setDecimals(1); sourceTarget->setSuffix(" LUFS"); targetRow->addWidget(sourceTarget, 1);
  auto* inherit = new QPushButton(QStringLiteral("共通値")); targetRow->addWidget(inherit); sourceLayout->addLayout(targetRow);
  connect(list, &QListWidget::currentItemChanged, &dialog, [=](QListWidgetItem* current, QListWidgetItem*) {
    QSignalBlocker block(sourceTarget); sourceTarget->setEnabled(current && showLufs->isChecked()); inherit->setEnabled(current && showLufs->isChecked());
    if (current) sourceTarget->setValue(current->data(Qt::UserRole + 2).toBool() ? current->data(Qt::UserRole + 1).toDouble() : target->value());
  });
  connect(sourceTarget, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [=](double value) {
    if (auto* current = list->currentItem()) { current->setData(Qt::UserRole + 1, value); current->setData(Qt::UserRole + 2, true); }
  });
  connect(target, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [=](double value) {
    if (auto* current = list->currentItem()) if (!current->data(Qt::UserRole + 2).toBool()) { QSignalBlocker block(sourceTarget); sourceTarget->setValue(value); }
  });
  connect(inherit, &QPushButton::clicked, &dialog, [=] {
    if (auto* current = list->currentItem()) { current->setData(Qt::UserRole + 2, false); QSignalBlocker block(sourceTarget); sourceTarget->setValue(target->value()); }
  });
  if (list->count()) list->setCurrentRow(0); else { sourceTarget->setEnabled(false); inherit->setEnabled(false); }
  connect(showLufs, &QCheckBox::toggled, &dialog, [=](bool visible) { sourceTarget->setEnabled(visible && list->currentItem()); inherit->setEnabled(visible && list->currentItem()); });
  tabs->addTab(sourcePage, QStringLiteral("ソース"));
  auto* externalPage = new QWidget; externalPage->setObjectName("settingsPage");
  auto* externalForm = new QFormLayout(externalPage); externalForm->setContentsMargins(16, 20, 16, 16); externalForm->setSpacing(16);
  auto* exportCheck = new QCheckBox(QStringLiteral("JSON出力")); exportCheck->setObjectName("exportEnabled"); exportCheck->setChecked(exportEnabled_); externalForm->addRow(QStringLiteral("外部連携"), exportCheck);
  auto* exportFile = new QLineEdit(exportPath_); exportFile->setObjectName("exportPath"); exportFile->setReadOnly(true); exportFile->setCursorPosition(0); exportFile->setToolTip(exportPath_); externalForm->addRow(QStringLiteral("保存先"), exportFile);
  auto* copyPath = new QPushButton(QStringLiteral("パスをコピー")); externalForm->addRow(QString(), copyPath);
  connect(copyPath, &QPushButton::clicked, &dialog, [=] { QApplication::clipboard()->setText(exportFile->text()); });
  tabs->addTab(externalPage, QStringLiteral("外部連携"));
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() == QDialog::Accepted) {
    setExportEnabled(exportCheck->isChecked()); setTruePeakWarning(peakWarning->value()); setMode(mode->currentIndex()); setTarget(target->value()); setOrientation(direction->currentIndex()); setMetrics(showLufs->isChecked(), showPeak->isChecked());
    QStringList selected;
    for (int i = 0; i < list->count(); ++i) {
      auto* item = list->item(i); const QString id = item->data(Qt::UserRole).toString(); const bool master = item->data(Qt::UserRole + 3).toBool();
      if (!master && item->checkState() == Qt::Checked && selected.size() < 32) selected.append(id);
      if (rowTargetChanged) rowTargetChanged(id, item->data(Qt::UserRole + 2).toBool() ? item->data(Qt::UserRole + 1).toDouble() : std::numeric_limits<double>::quiet_NaN());
    }
    if (selectionChanged) selectionChanged(selected);
  }
}
}
