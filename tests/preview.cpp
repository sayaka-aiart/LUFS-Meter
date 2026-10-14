#include "dock.hpp"
#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QPushButton>
#include <QDialog>
#include <cmath>
#include <limits>
int main(int argc, char** argv) {
  QApplication app(argc, argv); lufs::Dock dock; dock.resize(340, 480);
  auto data = [](QString id, QString name, double value, QString note, QString state = "計測中") {
    lufs::RowData row; row.id = id; row.name = name; row.note = note; row.state = state;
    row.shortTerm = value; row.momentary = value + 0.7; row.integrated = value - 0.5; row.available = std::isfinite(value); return row;
  };
  std::vector<lufs::RowData> masters = {data("master", QStringLiteral("配信マスター"), -13.4, QStringLiteral("トラック2 · 配信設定から自動選択"))};
  std::vector<lufs::RowData> sources = {
    data("mic", QStringLiteral("マイク"), -16.0, QStringLiteral("配信トラックに送信")),
    data("game", QStringLiteral("ゲーム音"), -25.4, QStringLiteral("配信トラックに送信")),
    data("bgm", "BGM", -30.1, QStringLiteral("配信トラックに送信")),
    data("fx", QStringLiteral("効果音"), std::numeric_limits<double>::quiet_NaN(), QStringLiteral("配信トラックに送信"), QStringLiteral("ミュート"))};
  dock.setSourceOptions({{"mic", QStringLiteral("マイク"), true}, {"game", QStringLiteral("ゲーム音"), true}, {"bgm", "BGM", true}, {"fx", QStringLiteral("効果音"), true}});
  dock.setMasterOptions({{"master", QStringLiteral("配信マスター"), true}});
  auto redraw = [&] { dock.updateRows(masters, sources, QStringLiteral("画面プレビュー · 表示値はサンプルです")); };
  dock.modeChanged = [&](int) { redraw(); };
  dock.targetChanged = [&](double value) { for (auto& r : masters) r.target = value; for (auto& r : sources) r.target = value; redraw(); };
  dock.rowTargetChanged = [&](const QString& id, double value) { for (auto& r : masters) if (r.id == id) r.target = value; for (auto& r : sources) if (r.id == id) r.target = value; redraw(); };
  dock.orientationChanged = [&](int) { redraw(); };
  if (argc > 2 && QString::fromLocal8Bit(argv[2]) == "vertical") { dock.setOrientation(1); dock.resize(480, 400); }
  redraw(); dock.show();
  if (argc > 1) { const QString path = QString::fromLocal8Bit(argv[1]);
    if (argc > 2 && QString::fromLocal8Bit(argv[2]) == "settings") {
      QTimer::singleShot(100, [&] { dock.findChild<QPushButton*>("gear")->click(); });
      QTimer::singleShot(400, [&, path] { if (auto* dialog = qobject_cast<QDialog*>(app.activeModalWidget())) { dialog->grab().save(path); dialog->reject(); } app.quit(); });
    } else QTimer::singleShot(400, [&, path] { dock.grab().save(path); app.quit(); }); }
  return app.exec();
}
