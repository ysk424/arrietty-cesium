"""Prepare local Firefly masters. Standard library only; originals stay untouched."""
from array import array
from pathlib import Path
import hashlib
import json
import math
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
SOUNDS = (
    ('catch.wav', 'S_Catch', False, 0),
    ('pull.wav', 'S_Pull', True, .15),
    ('boat.wav', 'S_Boat', True, .20),
    ('silent-water.wav', 'S_Water', True, .60),
    ('silent-wind.wav', 'S_Wind', True, .75),
)
LEFT_DB = 6.0
PEAK = .5


def prepare(source, destination, loop, overlap):
    with wave.open(str(source), 'rb') as w:
        if w.getsampwidth() != 2 or w.getnchannels() not in (1, 2) or w.getcomptype() != 'NONE':
            raise ValueError(f'{source.name}: expected uncompressed 16-bit mono/stereo WAV')
        rate, channels = w.getframerate(), w.getnchannels()
        pcm = array('h', w.readframes(w.getnframes()))
    if sys.byteorder != 'little':
        pcm.byteswap()
    # Same content in both ears: original stereo imbalance cannot defeat the
    # requested 6 dB. Head turns must not rotate this accessibility balance.
    mono = [sum(pcm[i:i+channels]) / (32768 * channels) for i in range(0, len(pcm), channels)]
    if len(mono) < rate // 4:
        raise ValueError(f'{source.name}: too short')
    dc = sum(mono) / len(mono)
    mono = [x - dc for x in mono]
    if loop:
        n = min(round(overlap * rate), len(mono) // 4)
        # Tail -> head crossfade, then the untouched middle. The wrap is now a
        # naturally adjacent sample pair; no silent fade at every repetition.
        join = []
        for i in range(n):
            t = .5 - .5 * math.cos(math.pi * i / (n - 1))
            join.append(mono[-n+i] * (1-t) + mono[i] * t)
        mono = join + mono[n:-n]
    else:
        attack, release = round(.005 * rate), round(.12 * rate)
        for i in range(attack):
            mono[i] *= .5 - .5 * math.cos(math.pi * i / (attack - 1))
        for i in range(release):
            mono[-release+i] *= .5 + .5 * math.cos(math.pi * i / (release - 1))
    peak = max(abs(x) for x in mono)
    if peak < 1e-6:
        raise ValueError(f'{source.name}: no audible samples')
    scale = PEAK / peak
    right_gain = 10 ** (-LEFT_DB / 20)
    out = array('h')
    for x in mono:
        left = round(x * scale * 32767)
        out.extend((left, round(left * right_gain)))
    left_energy = sum(x*x for x in out[0::2])
    right_energy = sum(x*x for x in out[1::2])
    balance = 10 * math.log10(left_energy / right_energy)
    assert abs(balance - LEFT_DB) < .01
    assert max(abs(x) for x in out) <= 16384
    if sys.byteorder != 'little':
        out.byteswap()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(destination), 'wb') as w:
        w.setparams((2, 2, rate, 0, 'NONE', 'not compressed'))
        w.writeframes(out.tobytes())
    return dict(source=source.name, sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                rate=rate, seconds=len(mono)/rate, loop=loop, left_minus_right_db=balance,
                peak=PEAK, crossfade_seconds=overlap)


def prepare_all():
    missing = [name for name, *_ in SOUNDS if not (ROOT/'sounds'/name).is_file()]
    if missing:
        raise FileNotFoundError('Place the five local masters in sounds/: ' + ', '.join(missing))
    output = ROOT/'artifacts/audio'
    report = []
    for name, asset, loop, overlap in SOUNDS:
        report.append(prepare(ROOT/'sounds'/name, output/(asset+'.wav'), loop, overlap))
    (output/'preparation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return output, report


if __name__ == '__main__':
    _, report = prepare_all()
    print(json.dumps(report, indent=2))
