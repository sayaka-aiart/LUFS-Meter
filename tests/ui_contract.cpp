#include "dock.hpp"
#include <QApplication>
#include <QComboBox>
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
  lufs::RowData master; master.id = "mix-1"; master.name = "Master"; master.available = true; master.shortTerm = -13.8;
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
    dialog->reject();
  });
  gear->click(); if (changed || failure) return 3;
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
  std::cout << "PASS: gear only, cancel, target/mode/source selection, horizontal/vertical switch\n";
  return 0;
}
