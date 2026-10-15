from pathlib import Path
import importlib.util
import json
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("read_meter", Path(__file__).resolve().parents[1] / "scripts/read_meter.py")
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)


class ReaderContract(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "readings.json"
        self.data = {"schemaVersion": 1, "app": "LUFS Meter", "updatedAtUnixMs": 10000,
                     "exportEnabled": True, "status": "running",
                     "masters": [{"id": "mix-1"}], "sources": [{"id": "mic-id", "name": "マイク"}]}
        self.save()

    def save(self):
        self.path.write_text(json.dumps(self.data, ensure_ascii=False), encoding="utf-8")

    def test_live_read_and_selection(self):
        data = reader.read_snapshot(self.path, now_ms=10500)
        self.assertTrue(data["reader"]["fresh"])
        self.assertEqual(reader.select_rows(data, "マイク")["sources"][0]["id"], "mic-id")
        self.assertEqual(reader.select_rows(data, master=True)["sources"], [])
        self.assertEqual(len(data["sources"]), 1)

    def test_stop_disable_pause_and_expiry_are_not_live(self):
        for status in ("stopped", "disabled", "configuration_changing", "audio_unavailable"):
            self.data["status"] = status
            self.save()
            with self.assertRaises(reader.MeterReadError):
                reader.read_snapshot(self.path, now_ms=10500)
        self.data["status"] = "running"
        self.data["exportEnabled"] = False
        self.save()
        with self.assertRaises(reader.MeterReadError):
            reader.read_snapshot(self.path, now_ms=10500)
        self.assertFalse(reader.read_snapshot(self.path, now_ms=10500, allow_stale=True)["reader"]["fresh"])
        self.data["exportEnabled"] = True
        self.save()
        for now in (13000, 8000):
            with self.assertRaises(reader.MeterReadError):
                reader.read_snapshot(self.path, now_ms=now)

    def test_bad_json_schema_and_ambiguous_source(self):
        for payload in ('{"x":', 'null', '{"schemaVersion":2}', '{"x":NaN}'):
            self.path.write_text(payload, encoding="utf-8")
            with self.assertRaises(reader.MeterReadError):
                reader.read_snapshot(self.path, now_ms=10500)
        self.save()
        self.data["sources"].append({"id": "other", "name": "マイク"})
        with self.assertRaises(reader.MeterReadError):
            reader.select_rows(self.data, "マイク")
        self.assertEqual(reader.select_rows(self.data, "mic-id")["sources"][0]["id"], "mic-id")


if __name__ == "__main__":
    unittest.main()
