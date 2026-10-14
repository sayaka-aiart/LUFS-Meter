#include "dock.hpp"
#include "target.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFormLayout>
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
class LoudnessBar : public QWidget {
public:
  double value = -60, target = -14;
  bool valid = false;
  bool vertical_ = false;
  explicit LoudnessBar(QWidget* parent, bool vertical = false) : QWidget(parent), vertical_(vertical) {
    if (vertical) { setFixedWidth(40); setMinimumHeight(130); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding); }
    else setFixedHeight(16);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (vertical_) {
      const QRectF track(29, 8, 3, height() - 16);
      p.setPen(Qt::NoPen); p.setBrush(QColor("#2b3037")); p.drawRoundedRect(track, 2, 2);
      if (valid) {
        const double fraction = std::clamp((value + 60.0) / 60.0, 0.0, 1.0);
        p.setBrush(accent(target_level(value, target, true)));
        p.drawRoundedRect(QRectF(track.x(), track.bottom() - track.height() * fraction, 3, track.height() * fraction), 1.5, 1.5);
      }
      const double ty = track.bottom() - track.height() * std::clamp((target + 60.0) / 60.0, 0.0, 1.0);
      p.setPen(QPen(QColor("#d1d4d9"), 1)); p.drawLine(QPointF(25, ty), QPointF(36, ty));
      QFont font = p.font(); font.setPixelSize(9); p.setFont(font); p.setPen(QColor("#6c727e"));
      for (int i = 0; i <= 2; ++i) p.drawText(QRectF(0, 2 + (height() - 16) * i / 2.0, 22, 12), Qt::AlignRight, QString::number(-30 * i));
      return;
    }
    const QRectF track(1, 1, width() - 2, 3);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#2b3037")); p.drawRoundedRect(track, 2, 2);
    if (valid) {
      const double fraction = std::clamp((value + 60.0) / 60.0, 0.0, 1.0);
      p.setBrush(accent(target_level(value, target, true)));
      p.drawRoundedRect(QRectF(track.x(), track.y(), track.width() * fraction, track.height()), 2, 2);
    }
    const double tx = track.x() + track.width() * std::clamp((target + 60.0) / 60.0, 0.0, 1.0);
    p.setPen(QPen(QColor("#d1d4d9"), 1)); p.drawLine(QPointF(tx, 0), QPointF(tx, 8));
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
  QLabel *name_, *number_, *state_;
  LoudnessBar* bar_;
  QString id_;
  double targetValue_ = -14;
  bool master_;
  bool vertical_;
  std::function<void(const QString&, double)> changeTarget;
  explicit MeterCard(bool master, QWidget* parent, bool vertical = false) : QFrame(parent), master_(master), vertical_(vertical) {
    setObjectName(master ? "masterCard" : "sourceCard");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(master ? 16 : 10, master ? 12 : 6, master ? 16 : 10, master ? 10 : 5);
    layout->setSpacing(master ? 7 : 3);
    auto* top = new QHBoxLayout; top->setSpacing(8);
    name_ = new QLabel; name_->setTextFormat(Qt::PlainText); name_->setObjectName("sourceName"); name_->setWordWrap(true);
    state_ = new QLabel; state_->setObjectName("state");
    top->addWidget(name_, 1);
    number_ = new QLabel("—"); number_->setObjectName(master ? "masterNumber" : "sourceNumber");
    auto* unit = new QLabel("LUFS"); unit->setObjectName("unit");
    if (vertical) {
      setFixedWidth(master ? 102 : 78); setMinimumHeight(300); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
      layout->setContentsMargins(8, 12, 8, 10); layout->setSpacing(7);
      name_->setAlignment(Qt::AlignCenter); name_->setFixedHeight(20); layout->addWidget(name_);
      state_->setAlignment(Qt::AlignCenter); state_->setFixedHeight(11); layout->addWidget(state_);
      bar_ = new LoudnessBar(this, true); layout->addWidget(bar_, 1, Qt::AlignHCenter);
      number_->setObjectName(master ? "verticalMasterNumber" : "verticalSourceNumber"); number_->setAlignment(Qt::AlignCenter); number_->setFixedHeight(38); layout->addWidget(number_);
      unit->setAlignment(Qt::AlignCenter); unit->setFixedHeight(11); layout->addWidget(unit);
      delete top;
    } else {
    if (master) {
      top->addWidget(state_); layout->addLayout(top);
      auto* numeric = new QHBoxLayout; numeric->setSpacing(9); numeric->addWidget(number_); numeric->addWidget(unit, 0, Qt::AlignBottom); numeric->addStretch(); layout->addLayout(numeric);
    } else {
      top->addWidget(state_); top->addWidget(number_); unit->setParent(this); unit->hide(); layout->addLayout(top);
    }
    bar_ = new LoudnessBar(this); layout->addWidget(bar_);
    }
  }
  void apply(const RowData& data, int mode) {
    id_ = data.id; targetValue_ = data.target; name_->setText(data.name);
    const double value = mode == 0 ? data.shortTerm : mode == 1 ? data.momentary : data.integrated;
    const bool valid = data.available && std::isfinite(value);
    number_->setText(valid ? QString::number(value, 'f', 1) : QStringLiteral("—"));
    const auto level = target_level(value, data.target, valid);
    const QString color = level == TargetLevel::Unavailable ? "#646c79" : level == TargetLevel::Over || level == TargetLevel::Near ? accent(level).name() : "#e2e5e9";
    number_->setStyleSheet("color:" + color + ";");
    QString state;
    if (!valid) state = data.state == QStringLiteral("ミュート") ? "MUTE" : data.state == QStringLiteral("計測準備中") ? QStringLiteral("準備中") : "—";
    if (master_ && valid) {
      const auto match = QRegularExpression(QStringLiteral("トラック(\\d+)")).match(data.note);
      if (match.hasMatch()) state = "T" + match.captured(1);
    }
    state_->setText(state); state_->setVisible(vertical_ || !state.isEmpty());
    setToolTip(data.note + "\n" + data.state + QStringLiteral("\n目標 %1 LUFS").arg(data.target, 0, 'f', 1));
    bar_->setToolTip(QStringLiteral("目標 %1 LUFS · 3 LU手前から黄色、超過で赤").arg(data.target, 0, 'f', 1));
    bar_->valid = valid; bar_->value = value; bar_->target = data.target; bar_->update();
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
    "QPushButton,QComboBox,QDoubleSpinBox {background:#1b2027; border:1px solid #333b47; border-radius:5px; padding:5px 7px;}"
    "QPushButton:hover {background:#29313b; border-color:#566474;}"
    "QPushButton:pressed {background:#13171c;}"
    "QPushButton#gear {background:transparent; border:none;} QPushButton#gear:hover {background:#252c35;}"
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
void Dock::setOrientation(int value) {
  value = std::clamp(value, 0, 1); if (value == orientation_) return;
  for (auto* card : masterCards_) { masters_->removeWidget(card); sources_->removeWidget(card); delete card; }
  for (auto* card : sourceCards_) { sources_->removeWidget(card); delete card; }
  masterCards_.clear(); sourceCards_.clear(); orientation_ = value;
  sourceHeading_->hide(); sources_->setDirection(value ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
  if (orientationChanged) orientationChanged(value);
}
void Dock::updateRows(const std::vector<RowData>& masters, const std::vector<RowData>& sources, const QString& status) {
  auto apply = [this](const std::vector<RowData>& data, std::vector<MeterCard*>& cards, QVBoxLayout* layout, bool master) {
    while (cards.size() > data.size()) { auto* card = cards.back(); cards.pop_back(); layout->removeWidget(card); delete card; }
    while (cards.size() < data.size()) { auto* card = new MeterCard(master, this, orientation_ == 1); if (master && orientation_) layout->insertWidget(static_cast<int>(cards.size()), card); else layout->addWidget(card); cards.push_back(card); }
    for (std::size_t i = 0; i < data.size(); ++i) cards[i]->apply(data[i], mode());
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
  auto* mode = new QComboBox; mode->addItems({QStringLiteral("3秒平均"), QStringLiteral("瞬間（0.4秒）"), QStringLiteral("全体平均")}); mode->setCurrentIndex(this->mode()); form->addRow(QStringLiteral("測定表示"), mode);
  auto* target = new QDoubleSpinBox; target->setObjectName("commonTarget"); target->setRange(-60, 0); target->setDecimals(1); target->setSuffix(" LUFS"); target->setValue(target_->value()); form->addRow(QStringLiteral("共通の目標"), target);
  auto* direction = new QComboBox; direction->addItems({QStringLiteral("横"), QStringLiteral("縦")}); direction->setCurrentIndex(orientation_); form->addRow(QStringLiteral("メーター"), direction);
  auto* reset = new QPushButton(QStringLiteral("計測リセット")); form->addRow(QString(), reset); connect(reset, &QPushButton::clicked, this, [this] { if (resetClicked) resetClicked(); });
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
    QSignalBlocker block(sourceTarget); sourceTarget->setEnabled(current); inherit->setEnabled(current);
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
  tabs->addTab(sourcePage, QStringLiteral("ソース"));
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() == QDialog::Accepted) {
    setMode(mode->currentIndex()); setTarget(target->value()); setOrientation(direction->currentIndex());
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
