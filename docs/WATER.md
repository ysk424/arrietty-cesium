# Kelvin wake and bow whitewater

日本語の説明・起動手順: [WATER.ja.md](WATER.ja.md)

Kelvin wakes and bow whitewater are inherited from Arrietty-row. Normal launches
and further development use the original `Arrietty-row` checkout. The temporary
feature worktree is retired; its local evidence is preserved in the main
checkout under `logs/kelvin-worktree/` and `artifacts/kelvin-worktree/`.

## Model

`Source/RowKelvin.h` solves linear **deep-water gravity waves**, in metres and
seconds, using surface displacement `eta` and surface velocity potential `phi`:

```
eta_t = |D| phi
phi_t = -g eta - p/rho
omega(k)^2 = g |k|, g = 9.81 m/s^2
```

Two moving Gaussian pressures represent the bow (2 m forward, radius 0.42 m)
and stern (1.8 m aft, radius 0.50 m). Their pressure heads are 4.5 cm and 3.2 cm
at 3 m/s; strength scales with speed squared and saturates above that speed.
These are visual hull parameters, not measured forces or resistance. They do
not feed back into propulsion, steering, calibration or session measurements.
Pressure follows speed through recovery/coasting; stationary, paused and
calibrating boats emit no new hull wake. Existing waves disperse and decay.

The transverse and divergent waves arise from the moving pressures and
dispersion. The solver does not draw a prescribed V or rotate a wake texture.
For steady motion in deep water the theoretical envelope half-angle is
`asin(1/3) = 19.47 degrees`; transverse wavelength is `2*pi*U^2/g`.
The angle of the most visible peaks can differ with speed and hull pressure.
References: [Richard Fitzpatrick, Ship Wakes](https://farside.ph.utexas.edu/teaching/315/Waves/node86.html)
and [Darmon, Benzaquen & Raphael, Kelvin wake pattern at large Froude numbers](https://arxiv.org/abs/1309.6751).

Implementation: a 256 x 256, 64 m field at 30 Hz, exact free-wave propagation
in Fourier space with symmetric pressure half-steps. Height and potential are
packed into two real channels of an original complex radix-2 FFT; there is no
added dependency or third-party implementation. Frequencies, twiddle factors,
and damping are precomputed. An 8 m boundary sponge suppresses periodic wrap;
the uniform height mode is removed. Discrete domain translations preserve the
samples' world positions. A displacement outside the entire domain clears both
height and potential. Turns therefore leave a curved, dispersing history.

`RowWater.cpp` combines these heights and slopes with the existing short oar
ripples and foam in the same RGBA16F texture. Both vertex displacement and
lighting normals read the resulting field. The old constant-speed bow/stern
height impulses are removed; their bubbles remain. Both oars now disturb their
respective sides. Texture origin changes accompany uploads to avoid sliding
between texture updates. Existing wind ripples, the 22–28 m surface fade, mean
lake reference **Z=0 cm**, scenery, and level HMD root remain in place.

`Source/RowBowWhitewater.h` adds speed-driven bow whitewater. Its two shoulders
join at 2.4 m ahead of the boat origin, outside the 2.3 m hull tip. Emission is
zero below 0.4 m/s, grows smoothly with boat speed, and is bounded at high speed.
The profile length/width grow with speed; a saturating rate deposits foam into
its own world-space foam channel at fixed steps, combined with existing foam
on upload. It continues while coasting and decays with an approximately 0.32 s
half-life after emission, faster than the unchanged oar bubble layer. A local
animated crest (at most 2.2 cm) is included in the
uploaded height AND its normal derivatives, separate from the Kelvin solver.
This is an artistic breaking/entrained-air layer, not particle or breaking-fluid
simulation. Oar ripples and bubbles remain. No material regeneration is needed.

Bow verification captures can use distinct names without replacing earlier
Kelvin evidence: `./tools/preview.ps1 -Chase -Straight -Name row-bow-chase` and
`./tools/preview.ps1 -Straight -Name row-bow-view`. Run them one at a time.

## Reproduce the checks

Run these from the normal `Arrietty-row` checkout:

```powershell
./tools/build_native.ps1
py -3.13 tools/plot_water.py
./tools/preview.ps1 -Water -Straight
# Wait for the first offline game to finish before starting another preview.
./tools/preview.ps1 -Water
./tools/preview.ps1 -Chase
./tools/preview.ps1
./tools/test_setup.ps1
```

`plot_water.py` needs NumPy and Matplotlib, only for offline verification. Native
tests export synthetic fields under ignored `artifacts/water/`. The plot and
`measurements.json` measure the actual C++ results; dashed theoretical lines in
the plot are annotations, not inputs to the solver. Speed, position and steering
in these fixtures are synthetic, with no BLE or real Tracker connection.

`-Water` uses an elevated offline camera looking aft and captures at 52 seconds, after the
normal demo calibration and enough travel to build a wake. `-Straight` removes
only the demo's synthetic lean. Neither option changes live HMD controls.
Build the Editor target before previewing; copied v1 generated materials already
read the combined wake texture, so this change needs no scenery/material rebuild.

## Limits

This is a linear local deep-water model, not CFD or a calibrated hull simulation.
It omits finite depth, bank reflection/refraction, nonlinear breaking and surface
tension. The 25 cm grid suppresses very short waves at low speed; the finite
visible radius limits long wavelengths and old wakes at higher speed. Foam is
still an artistic oar/hull bubble layer rather than a computed breaking model.
Fixed-step pressure sampling and the boundary sponge introduce approximation.
Numerical field checks and desktop renders do not establish quantitative stereo
frame time or long-session comfort. Current-fork evidence and limits are recorded in [validation](VALIDATION.md).
