#include "dock.hpp"
#include "telemetry.hpp"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <cmath>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFrame>
#include <QLayout>
#include <QFontMetricsF>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <iostream>
int main(int argc, char** argv) {
  QApplication app(argc, argv); lufs::Dock dock; dock.resize(420, 680);
  dock.setSourceOptions({{"mic", "Mic", true}, {"bgm", "BGM", true}});
  dock.setMasterOptions({{"mix-1", "Master", true}});
  lufs::RowData master; master.id = "mix-1"; master.name = "Master"; master.available = true; master.shortTerm = -13.8; master.truePeak = -0.5; master.truePeakAvailable = true; master.truePeakMax = 1.2;
  lufs::RowData mic = master; mic.id = "mic"; mic.name = "Mic"; mic.shortTerm = -16.5;
  dock.updateRows({master}, {mic}, "Local UI contract");
  double peakWarning = -6; dock.truePeakWarningChanged = [&](double value) { peakWarning = value; };
  double target = -14; int direction = 0; QStringList selected; bool changed = false, failure = false;
  dock.targetChanged = [&](double value) { target = value; changed = true; };
  dock.orientationChanged = [&](int value) { direction = value; dock.updateRows({master}, {mic}, ""); changed = true; };
  dock.selectionChanged = [&](const QStringList& value) { selected = value; changed = true; };
  dock.show();
  auto* gear = dock.findChild<QPushButton*>("gear"); if (!gear || gear->width() > 30) return 1;
  int resets = 0; dock.resetClicked = [&] { ++resets; };
  auto* resetButton = dock.findChild<QPushButton*>("reset"); app.processEvents();
  if (!resetButton || !resetButton->isVisible() || resetButton->width() > 30 || resetButton->geometry().right() >= gear->geometry().left()) return 14;
  resetButton->click(); if (resets != 1) return 15;
  // Main panel exposes reset and settings; mode/target controls live in the dialog.
  if (dock.findChild<QComboBox*>()->isVisible() || dock.findChild<QDoubleSpinBox*>()->isVisible()) return 2;
  QTimer::singleShot(50, [&] {
    auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget()); if (!dialog) { failure = true; app.quit(); return; }
    for (auto* spin : dialog->findChildren<QDoubleSpinBox*>()) spin->setValue(-18);
    dialog->findChild<QCheckBox*>("showTruePeak")->setChecked(true);
    dialog->findChild<QCheckBox*>("exportEnabled")->setChecked(true);
    dialog->reject();
  });
  gear->click(); if (changed || failure || !dock.showsLufs() || dock.showsTruePeak() || dock.truePeakWarning() != -6 || peakWarning != -6 || dock.exportEnabled()) return 3;
  QTimer::singleShot(50, [&] {
    auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget()); if (!dialog) { failure = true; app.quit(); return; }
    const auto combos = dialog->findChildren<QComboBox*>();
    for (auto* combo : combos) { if (combo->count() == 3) combo->setCurrentIndex(1); else if (combo->count() == 2) combo->setCurrentIndex(1); }
    dialog->findChild<QDoubleSpinBox*>("commonTarget")->setValue(-18);
    dialog->findChild<QCheckBox*>("showTruePeak")->setChecked(true);
    dialog->findChild<QDoubleSpinBox*>("truePeakWarning")->setValue(-12);
    auto* list = dialog->findChild<QListWidget*>();
    for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole).toString() == "bgm") list->item(i)->setCheckState(Qt::Unchecked);
    dialog->accept();
  });
  gear->click();
  if (failure || target != -18 || direction != 1 || dock.mode() != 1 || selected != QStringList{"mic"} || dock.truePeakWarning() != -12 || peakWarning != -12) {
    std::cerr << "settings: target=" << target << " direction=" << direction << " mode=" << dock.mode() << " sources=" << selected.join(',').toStdString() << '\n'; return 4;
  }
  dock.setOrientation(0); dock.updateRows({master}, {mic}, "");
  dock.setMetrics(true, false);
  int metricChanges = 0;
  dock.metricsChanged = [&](bool, bool) { ++metricChanges; changed = true; };
  auto choose = [&](bool lufs, bool peak, bool vertical) {
    QTimer::singleShot(30, [&] {
      auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget());
      if (!dialog) { failure = true; app.quit(); return; }
      auto* lc = dialog->findChild<QCheckBox*>("showLufs");
      auto* pc = dialog->findChild<QCheckBox*>("showTruePeak");
      if (lufs) lc->setChecked(true);
      if (peak) pc->setChecked(true);
      if (!lufs) lc->setChecked(false);
      if (!peak) pc->setChecked(false);
      if ((!lufs && pc->isEnabled()) || (!peak && lc->isEnabled())) failure = true;
      if (dialog->findChild<QComboBox*>("lufsMode")->isEnabled() != lufs) failure = true;
      if (dialog->findChild<QDoubleSpinBox*>("truePeakWarning")->isEnabled() != peak) failure = true;
      dialog->findChild<QComboBox*>("orientation")->setCurrentIndex(vertical ? 1 : 0);
      dialog->accept();
    });
    gear->click();
    if (dock.showsLufs() != lufs || dock.showsTruePeak() != peak || dock.orientation() != int(vertical)) failure = true;
    dock.updateRows({master}, {mic}, "");
    if (dock.findChildren<QWidget*>("lufsMetric").size() != (lufs ? 2 : 0)) failure = true;
    if (dock.findChildren<QWidget*>("truePeakMetric").size() != (peak ? 2 : 0)) failure = true;
    for (auto* label : dock.findChildren<QLabel*>("truePeakMax")) if (label->text() != "MAX 1.2" || !label->styleSheet().contains("#eb7779")) failure = true;
    dock.layout()->activate(); app.processEvents();
    if (lufs && peak) for (auto* card : dock.findChildren<QFrame*>()) {
      if (card->objectName() != "masterCard" && card->objectName() != "sourceCard") continue;
      auto* left = card->findChild<QWidget*>("lufsMetric"); auto* right = card->findChild<QWidget*>("truePeakMetric");
      if (!left || !right) { failure = true; continue; }
      const auto lp = left->mapTo(card, QPoint()); const auto rp = right->mapTo(card, QPoint());
      if (vertical ? !(lp.x() < rp.x() && lp.y() == rp.y()) : !(lp.y() < rp.y() && lp.x() == rp.x())) failure = true;
    }
    for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak" && label->text() != "-0.5") failure = true;
  };
  for (bool vertical : {false, true}) { choose(true, true, vertical); choose(false, true, vertical); choose(true, false, vertical); }
  if (failure || metricChanges != 6) return 5;
  dock.setMetrics(false, false); if (!dock.showsLufs() || dock.showsTruePeak()) return 6;
  dock.setMetrics(false, true);
  auto preparing = master; preparing.available = false; preparing.state = QStringLiteral("計測準備中");
  dock.updateRows({preparing}, {}, "");
  for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak" && label->text() != "-0.5") return 7;
  preparing.truePeakAvailable = false; preparing.state = QStringLiteral("ミュート");
  dock.updateRows({preparing}, {}, "");
  for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak" && label->text() != QStringLiteral("—")) return 8;
  for (auto* label : dock.findChildren<QLabel*>("truePeakMax")) if (label->text() != "MAX 1.2") return 10;
  dock.setMetrics(false, true);
  auto warningRow = master; warningRow.truePeak = -10; dock.updateRows({warningRow}, {}, "");
  dock.setTruePeakWarning(-6);
  for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak" && !label->styleSheet().contains("#e2e5e9")) return 11;
  dock.setTruePeakWarning(-12);
  for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak" && !label->styleSheet().contains("#e2c27b")) return 12;
  dock.setMetrics(true, true); dock.setOrientation(1);
  auto quiet = master; quiet.truePeak = -100; dock.updateRows({quiet}, {quiet}, ""); dock.layout()->activate(); app.processEvents(); dock.updateRows({quiet}, {quiet}, "");
  for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak") {
    if (label->text() != "-100.0" || QFontMetricsF(label->font()).horizontalAdvance(label->text()) > label->width()) { std::cerr << "quiet number=" << label->text().toStdString() << " width=" << label->width() << " textWidth=" << QFontMetricsF(label->font()).horizontalAdvance(label->text()) << " pixels=" << label->font().pixelSize() << " style=" << label->styleSheet().toStdString() << "\n"; return 9; }
  }
  auto cleared = master; cleared.truePeakMax = std::numeric_limits<double>::quiet_NaN();
  dock.updateRows({cleared}, {}, "");
  for (auto* label : dock.findChildren<QLabel*>("truePeakMax")) if (label->text() != QStringLiteral("MAX —")) return 13;
  bool exportChanged = false; dock.exportEnabledChanged = [&](bool value) { exportChanged = value; };
  QTimer::singleShot(30, [&] { auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget()); if (!dialog) { failure = true; return; } dialog->findChild<QCheckBox*>("exportEnabled")->setChecked(true); dialog->accept(); });
  gear->click(); if (failure || !dock.exportEnabled() || !exportChanged) return 16;
  auto exported = master; exported.fresh = true; exported.frames = 48000 * 4; exported.ageMs = 5; exported.momentary = -14; exported.integrated = -15;
  QJsonObject meta{{"status", "running"}, {"exportEnabled", true}};
  auto snapshot = lufs::telemetrySnapshot({exported}, {exported}, meta, 48000);
  auto row = snapshot["sources"].toArray()[0].toObject();
  if (snapshot["schemaVersion"].toInt() != 1 || row["lufs"].toObject()["shortTerm"].toDouble() != -13.8 || row["truePeakMaxDbtp"].toDouble() != 1.2) return 17;
  exported.muted = true;
  row = lufs::telemetrySnapshot({}, {exported}, meta, 48000)["sources"].toArray()[0].toObject();
  if (!row["truePeakDbtp"].isNull() || !row["lufs"].toObject()["shortTerm"].isNull() || row["truePeakMaxDbtp"].toDouble() != 1.2 || row["status"] != "muted") return 18;
  exported.muted = false; exported.frames = 480; exported.truePeak = -std::numeric_limits<double>::infinity();
  row = lufs::telemetrySnapshot({}, {exported}, meta, 48000)["sources"].toArray()[0].toObject();
  if (!row["lufs"].toObject()["momentary"].isNull() || !row["truePeakDbtp"].isNull()) return 19;
  QTemporaryDir temporary; const auto exportFile = temporary.filePath("readings.json"); QString error;
  if (!lufs::writeTelemetry(exportFile, snapshot, &error)) return 20;
  auto stopped = lufs::telemetrySnapshot({}, {}, QJsonObject{{"status", "stopped"}}, 48000);
  if (!lufs::writeTelemetry(exportFile, stopped, &error)) return 21;
  QFile input(exportFile); if (!input.open(QIODevice::ReadOnly) || QJsonDocument::fromJson(input.readAll()).object()["status"] != "stopped") return 22;
  if (lufs::writeTelemetry(temporary.path(), snapshot, &error) || error.isEmpty()) return 23;
  std::cout << "PASS: external export save/cancel, full schema, muted/null/held values, atomic publication; header reset left of gear; MAX labels survive mute, custom warning save/cancel and live recolor; LUFS/both/True Peak in both orientations, cancel, at least one metric, warmup/mute; gear only, cancel, target/mode/source selection, horizontal/vertical switch\n";
  return 0;
}
