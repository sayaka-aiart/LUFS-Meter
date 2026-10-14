"""Prepare a new isolated OBS tree and synthetic local audio fixtures only."""
from pathlib import Path
import argparse, json, math, shutil, struct, uuid, wave

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--obs-root', default=r'C:\Program Files\obs-studio')
args = parser.parse_args()
runtime = root / 'qa' / 'obs-portable'
runtime.mkdir(parents=True, exist_ok=True)
for name in ('bin', 'data', 'obs-plugins'):
    shutil.copytree(Path(args.obs_root) / name, runtime / name, dirs_exist_ok=True)
# Only the minimal official media/filter/output plugins in this disposable runtime.
plugin_dir = runtime / 'obs-plugins' / '64bit'
allowed = {'obs-lufs-meter.dll', 'obs-ffmpeg.dll', 'obs-filters.dll', 'obs-outputs.dll', 'obs-transitions.dll', 'obs-x264.dll', 'rtmp-services.dll'}
for candidate in plugin_dir.glob('*.dll'):
    if candidate.name not in allowed:
        candidate.replace(candidate.with_suffix('.dll.disabled'))
shutil.copy2(root / 'build/Release/obs-lufs-meter.dll', plugin_dir / 'obs-lufs-meter.dll')
config = runtime / 'config/obs-studio'
profile = config / 'basic/profiles/LUFS-QA'
scenes = config / 'basic/scenes'
profile.mkdir(parents=True, exist_ok=True)
scenes.mkdir(parents=True, exist_ok=True)
ini = '''[General]
FirstRun=true
EnableAutoUpdates=false
ConfirmOnExit=false
WarnedAboutClosingDocks=true
InfoIncrement=1
MaxLogs=10
BrowserHWAccel=false
SysTrayEnabled=false
SysTrayWhenStarted=false

[Basic]
Profile=LUFS-QA
ProfileDir=LUFS-QA
SceneCollection=LUFS-QA
SceneCollectionFile=LUFS-QA
ConfigOnNewProfile=false
'''
for name in ('global.ini', 'user.ini'):
    (config / name).write_text(ini, encoding='utf-8')
(profile / 'basic.ini').write_text('''[General]
Name=LUFS-QA

[Output]
Mode=Advanced

[AdvOut]
TrackIndex=2
VodTrackEnabled=false

[Audio]
SampleRate=48000
ChannelSetup=Stereo

[Video]
BaseCX=1280
BaseCY=720
OutputCX=1280
OutputCY=720
FPSType=0
FPSCommon=30
''', encoding='utf-8')
sources, items = [], []
specs = [('マイク', 1000, .3, .5, 2, False),
         ('BGM', 500, .15, 1., 2, False),
         ('ゲーム音', 1500, .1, 1., 1, False),
         ('効果音', 1000, .1, 1., 2, True)]
for i, (name, frequency, amplitude, gain, mixers, muted) in enumerate(specs):
    path = root / 'qa' / f'tone-{i}.wav'
    # 12 seconds, two identical channels. WAV quantisation error is negligible for LUFS.
    with wave.open(str(path), 'wb') as output:
        output.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        samples = bytearray()
        for f in range(12 * 48000):
            value = round(amplitude * math.sin(2 * math.pi * frequency * f / 48000) * 32767)
            samples += struct.pack('<hh', value, value)
        output.writeframes(samples)
    uid = str(uuid.uuid5(uuid.NAMESPACE_URL, 'lufs-qa-source-' + str(i)))
    source = dict(name=name, uuid=uid, id='ffmpeg_source', versioned_id='ffmpeg_source',
                  settings=dict(is_local_file=True, local_file=path.as_posix(), looping=True,
                                restart_on_activate=True, clear_on_media_end=True, close_when_inactive=False, speed_percent=100),
                  mixers=mixers, sync=0, flags=0, volume=gain, balance=.5, enabled=True, muted=muted,
                  monitoring_type=0, filters=[], private_settings={})
    source.update({'push-to-mute': False, 'push-to-talk': False, 'hotkeys': {}})
    sources.append(source)
    items.append(dict(name=name, source_uuid=uid, visible=True, locked=False, rot=0., align=5, bounds_type=0,
                      pos=dict(x=0.,y=0.), scale=dict(x=1.,y=1.), bounds=dict(x=0.,y=0.), id=i+1,
                      blend_method='default', blend_type='normal', scale_filter='disable', private_settings={}))
sources.append(dict(name='LUFS-QA', uuid=str(uuid.uuid5(uuid.NAMESPACE_URL,'lufs-qa-scene')), id='scene', versioned_id='scene',
                    settings=dict(id_counter=len(items), custom_size=False, items=items), mixers=0,
                    sync=0, flags=0, volume=1., balance=.5, enabled=True, muted=False, private_settings={}))
scene = dict(current_scene='LUFS-QA', current_program_scene='LUFS-QA', scene_order=[{'name':'LUFS-QA'}],
             name='LUFS-QA', sources=sources, groups=[], quick_transitions=[], transitions=[], saved_projectors=[],
             canvases=[], current_transition='', transition_duration=300, preview_locked=False,
             scaling_enabled=False, modules={}, version=2)
(scenes / 'LUFS-QA.json').write_text(json.dumps(scene, ensure_ascii=False, indent=2), encoding='utf-8')
(runtime / 'portable_mode.txt').write_text('', encoding='ascii')
print(runtime)
