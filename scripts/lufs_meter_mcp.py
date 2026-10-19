"""LUFS Meter local stdio MCP adapter. Python 3.10+, standard library only.

SPDX-License-Identifier: GPL-2.0-or-later
"""
from pathlib import Path
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
import uuid

from read_meter import default_path, read_snapshot, select_rows

PROTOCOLS = ('2024-11-05', '2025-03-26', '2025-06-18', '2025-11-25')


def tool(name, description, properties=None, required=(), read_only=True):
    return {'name': name, 'description': description,
            'inputSchema': {'type': 'object', 'properties': properties or {},
                            'required': list(required), 'additionalProperties': False},
            'annotations': {'readOnlyHint': read_only, 'destructiveHint': not read_only,
                            'idempotentHint': read_only, 'openWorldHint': False}}


STRING = {'type': 'string', 'minLength': 1}
CONFIRM = {'type': 'boolean', 'description': 'Must be true to apply the requested change.'}
TOOLS = [
    tool('meter_read', 'Read fresh source/master LUFS, True Peak and MAX; inspect row readiness before use.',
         {'source': STRING, 'master': {'type': 'boolean'}}),
    tool('meter_sample', 'Record fresh readings for up to 30 seconds. Announce the measurement period first.',
         {'seconds': {'type': 'number', 'minimum': 1, 'maximum': 30},
          'interval': {'type': 'number', 'minimum': .5, 'maximum': 5},
          'source': STRING, 'master': {'type': 'boolean'}}),
    tool('obs_inspect', 'List OBS inputs, or inspect one input volume, mute and filters. No changes.', {'source': STRING}),
    tool('obs_set_volume', 'Set an input fader in dB, not a LUFS target. Save a new checkpoint and verify.',
         {'source': STRING, 'volume_db': {'type': 'number', 'minimum': -60, 'maximum': 6}, 'apply': CONFIRM},
         ('source', 'volume_db', 'apply'), False),
    tool('obs_set_filter', 'Change scalar settings on an existing filter. Save a checkpoint and verify.',
         {'source': STRING, 'filter_name': STRING, 'settings': {'type': 'object'}, 'apply': CONFIRM},
         ('source', 'filter_name', 'settings', 'apply'), False),
    tool('obs_create_filter', 'Add a named filter with scalar settings. Save a checkpoint and verify.',
         {'source': STRING, 'filter_name': STRING, 'filter_kind': STRING, 'settings': {'type': 'object'}, 'apply': CONFIRM},
         ('source', 'filter_name', 'filter_kind', 'settings', 'apply'), False),
    tool('obs_restore', 'Restore a checkpoint, saving the current state to a new checkpoint first.',
         {'backup_path': STRING, 'apply': CONFIRM}, ('backup_path', 'apply'), False),
    tool('meter_reset', 'Clear all LUFS histories and True Peak MAX. Wait for warmup before comparing.',
         {'apply': CONFIRM}, ('apply',), False),
]


def validate(name, args):
    definition = next((item for item in TOOLS if item['name'] == name), None)
    if definition is None:
        raise ValueError('Unknown tool')
    schema = definition['inputSchema']
    if not isinstance(args, dict) or set(args) - set(schema['properties']):
        raise ValueError('Invalid tool arguments')
    if set(schema['required']) - set(args):
        raise ValueError('Missing required arguments')
    for key, value in args.items():
        prop = schema['properties'][key]
        kind = prop['type']
        valid = ((kind == 'string' and isinstance(value, str) and bool(value) and '\0' not in value)
                 or (kind == 'boolean' and type(value) is bool)
                 or (kind == 'object' and isinstance(value, dict))
                 or (kind == 'number' and type(value) in (int, float) and math.isfinite(value)))
        if not valid or (kind == 'number' and not prop['minimum'] <= value <= prop['maximum']):
            raise ValueError('Invalid argument: ' + key)
    if args.get('source') and args.get('master'):
        raise ValueError('Choose source or master, not both')
    if not definition['annotations']['readOnlyHint'] and args.get('apply') is not True:
        raise ValueError('Changes require apply=true')


class Adapter:
    def __init__(self, file=None, endpoint='ws://127.0.0.1:4455', backup_dir=None, pwsh='pwsh'):
        self.file = file
        self.endpoint = endpoint
        base = Path(os.environ.get('LOCALAPPDATA', Path.home())) / 'LUFS-Meter' / 'backups'
        self.backup_dir = Path(backup_dir) if backup_dir else base
        self.pwsh = pwsh

    def read(self, args=None):
        args = args or {}
        data = read_snapshot(self.file if self.file else default_path())
        return select_rows(data, args.get('source'), args.get('master', False))

    def bridge(self, action, **values):
        executable = shutil.which(self.pwsh)
        if not executable:
            raise ValueError('PowerShell 7 not found; set --pwsh to its full executable path.')
        command = [executable, '-NoProfile', '-NonInteractive', '-File',
                   str(Path(__file__).with_name('obs_control.ps1')), '-Endpoint', self.endpoint, '-Action', action]
        for key, value in values.items():
            command.append('-' + key)
            if value is not True:
                command.append(str(value))
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=25, shell=False,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        if result.returncode:
            # The existing bridge emits JSON errors, never its password.
            try:
                message = json.loads(result.stderr).get('error', 'OBS request failed')
            except (ValueError, AttributeError):
                message = 'OBS bridge failed; check PowerShell 7, WebSocket and authentication.'
            raise ValueError(message)
        return json.loads(result.stdout)

    def checkpoint_path(self):
        return str((self.backup_dir / (time.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex + '.json')).resolve())

    def call(self, name, args):
        validate(name, args)
        if name == 'meter_read':
            return self.read(args)
        if name == 'meter_sample':
            seconds, interval = args.get('seconds', 30), args.get('interval', .5)
            start = time.monotonic()
            samples = [self.read(args)]
            for index in range(1, math.ceil(seconds / interval) + 1):
                time.sleep(max(0, start + min(index * interval, seconds) - time.monotonic()))
                samples.append(self.read(args))
            return {'samples': samples, 'seconds': seconds, 'interval': interval}
        if name == 'obs_inspect':
            return self.bridge('Inspect', **({'Source': args['source']} if args.get('source') else {}))
        snapshot = self.read()
        source = args.get('source')
        if name == 'obs_restore':
            saved = json.loads(Path(args['backup_path']).read_text(encoding='utf-8-sig'))
            source = saved.get('sourceUuid')
            if not isinstance(source, str) or not source:
                raise ValueError('Checkpoint has no source UUID')
        if source:
            selected = select_rows(snapshot, source)['sources'][0]
            source = selected['id']
        inspection = self.bridge('Inspect', **({'Source': source} if source else {}))
        context = inspection.get('context', {})
        if context.get('profile') != snapshot.get('profile') or context.get('collection') != snapshot.get('collection'):
            raise ValueError('Meter JSON and OBS WebSocket profile/collection do not match.')
        # Re-check freshness after inspection, before requesting a change.
        current = self.read()
        if any(current.get(key) != snapshot.get(key) for key in ('instanceId', 'profile', 'collection')):
            raise ValueError('OBS context changed; inspect again before applying.')
        if name == 'meter_reset':
            return self.bridge('ResetMeters', Apply=True)
        values = {'Apply': True, 'BackupPath': self.checkpoint_path()}
        if name == 'obs_restore':
            return self.bridge('Restore', FromBackup=str(Path(args['backup_path']).resolve()), **values)
        values['Source'] = source
        if name == 'obs_set_volume':
            return self.bridge('SetVolume', VolumeDb=args['volume_db'], **values)
        values.update(FilterName=args['filter_name'], SettingsJson=json.dumps(args['settings'], allow_nan=False))
        if name == 'obs_create_filter':
            return self.bridge('CreateFilter', FilterKind=args['filter_kind'], **values)
        return self.bridge('SetFilter', **values)


def run(adapter, incoming, outgoing):
    initialized = False
    ready = False

    def send(payload):
        outgoing.write(json.dumps(payload, ensure_ascii=False, allow_nan=False) + '\n')
        outgoing.flush()

    while True:
        line = incoming.readline(1_048_577)
        if not line:
            return
        if len(line) > 1_048_576:
            return  # Do not parse an oversized or incomplete message.
        request_id = None
        try:
            request = json.loads(line, parse_constant=lambda x: (_ for _ in ()).throw(ValueError('Nonfinite number')))
        except ValueError:
            send({'jsonrpc': '2.0', 'id': None, 'error': {'code': -32700, 'message': 'Parse error'}})
            continue
        if (not isinstance(request, dict) or request.get('jsonrpc') != '2.0'
                or not isinstance(request.get('method'), str)
                or ('id' in request and type(request['id']) not in (int, str))):
            send({'jsonrpc': '2.0', 'id': None, 'error': {'code': -32600, 'message': 'Invalid request'}})
            continue
        method = request['method']
        if 'id' not in request:
            if method == 'notifications/initialized' and initialized:
                ready = True
            continue
        request_id = request['id']
        params = request.get('params', {})
        response = {'jsonrpc': '2.0', 'id': request_id}
        try:
            if not isinstance(params, dict):
                raise ValueError('params must be an object')
            if method == 'initialize' and not initialized:
                proposed = params.get('protocolVersion')
                response['result'] = {'protocolVersion': proposed if proposed in PROTOCOLS else PROTOCOLS[-1],
                                      'capabilities': {'tools': {'listChanged': False}},
                                      'serverInfo': {'name': 'lufs-meter', 'version': '1.0.0'},
                                      'instructions': 'Inspect measurements and OBS context first. Changes require apply=true and automatic backups. Announce sampling so the user can speak.'}
                initialized = True
            elif method == 'ping':
                response['result'] = {}
            elif not ready:
                response['error'] = {'code': -32002, 'message': 'Initialize the server first'}
            elif method == 'tools/list':
                response['result'] = {'tools': TOOLS}
            elif method == 'tools/call':
                try:
                    data = adapter.call(params.get('name'), params.get('arguments', {}))
                    response['result'] = {'content': [{'type': 'text', 'text': json.dumps(data, ensure_ascii=False, allow_nan=False)}], 'isError': False}
                except (ValueError, OSError, subprocess.SubprocessError) as error:
                    response['result'] = {'content': [{'type': 'text', 'text': str(error)}], 'isError': True}
            else:
                response['error'] = {'code': -32601, 'message': 'Method not found'}
        except ValueError as error:
            response['error'] = {'code': -32602, 'message': str(error)}
        send(response)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--file', type=Path)
    parser.add_argument('--endpoint', default='ws://127.0.0.1:4455')
    parser.add_argument('--backup-dir', type=Path)
    parser.add_argument('--pwsh', default='pwsh')
    args = parser.parse_args()
    sys.stdin.reconfigure(encoding='utf-8')
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
    try:
        return run(Adapter(args.file, args.endpoint, args.backup_dir, args.pwsh), sys.stdin, sys.stdout)
    except (BrokenPipeError, KeyboardInterrupt):
        return 0


if __name__ == '__main__':
    raise SystemExit(main())
