"""Check the actual UE stereo output, not only the prepared source files."""
from array import array
from pathlib import Path
import json
import math
import sys
import wave


def verify(path):
    with wave.open(str(path), 'rb') as w:
        assert w.getnchannels() == 2 and w.getsampwidth() == 2, 'Expected 16-bit stereo UE capture'
        rate, frames = w.getframerate(), w.getnframes()
        pcm = array('h', w.readframes(frames))
    if sys.byteorder != 'little':
        pcm.byteswap()
    assert frames / rate >= 28, 'Incomplete 30-second capture'
    left_all, right_all = pcm[0::2], pcm[1::2]
    ratio = 10 ** (-6/20)
    # UE exports a float mix to 16-bit integers. At quiet ambient levels even
    # one PCM unit biases the RMS ratio visibly. Check every sample against
    # the expected gain, allowing source + export quantization (<2.5 units).
    residual = max(abs(l*ratio-r) for l, r in zip(left_all, right_all))
    assert residual <= 2.5, f'Channel gain/phase mismatch: {residual:.3f} PCM units'
    run = longest_silence = 0
    for l, r in zip(left_all, right_all):
        run = run + 1 if l == 0 and r == 0 else 0
        longest_silence = max(longest_silence, run)
    assert longest_silence/rate < .02, 'Digital silence/dropout longer than 20 ms'
    windows = []
    for offset in range(0, frames, rate):
        block = pcm[2*offset:2*min(offset+rate, frames)]
        left = sum(x*x for x in block[0::2])
        right = sum(x*x for x in block[1::2])
        if right < 1:
            continue
        db = 10 * math.log10(left/right)
        rms = math.sqrt(left / (len(block)//2)) / 32768
        if rms > .0001:
            windows.append(dict(second=offset/rate, left_minus_right_db=db, left_rms=rms))
    assert len(windows) >= 28, 'Output is silent or drops out'
    peak = max(abs(x) for x in pcm) / 32768
    assert 0 < peak < .76, f'Unexpected output peak {peak}'
    balance = 10*math.log10(sum(x*x for x in left_all)/sum(x*x for x in right_all))
    assert abs(balance-6) < .05, f'Whole-recording ear balance: {balance:.4f} dB'
    result = dict(seconds=frames/rate, rate=rate, peak=peak, left_minus_right_db=balance,
                  max_channel_residual_pcm_units=residual,
                  longest_digital_silence_seconds=longest_silence/rate, windows=windows)
    path.with_suffix('.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(f'PASS UE output: {frames/rate:.2f}s, {len(windows)} audible windows (up to 1s each), '
          f'left +{balance:.4f} dB, max channel residual {residual:.3f} PCM units, '
          f'peak {20*math.log10(peak):.2f} dBFS, no clipped samples')


if __name__ == '__main__':
    verify(Path(sys.argv[1]))
