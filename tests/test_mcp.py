from pathlib import Path
import io
import json
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts'))
import lufs_meter_mcp as mcp


def snapshot():
    return {'schemaVersion': 1, 'app': 'LUFS Meter', 'status': 'running', 'exportEnabled': True,
            'updatedAtUnixMs': time.time() * 1000, 'instanceId': 'obs-a', 'profile': 'test', 'collection': 'test',
            'sources': [{'id': 'voice-id', 'name': 'マイク', 'lufs': {'shortTerm': -16.5}}], 'masters': []}


class McpContract(unittest.TestCase):
    def test_stdio_lifecycle_and_errors(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / '測定.json'
            path.write_text(json.dumps(snapshot()), encoding='utf-8')
            messages = [
                {'jsonrpc': '2.0', 'id': 0, 'method': 'tools/list'},
                {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'protocolVersion': '2025-06-18'}},
                {'jsonrpc': '2.0', 'method': 'notifications/initialized'},
                {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'},
                {'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call', 'params': {'name': 'meter_read', 'arguments': {'source': 'マイク'}}},
                {'jsonrpc': '2.0', 'id': 4, 'method': 'tools/call', 'params': {'name': 'obs_set_volume', 'arguments': {'source': 'マイク', 'volume_db': -9, 'apply': False}}},
                {'jsonrpc': '2.0', 'id': 5, 'method': 'tools/call', 'params': {'name': 'meter_read', 'arguments': {'command': 'forbidden'}}},
                {'jsonrpc': '2.0', 'id': 6, 'method': 'does/not/exist'},
            ]
            result = subprocess.run([sys.executable, '-X', 'utf8', str(Path(mcp.__file__)), '--file', str(path)],
                                    input='\n'.join(json.dumps(x) for x in messages) + '\n{bad json}\n',
                                    text=True, encoding='utf-8', capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            replies = [json.loads(line) for line in result.stdout.splitlines()]
            self.assertEqual(len(replies), len(messages))  # notification silent, malformed JSON responds
            self.assertEqual(replies[0]['error']['code'], -32002)
            self.assertEqual(replies[1]['result']['protocolVersion'], '2025-06-18')
            self.assertEqual(len(replies[2]['result']['tools']), 8)
            data = json.loads(replies[3]['result']['content'][0]['text'])
            self.assertEqual(data['sources'][0]['id'], 'voice-id')
            self.assertTrue(replies[4]['result']['isError'])
            self.assertTrue(replies[5]['result']['isError'])
            self.assertEqual(replies[6]['error']['code'], -32601)
            self.assertEqual(replies[7]['error']['code'], -32700)

    def test_context_freshness_and_backup_guards(self):
        adapter = mcp.Adapter()
        data = snapshot()
        args = {'source': 'マイク', 'volume_db': -9, 'apply': True}
        with patch.object(adapter, 'read', return_value=data), patch.object(adapter, 'bridge', return_value={'context': {'profile': 'OTHER', 'collection': 'test'}}) as bridge:
            with self.assertRaisesRegex(ValueError, 'do not match'):
                adapter.call('obs_set_volume', args)
            self.assertEqual([call.args[0] for call in bridge.call_args_list], ['Inspect'])
        with patch.object(adapter, 'read', side_effect=[data, {**data, 'instanceId': 'obs-b'}]), patch.object(adapter, 'bridge', return_value={'context': {'profile': 'test', 'collection': 'test'}}) as bridge:
            with self.assertRaisesRegex(ValueError, 'context changed'):
                adapter.call('obs_set_volume', args)
            self.assertEqual(bridge.call_count, 1)
        with patch.object(adapter, 'read', return_value=data), patch.object(adapter, 'bridge', side_effect=[{'context': {'profile': 'test', 'collection': 'test'}}, {'verified': True}]) as bridge:
            self.assertTrue(adapter.call('obs_set_volume', args)['verified'])
            call = bridge.call_args_list[1]
            self.assertEqual(call.args, ('SetVolume',))
            self.assertEqual(call.kwargs['Source'], 'voice-id')
            self.assertTrue(call.kwargs['Apply'])
            self.assertTrue(Path(call.kwargs['BackupPath']).is_absolute())
            self.assertNotEqual(adapter.checkpoint_path(), adapter.checkpoint_path())
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'stopped.json'
            path.write_text(json.dumps({**data, 'status': 'stopped'}))
            stopped = mcp.Adapter(path)
            with patch.object(stopped, 'bridge') as bridge:
                with self.assertRaises(ValueError):
                    stopped.call('obs_set_volume', args)
                bridge.assert_not_called()

    def test_sample_duration_and_argument_validation(self):
        adapter = mcp.Adapter()
        with patch.object(adapter, 'read', return_value=snapshot()), patch.object(mcp.time, 'sleep') as sleep:
            result = adapter.call('meter_sample', {'seconds': 1, 'interval': 5})
            self.assertEqual(len(result['samples']), 2)
            self.assertGreater(sleep.call_args.args[0], .9)
        for name, args in [('obs_set_volume', {'source': 'x', 'volume_db': float('nan'), 'apply': True}),
                           ('meter_sample', {'seconds': 31}), ('meter_read', {'source': 'x', 'master': True})]:
            with self.assertRaises(ValueError):
                mcp.validate(name, args)


if __name__ == '__main__':
    unittest.main()
