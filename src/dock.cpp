#include "dock.hpp"
#include "target.hpp"
#include "theme.hpp"
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
#include <utility>

namespace lufs {
// LUFS bar fill. Numbers use readingColor() so that red text stays readable on the dark card.
static QColor lufsBarColor(TargetLevel level) {
  return QColor(level == TargetLevel::Over ? theme::kMute : level == TargetLevel::Near ? theme::kWarn : theme::kLufs);
}
static QString readingColor(TargetLevel level) {
  return level == TargetLevel::Over ? theme::kOverText : level == TargetLevel::Near ? theme::kWarn : theme::kText;
}
static QString maxColor(TargetLevel level) {
  return level == TargetLevel::Over ? theme::kOverText : level == TargetLevel::Near ? theme::kWarn : theme::kDim;
}
static double meterFraction(double db) { return std::clamp((db + 60.0) / 60.0, 0.0, 1.0); }
class LoudnessBar : public QWidget {
public:
  double value = -60, target = -14, warning = -6, heldPeak = std::numeric_limits<double>::quiet_NaN();
  bool valid = false;
  bool vertical_ = false;
  bool peak_ = false, compact_ = false;
  explicit LoudnessBar(QWidget* parent, bool vertical = false) : QWidget(parent), vertical_(vertical) {
    if (vertical) { setFixedWidth(40); setMinimumHeight(130); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding); }
    else setFixedHeight(18);
  }
  // True Peak follows the LAA LED meter: 3 px lit and 2 px dark, green up to the warning level,
  // then yellow. Going over 0 dBTP turns every lit cell red.
  void paintPeakCells(QPainter& p, const QRectF& track) const {
    static constexpr double kCell = 3, kStep = 5;
    p.setRenderHint(QPainter::Antialiasing, false); p.setPen(Qt::NoPen);
    const double length = vertical_ ? track.height() : track.width();
    const double lit = valid ? meterFraction(value) * length : 0;
    const bool over = valid && true_peak_level(value, true, warning) == TargetLevel::Over;
    for (double at = 0; at < length; at += kStep) {
      const double size = std::min(kCell, length - at);
      QColor color(theme::kUnlit);
      if (at < lit) color = QColor(over ? theme::kMute : (at + size) / length * 60.0 - 60.0 > warning ? theme::kWarn : theme::kOk);
      p.setBrush(color);
      p.drawRect(vertical_ ? QRectF(track.x(), track.bottom() - at - size, track.width(), size) : QRectF(track.x() + at, track.y(), size, track.height()));
    }
    p.setRenderHint(QPainter::Antialiasing, true);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QColor mark(theme::kText);
    QFont scale = p.font(); scale.setFamily(QStringLiteral("Bahnschrift")); scale.setPixelSize(10);
    if (vertical_) {
      const QRectF track(27, 8, peak_ ? 5 : 7, height() - 16);
      if (peak_) paintPeakCells(p, track);
      else {
        p.setPen(Qt::NoPen); p.setBrush(QColor(theme::kUnlit)); p.drawRoundedRect(track, 2, 2);
        if (valid) {
          const double fraction = meterFraction(value);
          p.setBrush(lufsBarColor(target_level(value, target, true)));
          p.drawRoundedRect(QRectF(track.x(), track.bottom() - track.height() * fraction, track.width(), track.height() * fraction), 2, 2);
        }
      }
      if (peak_ && std::isfinite(heldPeak)) {
        const double y = track.bottom() - track.height() * meterFraction(heldPeak);
        p.setPen(QPen(QColor(theme::kPeakMark), 2)); p.drawLine(QPointF(25, y), QPointF(track.right() + 2, y));
      }
      const double ty = track.bottom() - track.height() * meterFraction(target);
      p.setPen(QPen(mark, 1)); p.drawLine(QPointF(25, ty), QPointF(track.right() + 2, ty));
      p.setFont(scale); p.setPen(QColor(theme::kDim));
      for (int i = 0; i <= 2; ++i) p.drawText(QRectF(0, 2 + (height() - 16) * i / 2.0, 22, 12), Qt::AlignRight, QString::number(-30 * i));
      return;
    }
    const QRectF track(1, 1, width() - 2, peak_ ? 5 : 7);
    if (peak_) paintPeakCells(p, track);
    else {
      p.setPen(Qt::NoPen); p.setBrush(QColor(theme::kUnlit)); p.drawRoundedRect(track, 2, 2);
      if (valid) {
        p.setBrush(lufsBarColor(target_level(value, target, true)));
        p.drawRoundedRect(QRectF(track.x(), track.y(), track.width() * meterFraction(value), track.height()), 2, 2);
      }
    }
    if (peak_ && std::isfinite(heldPeak)) {
      const double x = track.x() + track.width() * meterFraction(heldPeak);
      p.setPen(QPen(QColor(theme::kPeakMark), 2)); p.drawLine(QPointF(x, 0), QPointF(x, track.bottom() + 1));
    }
    const double tx = track.x() + track.width() * meterFraction(target);
    p.setPen(QPen(mark, 1)); p.drawLine(QPointF(tx, 0), QPointF(tx, track.bottom() + 1));
    if (compact_) return;
    p.setPen(QColor(theme::kDim)); p.setFont(scale);
    for (int i = 0; i <= 2; ++i) {
      const int x = 1 + static_cast<int>((width() - 2) * i / 2.0);
      const QString text = QString::number(-60 + 30 * i);
      const int tw = p.fontMetrics().horizontalAdvance(text);
      p.drawText(std::clamp(x - tw / 2, 0, width() - tw), 18, text);
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
        maxLabel->setStyleSheet(QStringLiteral("font-family:%1;font-size:10px;font-weight:500;color:%2;").arg(theme::kFontNum, theme::kDim));
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
      unit->setStyleSheet("padding-bottom:0px;");
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
      const QString color = finite ? readingColor(level) : QString(theme::kDim);
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
      const QString color = held ? maxColor(level) : QString(theme::kDim);
      peakMax_->setStyleSheet(QStringLiteral("font-family:%1;font-size:10px;font-weight:500;color:%2;").arg(theme::kFontNum, color));
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
  setAttribute(Qt::WA_StyledBackground, true);
  QString sheet = QStringLiteral(
    "QWidget {font-family:$ui$; font-size:12px; color:$text$;}"
    "QWidget#lufsDock,QWidget#sourcesContent {background:$bg$;}"
    "QFrame#topBar {background:$bg$; border:none; border-bottom:1px solid $line$;}"
    "QFrame#masterCard,QFrame#sourceCard {background:$plate$; border:1px solid $line$; border-radius:9px;}"
    "QLabel {border:none; background:transparent;}"
    "QLabel#masterNumber {font-family:$num$; font-size:44px; font-weight:600;}"
    "QLabel#sourceNumber {font-family:$num$; font-size:22px; font-weight:600;}"
    "QLabel#verticalMasterNumber {font-family:$num$; font-size:28px; font-weight:600;}"
    "QLabel#verticalSourceNumber {font-family:$num$; font-size:20px; font-weight:600;}"
    "QLabel#sourceName {font-size:12px; font-weight:600;}"
    "QLabel#unit {font-family:$num$; font-size:10px; font-weight:500; color:$dim$; padding-bottom:5px;}"
    "QLabel#state,QLabel#smallLabel {font-family:$num$; color:$dim$; font-size:11px; font-weight:500;}"
    "QLabel#brand {font-family:$num$; font-size:16px; font-weight:700;}"
    "QPushButton {background:$raised$; border:1px solid $line$; border-radius:7px; min-height:26px; padding:0 10px;}"
    "QPushButton:hover {border-color:$lineHi$;}"
    "QPushButton:pressed {background:$surface$;}"
    "QPushButton:disabled {color:$dim$; background:$surface$;}"
    "QPushButton:default {background:$raisedOn$; border-color:transparent; color:$onAccent$; font-weight:600;}"
    "QPushButton#gear,QPushButton#reset {background:transparent; border:none; border-radius:6px; min-height:0; padding:0;}"
    "QPushButton#gear:hover,QPushButton#reset:hover {background:$surface$;}"
    "QComboBox,QDoubleSpinBox,QLineEdit {background:$sunken$; border:1px solid $line$; border-radius:7px; min-height:26px; padding:0 8px; selection-background-color:$surfaceHi$; selection-color:$text$;}"
    "QComboBox:hover,QDoubleSpinBox:hover,QLineEdit:hover {border-color:$lineHi$;}"
    "QComboBox:disabled,QDoubleSpinBox:disabled {color:$dim$;}"
    "QComboBox QAbstractItemView,QMenu {background:$surfaceHi$; color:$text$; border:1px solid $lineHi$; selection-background-color:$surface$; selection-color:$text$;}"
    "QDialog {background:$surface$;}"
    "QWidget#settingsPage {background:$surface$;}"
    "QTabWidget::pane {background:$surface$; border:none; border-top:1px solid $line$;}"
    "QTabBar {background:transparent;}"
    "QTabBar::tab {background:transparent; color:$dim$; padding:10px 14px; border:none; border-bottom:2px solid transparent;}"
    "QTabBar::tab:hover {color:$text$;}"
    "QTabBar::tab:selected {color:$text$; border-bottom:2px solid $text$;}"
    "QCheckBox {spacing:6px;}"
    "QCheckBox::indicator,QListWidget::indicator {width:13px; height:13px; background:$sunken$; border:1px solid $lineHi$; border-radius:3px;}"
    "QCheckBox::indicator:checked,QListWidget::indicator:checked {background:$text$; border-color:$text$;}"
    "QCheckBox::indicator:disabled {border-color:$line$;}"
    "QListWidget {background:$sunken$; border:1px solid $line$; border-radius:6px; outline:none;}"
    "QListWidget::item {min-height:26px; padding:0 4px;}"
    "QListWidget::item:hover {background:$surface$;}"
    "QListWidget::item:selected {background:$surfaceHi$; color:$text$;}"
    "QToolTip {background:$surfaceHi$; color:$text$; border:1px solid $lineHi$;}"
    "QScrollArea {border:none; background:transparent;}"
    "QScrollBar:vertical {background:transparent; width:6px; margin:0;}"
    "QScrollBar:horizontal {background:transparent; height:6px; margin:0;}"
    "QScrollBar::handle:vertical {background:$lineHi$; min-height:24px; border-radius:3px;}"
    "QScrollBar::handle:horizontal {background:$lineHi$; min-width:24px; border-radius:3px;}"
    "QScrollBar::add-line,QScrollBar::sub-line {width:0; height:0;}"
    "QScrollBar::add-page,QScrollBar::sub-page {background:none;}");
  const QString plate = QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202125,stop:1 #18191c)");
  const QString raised = QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2f3135,stop:1 #222327)");
  const QString raisedOn = QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #fbfaf5,stop:1 #d6d5d0)");
  const std::pair<const char*, QString> tokens[] = {
    {"$ui$", theme::kFontUi}, {"$num$", theme::kFontNum}, {"$bg$", theme::kBg}, {"$surface$", theme::kSurface}, {"$surfaceHi$", theme::kSurfaceHi},
    {"$sunken$", theme::kSunken}, {"$line$", theme::kLine}, {"$lineHi$", theme::kLineHi}, {"$text$", theme::kText}, {"$dim$", theme::kDim},
    {"$onAccent$", theme::kOnAccent}, {"$plate$", plate}, {"$raised$", raised}, {"$raisedOn$", raisedOn}};
  for (const auto& [name, value] : tokens) sheet.replace(QLatin1String(name), value);
  setStyleSheet(sheet);
  auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
  auto* topBar = new QFrame; topBar->setObjectName("topBar"); topBar->setFixedHeight(44);
  auto* header = new QHBoxLayout(topBar); header->setContentsMargins(12, 0, 8, 0); header->setSpacing(4);
  auto* brand = new QLabel("LUFS Meter"); brand->setObjectName("brand");
  QFont brandFont = brand->font(); brandFont.setLetterSpacing(QFont::PercentageSpacing, 104); brand->setFont(brandFont);
  header->addWidget(brand); header->addStretch();
  mode_ = new QComboBox(this); mode_->addItems({QStringLiteral("3秒平均"), QStringLiteral("瞬間"), QStringLiteral("全体平均")}); mode_->hide();
  target_ = new QDoubleSpinBox(this); target_->setRange(-60, 0); target_->setDecimals(1); target_->setSingleStep(1); target_->setValue(-14); target_->hide();
  auto* resetButton = new QPushButton; resetButton->setObjectName("reset"); resetButton->setFixedSize(28, 28);
  resetButton->setToolTip(QStringLiteral("計測リセット（MAXもクリア）")); resetButton->setAccessibleName(QStringLiteral("計測リセット"));
  const QPen iconPen(QColor(theme::kText), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  QPixmap resetIcon(40, 40); resetIcon.fill(Qt::transparent); {
    QPainter p(&resetIcon); p.setRenderHint(QPainter::Antialiasing); p.scale(2, 2); p.setPen(iconPen);
    p.drawArc(QRectF(4, 4, 12, 12), 40 * 16, 280 * 16);
    p.drawPolyline(QPolygonF{QPointF(15, 3), QPointF(15, 7), QPointF(11, 6)});
  }
  resetIcon.setDevicePixelRatio(2);
  resetButton->setIcon(QIcon(resetIcon)); resetButton->setIconSize(QSize(20, 20)); header->addWidget(resetButton);
  connect(resetButton, &QPushButton::clicked, this, [this] { if (resetClicked) resetClicked(); });
  auto* gear = new QPushButton; gear->setObjectName("gear"); gear->setFixedSize(28, 28); gear->setToolTip(QStringLiteral("設定")); gear->setAccessibleName(QStringLiteral("設定"));
  // Same shape as the settings icon used in the LAA tab rail and the AI panel.
  QPixmap icon(40, 40); icon.fill(Qt::transparent); {
    QPainter p(&icon); p.setRenderHint(QPainter::Antialiasing); p.scale(2, 2); p.setPen(iconPen);
    p.drawEllipse(QPointF(10, 10), 2.5, 2.5);
    for (int i = 0; i < 8; ++i) { const double a = i * 3.141592653589793 / 4; p.drawLine(QPointF(10 + 5*std::cos(a), 10+5*std::sin(a)), QPointF(10+7.5*std::cos(a), 10+7.5*std::sin(a))); }
  }
  icon.setDevicePixelRatio(2);
  gear->setIcon(QIcon(icon)); gear->setIconSize(QSize(20, 20)); header->addWidget(gear); root->addWidget(topBar);
  auto* body = new QVBoxLayout; body->setContentsMargins(8, 8, 8, 8); body->setSpacing(6); root->addLayout(body, 1);
  masters_ = new QVBoxLayout; masters_->setSpacing(6); body->addLayout(masters_);
  status_ = new QLabel(this); status_->hide();
  sourceHeading_ = new QLabel("SOURCES"); sourceHeading_->hide(); sourceHeading_->setObjectName("smallLabel"); body->addWidget(sourceHeading_);
  auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->viewport()->setStyleSheet(QStringLiteral("background:%1;").arg(theme::kBg));
  auto* content = new QWidget; content->setObjectName("sourcesContent");
  sources_ = new QVBoxLayout(content); sources_->setContentsMargins(0, 0, 0, 0); sources_->setSpacing(4); sources_->setAlignment(Qt::AlignTop);
  empty_ = new QLabel(QStringLiteral("ソースなし")); empty_->setObjectName("smallLabel"); empty_->setWordWrap(true); sources_->addWidget(empty_);
  scroll->setWidget(content); body->addWidget(scroll, 1);
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
  // Number fields match LAA: typed or stepped with the arrow keys, with no spin buttons.
  for (auto* spin : dialog.findChildren<QDoubleSpinBox*>()) spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
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
