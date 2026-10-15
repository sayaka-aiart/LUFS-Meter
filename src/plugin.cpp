#include "meter.hpp"
#include "dock.hpp"
#include "routing.hpp"
#include "telemetry.hpp"
#include <QDateTime>
#include <QUuid>
#include <obs-module.h>
#include <obs-hotkey.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char* obs_module_name(void) { return "LUFS Meter"; }
MODULE_EXPORT const char* obs_module_description(void) { return "Independent source and stream-master LUFS dock (libebur128)"; }
namespace {
struct SourceTap {
  obs_source_t* source;
  std::unique_ptr<lufs::Meter> meter;
  explicit SourceTap(obs_source_t* s, unsigned rate, unsigned channels) : source(obs_get_source_by_uuid(obs_source_get_uuid(s))), meter(std::make_unique<lufs::Meter>(rate, channels)) {
    obs_source_add_audio_capture_callback(source, capture, this);
  }
  ~SourceTap() { obs_source_remove_audio_capture_callback(source, capture, this); obs_source_release(source); }
  static void capture(void* arg, obs_source_t* source, const audio_data* audio, bool muted) {
    auto* self = static_cast<SourceTap*>(arg);
    const auto* planes = reinterpret_cast<const float* const*>(audio->data);
    // Source capture is after filters/balance, before OBS fader. Apply current user gain to our copy only.
    self->meter->capture(planes, audio->frames, obs_source_get_volume(source), muted);
  }
};
struct MixTap {
  unsigned index;
  audio_t* audio;
  bool connected = false;
  std::unique_ptr<lufs::Meter> meter;
  MixTap(unsigned idx, audio_t* a, unsigned rate, speaker_layout layout) : index(idx), audio(a), meter(std::make_unique<lufs::Meter>(rate, get_audio_channels(layout))) {
    audio_convert_info conversion{}; conversion.format = AUDIO_FORMAT_FLOAT_PLANAR;
    conversion.samples_per_sec = rate; conversion.speakers = layout; conversion.allow_clipping = false;
    connected = audio_output_connect(audio, index, &conversion, capture, this);
  }
  ~MixTap() { if (connected) audio_output_disconnect(audio, index, capture, this); }
  static void capture(void* arg, std::size_t, audio_data* data) {
    auto* self = static_cast<MixTap*>(arg);
    self->meter->capture(reinterpret_cast<const float* const*>(data->data), data->frames);
  }
};
struct SourceInfo { QString id, name; };
std::vector<SourceInfo> enumerate_sources() {
  std::vector<SourceInfo> result;
  obs_enum_sources([](void* arg, obs_source_t* source) {
    if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) && !obs_source_removed(source)) {
      auto& result = *static_cast<std::vector<SourceInfo>*>(arg);
      result.push_back({QString::fromUtf8(obs_source_get_uuid(source)), QString::fromUtf8(obs_source_get_name(source))});
    }
    return true;
  }, &result);
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name.localeAwareCompare(b.name) < 0; });
  return result;
}
class Runtime {
public:
  QPointer<lufs::Dock> dock;
  QPointer<QTimer> timer;
  std::unique_ptr<QSettings> settings;
  QString collectionKey;
  QStringList selected;
  bool explicitSelection = false;
  std::mutex metersMutex;
  std::map<QString, std::unique_ptr<SourceTap>> sources;
  std::map<unsigned, std::unique_ptr<MixTap>> mixes;
  std::atomic<bool> running{true};
  std::thread worker;
  unsigned rate = 0, channels = 0;
  audio_t* currentAudio = nullptr;
  std::uint8_t configuredMask = 0, observedMask = 0, mask = 0;
  bool live = false;
  QString routeNote;
  unsigned ticks = 0;
  bool qaSaved = false;
  QString exportPath, exportError;
  QString instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  qint64 lastExportMs = 0;
  quint64 exportSequence = 0;
  bool exportWritten = false;
  obs_hotkey_id resetHotkey = OBS_INVALID_HOTKEY_ID;
  Runtime() {
    char* path = obs_module_config_path("settings.ini");
    QDir().mkpath(QFileInfo(QString::fromUtf8(path)).absolutePath());
    settings = std::make_unique<QSettings>(QString::fromUtf8(path), QSettings::IniFormat); bfree(path);
    exportPath = QFileInfo(settings->fileName()).dir().filePath("readings.json");
    dock = new lufs::Dock; dock->setExportPath(exportPath);
    dock->setExportEnabled(settings->value("exportEnabled", false).toBool());
    dock->exportEnabledChanged = [this](bool enabled) {
      settings->setValue("exportEnabled", enabled); settings->sync();
      if (!enabled) publish({}, {}, "disabled", true); else { lastExportMs = 0; refresh(); }
    };
    dock->setMode(settings->value("displayMode", 0).toInt());
    dock->setTarget(settings->value("target", -14.0).toDouble());
    dock->setOrientation(settings->value("orientation", 0).toInt());
    dock->setTruePeakWarning(settings->value("truePeakWarning", -6.0).toDouble());
    dock->truePeakWarningChanged = [this](double value) { settings->setValue("truePeakWarning", value); settings->sync(); refresh(); };
    dock->setMetrics(settings->value("showLufs", true).toBool(), settings->value("showTruePeak", false).toBool());
    dock->metricsChanged = [this](bool lufs, bool peak) { settings->setValue("showLufs", lufs); settings->setValue("showTruePeak", peak); settings->sync(); refresh(); };
    dock->orientationChanged = [this](int value) { settings->setValue("orientation", value); refresh(); };
    dock->targetChanged = [this](double value) { settings->setValue("target", value); refresh(); };
    dock->rowTargetChanged = [this](const QString& id, double value) { const auto key = collectionKey + "/targets/" + id; if (std::isfinite(value)) settings->setValue(key, value); else settings->remove(key); refresh(); };
    dock->modeChanged = [this](int mode) { settings->setValue("displayMode", mode); };
    dock->selectionChanged = [this](const QStringList& ids) {
      selected = ids; explicitSelection = true;
      settings->setValue(collectionKey + "/selected", selected); settings->sync(); refresh();
    };
    dock->resetClicked = [this] { resetMeters(); };
    if (!obs_frontend_add_dock_by_id("obs-lufs-meter", "LUFS Meter", dock)) {
      delete dock; dock = nullptr; blog(LOG_ERROR, "[obs-lufs-meter] dock registration failed"); return;
    }
    resetHotkey = obs_hotkey_register_frontend("obs-lufs-meter.reset", "LUFS Meter: Reset measurements", [](void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed) { if (pressed) static_cast<Runtime*>(data)->resetMeters(); }, this);
    worker = std::thread([this] {
      while (running.load(std::memory_order_acquire)) {
        { std::lock_guard<std::mutex> lock(metersMutex);
          for (auto& pair : sources) pair.second->meter->process();
          for (auto& pair : mixes) pair.second->meter->process(); }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });
    timer = new QTimer(dock); QObject::connect(timer, &QTimer::timeout, dock, [this] { refresh(); }); timer->start(100);
    blog(LOG_INFO, "[obs-lufs-meter] native dock registered; libebur128 1.2.6; read-only measurement");
  }
  ~Runtime() {
    if (resetHotkey != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(resetHotkey);
    if (timer) timer->stop();
    if (exportWritten) publish({}, {}, "stopped", true);
    running.store(false, std::memory_order_release); if (worker.joinable()) worker.join();
    { std::lock_guard<std::mutex> lock(metersMutex); sources.clear(); mixes.clear(); }
    if (dock) { dock->selectionChanged = {}; dock->modeChanged = {}; dock->resetClicked = {}; dock->targetChanged = {}; dock->rowTargetChanged = {}; dock->orientationChanged = {}; dock->metricsChanged = {}; dock->truePeakWarningChanged = {}; dock->exportEnabledChanged = {}; obs_frontend_remove_dock("obs-lufs-meter"); }
    settings->sync();
  }
  void resetMeters() {
    std::lock_guard<std::mutex> lock(metersMutex);
    for (auto& pair : sources) pair.second->meter->reset();
    for (auto& pair : mixes) pair.second->meter->reset();
  }
  void publish(const std::vector<lufs::RowData>& masters, const std::vector<lufs::RowData>& rows, const QString& status = "running", bool force = false) {
    const bool enabled = dock ? dock->exportEnabled() : settings->value("exportEnabled", false).toBool();
    if (!enabled && !force) return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!force && now >= lastExportMs && now - lastExportMs < 500) return;
    lastExportMs = now;
    char* profile = obs_frontend_get_current_profile(); char* collection = obs_frontend_get_current_scene_collection();
    QJsonObject metadata{{"pluginVersion", LUFS_METER_VERSION}, {"instanceId", instanceId}, {"obsPid", static_cast<double>(QCoreApplication::applicationPid())},
      {"sequence", static_cast<double>(++exportSequence)}, {"status", status}, {"exportEnabled", enabled},
      {"profile", QString::fromUtf8(profile ? profile : "")}, {"collection", QString::fromUtf8(collection ? collection : "")},
      {"channels", static_cast<int>(channels)}, {"streaming", live}, {"configuredTrackMask", configuredMask}, {"effectiveTrackMask", mask},
      {"settings", QJsonObject{{"lufsMode", dock ? dock->mode() : settings->value("displayMode", 0).toInt()}, {"truePeakWarningDbtp", dock ? dock->truePeakWarning() : settings->value("truePeakWarning", -6).toDouble()}, {"showLufs", dock ? dock->showsLufs() : settings->value("showLufs", true).toBool()}, {"showTruePeak", dock ? dock->showsTruePeak() : settings->value("showTruePeak", false).toBool()}}}};
    bfree(profile); bfree(collection);
    QString error;
    if (lufs::writeTelemetry(exportPath, lufs::telemetrySnapshot(masters, rows, metadata, rate), &error)) { exportWritten = true; exportError.clear(); }
    else if (error != exportError) { exportError = error; blog(LOG_WARNING, "[obs-lufs-meter] JSON export failed: %s", error.toUtf8().constData()); }
  }
  void loadCollection() {
    char* name = obs_frontend_get_current_scene_collection();
    const QString key = "collections/" + QCryptographicHash::hash(QByteArray(name ? name : ""), QCryptographicHash::Sha256).toHex(); bfree(name);
    if (key == collectionKey) return;
    { std::lock_guard<std::mutex> lock(metersMutex); sources.clear(); }
    collectionKey = key; explicitSelection = settings->contains(key + "/selected");
    selected = settings->value(key + "/selected").toStringList();
  }
  void resolveRouting() {
    configuredMask = 0; observedMask = 0;
    config_t* config = obs_frontend_get_profile_config();
    bool multiple = false;
    obs_service_t* service = obs_frontend_get_streaming_service();
    const char* protocol = service ? obs_service_get_protocol(service) : nullptr;
    if (protocol) { const QString p = QString::fromUtf8(protocol).toLower(); multiple = p.startsWith("srt") || p.startsWith("rist"); }
    if (config) {
      const char* mode = config_get_string(config, "Output", "Mode");
      configuredMask = lufs::configured_tracks(mode && std::strcmp(mode, "Advanced") == 0, multiple,
        static_cast<unsigned>(config_get_uint(config, "AdvOut", "TrackIndex")),
        static_cast<unsigned>(config_get_uint(config, "AdvOut", "StreamMultiTrackAudioMixes")));
    }
    live = obs_frontend_streaming_active();
    obs_output_t* output = obs_frontend_get_streaming_output();
    if (output && live) {
      for (std::size_t slot = 0; slot < (multiple ? 6u : 1u); ++slot) {
        obs_encoder_t* encoder = obs_output_get_audio_encoder(output, slot);
        if (encoder) { const auto index = obs_encoder_get_mixer_index(encoder); if (index < 6) observedMask |= 1u << index; }
      }
      if (!observedMask) observedMask = static_cast<std::uint8_t>(obs_output_get_mixers(output) & 63);
    }
    if (output) obs_output_release(output);
    mask = lufs::effective_tracks(configuredMask, live, observedMask);
    routeNote = live ? QStringLiteral("配信中 · 実際の出力トラック") : QStringLiteral("配信停止中 · 配信設定のトラックを計測");
    if (live && observedMask != configuredMask) routeNote += QStringLiteral(" · 設定と出力のトラックが異なります");
    if (!mask) routeNote = QStringLiteral("配信トラックを確認できません。OBSの出力設定を確認してください。");
  }
  lufs::RowData row(const QString& id, const QString& name, const QString& note, lufs::Meter& meter, bool muted = false, bool active = true) {
    const auto r = meter.reading(); lufs::RowData data; data.id = id; data.name = name; data.note = note;
    data.target = settings->value(collectionKey + "/targets/" + id, settings->value("target", -14.0)).toDouble();
    data.momentary = r.momentary; data.shortTerm = r.shortTerm; data.integrated = r.integrated; data.truePeak = r.truePeak; data.truePeakMax = r.truePeakMax;
    const auto now = lufs::now_ms();
    const bool fresh = r.lastMs && now <= r.lastMs + 1000;
    data.fresh = fresh; data.active = active; data.muted = muted || r.muted; data.error = r.error;
    data.frames = r.frames; data.resets = r.resets; data.dropped = r.dropped;
    data.ageMs = r.lastMs ? static_cast<qint64>(now - r.lastMs) : -1;
    const double value = dock->mode() == 0 ? r.shortTerm : dock->mode() == 1 ? r.momentary : r.integrated;
    const auto threshold = dock->mode() == 0 ? rate * 3ULL : static_cast<std::uint64_t>(rate * .4);
    data.truePeakAvailable = fresh && active && !muted && !r.muted && !r.error && r.frames > 0;
    data.available = fresh && active && !muted && !r.muted && !r.error && r.frames >= threshold;
    if (r.error) data.state = QStringLiteral("計測エラー");
    else if (muted || r.muted) data.state = QStringLiteral("ミュート");
    else if (!active || !fresh) data.state = QStringLiteral("音声停止");
    else if (r.frames < (dock->showsLufs() ? threshold : 1ULL)) data.state = QStringLiteral("計測準備中");
    else if (!std::isfinite(dock->showsLufs() ? value : r.truePeak)) data.state = QStringLiteral("無音");
    else data.state = QStringLiteral("計測中");
    if (r.dropped) data.note += QStringLiteral(" · 欠落後に計測再開（%1）").arg(r.dropped);
    return data;
  }
  void refresh() {
    if (!dock) return;
    const auto stopFile = qEnvironmentVariable("LUFS_METER_QA_STOP_FILE");
    if (!qEnvironmentVariableIsEmpty("LUFS_METER_QA_DIR") && !stopFile.isEmpty() && QFileInfo::exists(stopFile)) { QTimer::singleShot(0, qApp, &QApplication::quit); return; }
    loadCollection(); resolveRouting();
    obs_audio_info info{};
    if (!obs_get_audio_info(&info)) { dock->updateRows({}, {}, QStringLiteral("OBSの音声初期化を待っています")); publish({}, {}, "audio_unavailable"); return; }
    const auto newChannels = get_audio_channels(info.speakers);
    auto* audio = obs_get_audio();
    if (newChannels > 2 || newChannels < 1) {
      std::lock_guard<std::mutex> lock(metersMutex); sources.clear(); mixes.clear();
      dock->updateRows({}, {}, QStringLiteral("モノラル / ステレオ対応です。OBSの音声設定を確認してください。")); publish({}, {}, "unsupported_channels"); return;
    }
    auto available = enumerate_sources();
    if (!explicitSelection) { selected.clear(); for (const auto& s : available) if (selected.size() < 32) selected.append(s.id); }
    std::vector<lufs::SourceOption> options;
    for (const auto& id : selected) for (const auto& s : available) if (s.id == id) options.push_back({s.id, s.name, true});
    for (const auto& s : available) if (!selected.contains(s.id)) options.push_back({s.id, s.name, false});
    for (auto& option : options) { const auto key = collectionKey + "/targets/" + option.id; option.target = settings->value(key, settings->value("target", -14.0)).toDouble(); option.customTarget = settings->contains(key); }
    dock->setSourceOptions(options);
    std::vector<lufs::RowData> masterRows, sourceRows;
    {
      std::lock_guard<std::mutex> lock(metersMutex);
      if (rate != info.samples_per_sec || channels != newChannels || currentAudio != audio) {
        sources.clear(); mixes.clear(); rate = info.samples_per_sec; channels = newChannels; currentAudio = audio;
      }
      for (auto it = sources.begin(); it != sources.end();) {
        const bool exists = std::any_of(available.begin(), available.end(), [&](const auto& s) { return s.id == it->first; });
        if (!exists || !selected.contains(it->first)) it = sources.erase(it); else ++it;
      }
      for (const auto& id : selected) {
        const auto found = std::find_if(available.begin(), available.end(), [&](const auto& s) { return s.id == id; });
        if (found == available.end()) continue;
        if (!sources.count(id)) {
          auto* source = obs_get_source_by_uuid(id.toUtf8().constData());
          if (!source) continue;
          sources.emplace(id, std::make_unique<SourceTap>(source, rate, channels)); obs_source_release(source);
        }
        auto& tap = *sources.at(id);
        const bool routed = (obs_source_get_audio_mixers(tap.source) & mask) && obs_source_get_monitoring_type(tap.source) != OBS_MONITORING_TYPE_MONITOR_ONLY;
        sourceRows.push_back(row(id, found->name, routed ? QStringLiteral("配信トラックに送信") : QStringLiteral("配信トラックへの送信なし"), *tap.meter,
          obs_source_muted(tap.source) || !obs_source_enabled(tap.source) || obs_source_get_volume(tap.source) == 0, obs_source_active(tap.source)));
        sourceRows.back().routed = routed; sourceRows.back().volumeGain = obs_source_get_volume(tap.source);
      }
      for (auto it = mixes.begin(); it != mixes.end();) if (!(mask & (1u << it->first))) it = mixes.erase(it); else ++it;
      for (unsigned index = 0; index < 6; ++index) if (mask & (1u << index)) {
        if (!mixes.count(index)) mixes.emplace(index, std::make_unique<MixTap>(index, audio, rate, info.speakers));
        auto& tap = *mixes.at(index);
        auto data = row("mix-" + QString::number(index), QStringLiteral("配信マスター"), QStringLiteral("トラック%1 · %2").arg(index + 1).arg(live ? "実出力から自動選択" : "配信設定から自動選択"), *tap.meter);
        if (!tap.connected) { data.error = true; data.available = false; data.truePeakAvailable = false; data.state = QStringLiteral("音声取得エラー"); }
        data.routed = true; masterRows.push_back(data);
      }
    }
    dock->updateRows(masterRows, sourceRows, routeNote);
    std::vector<lufs::SourceOption> masterOptions;
    for (const auto& row : masterRows) masterOptions.push_back({row.id, row.name, true, row.target, settings->contains(collectionKey + "/targets/" + row.id)});
    dock->setMasterOptions(std::move(masterOptions));
    publish(masterRows, sourceRows);
    ++ticks;
    // Opt-in, local QA evidence only. No audio/source/output mutation.
    const QString qa = qEnvironmentVariable("LUFS_METER_QA_DIR");
    if (!qa.isEmpty() && ticks >= 85 && !qaSaved) {
      qaSaved = true; QDir().mkpath(qa); dock->setFixedSize(dock->orientation() ? (dock->showsLufs() && dock->showsTruePeak() ? 660 : 480) : 340, dock->orientation() ? 400 : 480); dock->layout()->activate(); dock->grab().save(qa + "/obs-dock.png");
      QJsonObject report; report["nativeDock"] = true; report["rate"] = static_cast<int>(rate); report["channels"] = static_cast<int>(channels);
      report["configuredMask"] = configuredMask; report["observedMask"] = observedMask; report["effectiveMask"] = mask; report["live"] = live; report["showLufs"] = dock->showsLufs(); report["showTruePeak"] = dock->showsTruePeak(); report["truePeakWarning"] = dock->truePeakWarning();
      auto encode = [](const std::vector<lufs::RowData>& rows) { QJsonArray array; for (const auto& r : rows) {
        QJsonObject obj; obj["id"] = r.id; obj["name"] = r.name; obj["state"] = r.state; obj["note"] = r.note;
        obj["shortTerm"] = std::isfinite(r.shortTerm) ? QJsonValue(r.shortTerm) : QJsonValue(); obj["truePeak"] = std::isfinite(r.truePeak) ? QJsonValue(r.truePeak) : QJsonValue(); obj["truePeakMax"] = std::isfinite(r.truePeakMax) ? QJsonValue(r.truePeakMax) : QJsonValue(); obj["truePeakAvailable"] = r.truePeakAvailable; obj["available"] = r.available; obj["target"] = r.target; array.append(obj); } return array; };
      report["masters"] = encode(masterRows); report["sources"] = encode(sourceRows);
      QFile file(qa + "/obs-readings.json"); if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(report).toJson());
      blog(LOG_INFO, "[obs-lufs-meter] QA snapshot written");
      if (qEnvironmentVariableIsSet("LUFS_METER_QA_EXIT")) QTimer::singleShot(100, qApp, &QApplication::quit);
    }
  }
};
std::unique_ptr<Runtime> runtime;
void on_event(obs_frontend_event event, void*) {
  if ((event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP || event == OBS_FRONTEND_EVENT_PROFILE_CHANGING) && runtime) {
    if (runtime->timer) runtime->timer->stop();
    if (runtime->dock && runtime->dock->exportEnabled()) runtime->publish({}, {}, "configuration_changing", true);
    std::lock_guard<std::mutex> lock(runtime->metersMutex); runtime->sources.clear(); runtime->mixes.clear();
  }
  if ((event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED || event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) && runtime && runtime->timer) runtime->timer->start(100);
  if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) runtime.reset();
}
}
bool obs_module_load(void) { return true; }
void obs_module_post_load(void) {
  obs_queue_task(OBS_TASK_UI, [](void*) {
    runtime = std::make_unique<Runtime>(); obs_frontend_add_event_callback(on_event, nullptr);
  }, nullptr, false);
}
void obs_module_unload(void) {
  obs_frontend_remove_event_callback(on_event, nullptr); runtime.reset();
}
