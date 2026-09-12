"""Verify the actual UE mixer export from FlyAudioFixture (not a microphone)."""
from array import array
import json
import math
from pathlib import Path
import sys
import wave


def verify(path):
    with wave.open(str(path),'rb') as w:
        rate, channels, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        assert width==2 and channels==2, 'Expected stereo PCM16 engine output'
        data=array('h',w.readframes(w.getnframes()))
        if sys.byteorder!='little': data.byteswap()
    duration=len(data)/rate/channels
    assert 29<=duration<=32, f'Unexpected capture duration: {duration}'
    def rms(start,end):
        part=data[int(start*rate)*channels:int(end*rate)*channels]
        return math.sqrt(sum(float(x)*x for x in part)/len(part))/32768
    levels={name:rms(start,end) for name,start,end in [
        ('setup',.3,1.5),('ground_pedal',3,6),('powered_flight',8,11),
        ('unpowered_glide',14,17),('fast_glide',19,22),('landing_roll',23.2,25.5),('stopped',28,29)]}
    peak=max(abs(x) for x in data)/32768
    assert peak<.95, f'Mix exceeds headroom: {peak}'
    assert levels['setup']<.0001 and levels['stopped']<.0001, levels
    for name in ('ground_pedal','powered_flight','unpowered_glide','fast_glide','landing_roll'):
        assert levels[name]>.001, f'Silent segment: {name}'
    assert levels['fast_glide']>levels['unpowered_glide']*1.1, 'Fast wind should be more audible'
    report=json.dumps(dict(seconds=duration,peak=peak,rms=levels),indent=2)
    path.with_suffix('.verification.json').write_text(report,encoding='utf-8')
    print(report)


if __name__=='__main__': verify(Path(sys.argv[1]))
