#include "dock.hpp"
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
  lufs::RowData master; master.id = "mix-1"; master.name = "Master"; master.available = true; master.shortTerm = -13.8; master.truePeak = -0.5; master.truePeakAvailable = true;
  lufs::RowData mic = master; mic.id = "mic"; mic.name = "Mic"; mic.shortTerm = -16.5;
  dock.updateRows({master}, {mic}, "Local UI contract");
  double target = -14; int direction = 0; QStringList selected; bool changed = false, failure = false;
  dock.targetChanged = [&](double value) { target = value; changed = true; };
  dock.orientationChanged = [&](int value) { direction = value; dock.updateRows({master}, {mic}, ""); changed = true; };
  dock.selectionChanged = [&](const QStringList& value) { selected = value; changed = true; };
  dock.show();
  auto* gear = dock.findChild<QPushButton*>("gear"); if (!gear || gear->width() > 30) return 1;
  // Main panel only exposes the gear; the mode/target controls live in the dialog.
  if (dock.findChild<QComboBox*>()->isVisible() || dock.findChild<QDoubleSpinBox*>()->isVisible()) return 2;
  QTimer::singleShot(50, [&] {
    auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget()); if (!dialog) { failure = true; app.quit(); return; }
    for (auto* spin : dialog->findChildren<QDoubleSpinBox*>()) spin->setValue(-18);
    dialog->findChild<QCheckBox*>("showTruePeak")->setChecked(true);
    dialog->reject();
  });
  gear->click(); if (changed || failure || !dock.showsLufs() || dock.showsTruePeak()) return 3;
  QTimer::singleShot(50, [&] {
    auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget()); if (!dialog) { failure = true; app.quit(); return; }
    const auto combos = dialog->findChildren<QComboBox*>();
    for (auto* combo : combos) { if (combo->count() == 3) combo->setCurrentIndex(1); else if (combo->count() == 2) combo->setCurrentIndex(1); }
    dialog->findChild<QDoubleSpinBox*>("commonTarget")->setValue(-18);
    auto* list = dialog->findChild<QListWidget*>();
    for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole).toString() == "bgm") list->item(i)->setCheckState(Qt::Unchecked);
    dialog->accept();
  });
  gear->click();
  if (failure || target != -18 || direction != 1 || dock.mode() != 1 || selected != QStringList{"mic"}) {
    std::cerr << "settings: target=" << target << " direction=" << direction << " mode=" << dock.mode() << " sources=" << selected.join(',').toStdString() << '\n'; return 4;
  }
  dock.setOrientation(0); dock.updateRows({master}, {mic}, "");
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
      dialog->findChild<QComboBox*>("orientation")->setCurrentIndex(vertical ? 1 : 0);
      dialog->accept();
    });
    gear->click();
    if (dock.showsLufs() != lufs || dock.showsTruePeak() != peak || dock.orientation() != int(vertical)) failure = true;
    dock.updateRows({master}, {mic}, "");
    if (dock.findChildren<QWidget*>("lufsMetric").size() != (lufs ? 2 : 0)) failure = true;
    if (dock.findChildren<QWidget*>("truePeakMetric").size() != (peak ? 2 : 0)) failure = true;
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
  dock.setMetrics(true, true); dock.setOrientation(1);
  auto quiet = master; quiet.truePeak = -100; dock.updateRows({quiet}, {quiet}, ""); dock.layout()->activate(); app.processEvents(); dock.updateRows({quiet}, {quiet}, "");
  for (auto* label : dock.findChildren<QLabel*>()) if (label->property("metric").toString() == "truePeak") {
    if (label->text() != "-100.0" || QFontMetricsF(label->font()).horizontalAdvance(label->text()) > label->width()) { std::cerr << "quiet number=" << label->text().toStdString() << " width=" << label->width() << " textWidth=" << QFontMetricsF(label->font()).horizontalAdvance(label->text()) << " pixels=" << label->font().pixelSize() << " style=" << label->styleSheet().toStdString() << "\n"; return 9; }
  }
  std::cout << "PASS: LUFS/both/True Peak in both orientations, cancel, at least one metric, warmup/mute; gear only, cancel, target/mode/source selection, horizontal/vertical switch\n";
  return 0;
}
