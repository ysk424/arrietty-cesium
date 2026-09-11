"""Plot/test the C++ solver's synthetic fields; no hardware or exercise data.

Run tools/build_native.ps1 first. Requires numpy and matplotlib for offline QA.
"""
from pathlib import Path
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/water'
fig, axes = plt.subplots(1, 3, figsize=(15, 6), constrained_layout=True)
measurements = []
for ax, name in zip(axes, ('straight-2mps', 'straight-3mps', 'turn')):
    meta = json.loads((OUT / (name + '.json')).read_text())
    h = np.fromfile(OUT / (name + '.f32'), dtype='<f4').reshape(256, 256)
    x, y = [meta['origin'][i] + np.arange(256) * .25 - meta['boat'][i] for i in (0, 1)]
    image = ax.pcolormesh(x, y, h * 100, cmap='RdBu_r', vmin=-2, vmax=2, shading='nearest')
    ax.set(xlim=(-28, 8), ylim=(-18, 18), aspect='equal', xlabel='World X from boat (m)', ylabel='World Y from boat (m)')
    heading = meta['heading']
    ax.plot([-1.8*np.cos(heading), 2*np.cos(heading)], [-1.8*np.sin(heading), 2*np.sin(heading)], 'k', lw=3)
    ax.set_title(name.replace('-', ' '))
    if name != 'turn':
        speed = float(name[9])
        # The analytic lines are an OVERLAY for comparison, never solver inputs.
        aft = np.linspace(0, 32, 100)
        for sign in (-1, 1):
            ax.plot(2 - aft, sign*aft/np.sqrt(8), color='#b08b24', ls='--', lw=1)
        X, Y = np.meshgrid(x-2, y)  # Kelvin wedge measured from bow pressure source.
        angle = np.degrees(np.arctan2(abs(Y), -X))
        far = (X < -9) & (X > -23)
        inside = far & (angle < 21)
        outside = far & (angle > 26) & (angle < 55)
        ratio = np.mean(h[outside]**2) / np.mean(h[inside]**2)
        line = h[np.argmin(abs(y))]
        peaks = np.flatnonzero((line[1:-1] > line[:-2]) & (line[1:-1] >= line[2:])) + 1
        peaks = peaks[(x[peaks] > -24) & (x[peaks] < -5)]
        measured = float(np.median(np.diff(x[peaks])))
        expected = 2*np.pi*speed**2/9.81
        assert abs(measured/expected-1) < .12, (name, measured, expected)
        assert ratio < .10, (name, ratio)
        measurements.append(dict(speed_mps=speed, measured_transverse_wavelength_m=measured,
                                 expected_transverse_wavelength_m=expected, outside_to_inside_mean_square= float(ratio)))
        ax.text(.03, .03, f'Transverse wavelength {measured:.2f} m\nTheory {expected:.2f} m', transform=ax.transAxes,
                fontsize=9, bbox=dict(facecolor='white', alpha=.9, edgecolor='none'))
fig.colorbar(image, ax=axes, label='Surface displacement (cm)', shrink=.7)
fig.suptitle('Kelvin wake — C++ deep-water pressure solver\nDashed lines: theoretical ±19.47° envelope; black: hull', fontsize=14)
fig.savefig(OUT/'kelvin-validation.png', dpi=160)
(OUT/'measurements.json').write_text(json.dumps(measurements, indent=2)+'\n')
print(json.dumps(measurements, indent=2))
print('PASS: measured wavelengths and energy confinement at two speeds')
