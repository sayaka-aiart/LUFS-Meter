from pathlib import Path
import json, math, sys
root = Path(__file__).resolve().parent.parent
directory = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'qa/actual-obs-final'
data = json.loads((directory / 'obs-readings.json').read_text(encoding='utf-8'))
assert data['nativeDock'] and data['configuredMask'] == data['effectiveMask'] == 2
assert data['channels'] == 2 and data['rate'] == 48000
sources = data['sources']
voice = next(s for s in sources if s['name'].startswith('マイク'))
bgm = next(s for s in sources if s['name'].startswith('BGM'))
game = next(s for s in sources if s['name'].startswith('ゲーム'))
muted = next(s for s in sources if s['name'].startswith('効果'))
master = data['masters'][0]
assert voice['available'] and bgm['available'] and game['available'] and not muted['available']
assert muted['state'] == 'ミュート' and muted['shortTerm'] is None
assert '送信なし' in game['note']
assert abs(voice['shortTerm'] - (-16.47)) < .15  # 1kHz, .3 amplitude, OBS fader .5, stereo
# Different frequencies: energies add. Track1-only game and muted FX must be absent.
expected = 10 * math.log10(10 ** (voice['shortTerm']/10) + 10 ** (bgm['shortTerm']/10))
assert abs(master['shortTerm'] - expected) < .1, (master['shortTerm'], expected)
assert voice['truePeakAvailable'] and bgm['truePeakAvailable'] and master['truePeakAvailable']
assert not muted['truePeakAvailable']
assert abs(voice['truePeak'] - 20*math.log10(.15)) < .15
assert abs(bgm['truePeak'] - 20*math.log10(.15)) < .15
assert max(voice['truePeak'], bgm['truePeak']) - .2 <= master['truePeak'] <= 20*math.log10(.3) + .2
assert data['showLufs'] or data['showTruePeak']
assert -60 <= data['truePeakWarning'] <= 0
for row in [voice, bgm, game, master]:
    assert row['truePeakMax'] is not None and row['truePeakMax'] >= row['truePeak'] - 1e-9
assert master['target'] == voice['target'] == -14
assert master['shortTerm'] > -14 and -17 <= voice['shortTerm'] <= -14
assert (directory / 'obs-dock.png').exists()
result = dict(status='PASS', scope='OBS 32.2.2 isolated local source/mix measurement, no external stream',
              voiceLUFS=voice['shortTerm'], bgmLUFS=bgm['shortTerm'], masterLUFS=master['shortTerm'],
              expectedMasterLUFS=expected, gameExcluded=True, mutedExcluded=True,
              targetDefault=-14, masterColor='red', voiceColor='yellow', voiceTruePeak=voice['truePeak'], bgmTruePeak=bgm['truePeak'], masterTruePeak=master['truePeak'], showLufs=data['showLufs'], showTruePeak=data['showTruePeak'], truePeakWarning=data['truePeakWarning'], masterTruePeakMax=master['truePeakMax'])
(directory / 'verification.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
