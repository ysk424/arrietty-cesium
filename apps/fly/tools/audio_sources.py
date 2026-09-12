"""Prepare local, immutable user masters for UE; no extra Python packages."""
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
# Canonical file, delivered alias, UE asset, loop, output channels.
SOUNDS = (
    ('wind-soft.wav', 'wind-soft.wav .wav', 'S_WindSoft', True, 2),
    ('wind-fast.wav', 'wind-fast.wav.wav', 'S_WindFast', True, 2),
    ('pedal-drive.wav', 'pedal-drive.wav.wav', 'S_PedalDrive', True, 1),
    ('propeller.wav', 'propeller.wav', 'S_Propeller', True, 1),
    ('ground-roll.wav', 'ground-roll.wav.wav', 'S_GroundRoll', True, 1),
    ('touchdown.wav', 'touchdown.wav', 'S_Touchdown', False, 1),
)


def prepare(source, output, loop, channels):
    original = source.read_bytes()
    with wave.open(str(source), 'rb') as w:
        if w.getsampwidth() != 2 or w.getnchannels() not in (1, 2) or w.getcomptype() != 'NONE':
            raise ValueError(f'{source.name}: PCM16 mono/stereo WAV required')
        rate, count = w.getframerate(), w.getnchannels()
        data = array('h', w.readframes(w.getnframes()))
        if sys.byteorder != 'little': data.byteswap()
    frames = len(data)//count
    if frames < rate//2: raise ValueError(f'{source.name}: sound is too short')
    tracks = []
    for ch in range(channels):
        track = ([sum(data[i:i+count])/count for i in range(0, len(data), count)]
                 if channels == 1 else list(data[min(ch,count-1)::count]))
        dc = sum(track)/frames
        tracks.append([v-dc for v in track])
    if loop:
        n = min(int(rate*.2), frames//4)
        for ch, track in enumerate(tracks):
            # Crossfade end into start, then retain the middle. Both joins are
            # adjacent original samples, including the wrap back to the end.
            join = []
            for i in range(n):
                t = .5-.5*math.cos(math.pi*i/(n-1))
                join.append(track[-n+i]*(1-t)+track[i]*t)
            tracks[ch] = join+track[n:-n]
    else:
        for track in tracks:
            attack, release = int(rate*.005), int(rate*.10)
            for i in range(attack): track[i] *= i/attack
            for i in range(release): track[-i-1] *= i/release
    # Shared stereo gain preserves the user's wind image; no ear correction.
    # Peak ceiling leaves headroom even with all six bounded mix voices.
    peak = max(abs(v) for track in tracks for v in track)
    if peak < 1: raise ValueError(f'{source.name}: silent master')
    gain = .5*32767/peak
    pcm = array('h', (round(tracks[ch][i]*gain) for i in range(len(tracks[0])) for ch in range(channels)))
    if sys.byteorder != 'little': pcm.byteswap()
    output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output), 'wb') as w:
        w.setnchannels(channels); w.setsampwidth(2); w.setframerate(rate); w.writeframes(pcm.tobytes())
    return dict(source=str(source.relative_to(ROOT)), sha256=hashlib.sha256(original).hexdigest(),
                output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                rate=rate, channels=channels, seconds=len(tracks[0])/rate,
                loop=loop, peak_ceiling=.5, gain=gain)


def prepare_all(required=True):
    found = []
    for canonical, alias, asset, loop, channels in SOUNDS:
        candidates = [APP/'sounds'/canonical, ROOT/'wav_fly'/canonical, ROOT/'wav_fly'/alias]
        source = next((p for p in candidates if p.is_file()), None)
        if source is None:
            if not required and not any(p.is_file() for p in (ROOT/'wav_fly').glob('*.wav')) and not any((APP/'sounds').glob('*.wav')):
                return None
            raise FileNotFoundError(f'Missing fly sound: {canonical}; see docs/FLY_AUDIO.ja.md')
        found.append((source, asset, loop, channels))
    output = APP/'artifacts/audio'
    report = {asset: prepare(source, output/(asset+'.wav'), loop, channels)
              for source, asset, loop, channels in found}
    (output/'preparation.local.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return output


if __name__ == '__main__':
    print('FLY_AUDIO_PREPARED', prepare_all())
