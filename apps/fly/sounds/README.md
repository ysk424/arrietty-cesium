# Human-powered glider sound masters

Six user-supplied PCM16 / 48 kHz stereo masters were received in root `wav_fly/`
on 2026-09-12: wind-soft, wind-fast, pedal-drive, propeller, ground-roll (15 s each)
and touchdown (1 s). The importer accepts the delivered double extensions and
space in `wind-soft.wav .wav`; originals are never renamed or overwritten.

For a new installation, place these canonical names here: `wind-soft.wav`,
`wind-fast.wav`, `pedal-drive.wav`, `propeller.wav`, `ground-roll.wav`,
`touchdown.wav`. This directory takes priority over root `wav_fly/`.

Run `./apps/fly/tools/prepare_audio.ps1` to regenerate `/Game/Fly/Audio`.
The complete fly preparation also imports available masters. Without local
masters a new build reports unavailable audio and remains usable without sound.
A partially supplied set is an error. See the [Japanese guide](../../../docs/FLY_AUDIO.ja.md).

Originals, editing sidecars, preparation hashes and generated WAV/UE assets are
ignored by Git. Derived files live in `apps/fly/artifacts/audio/` and the app's
UE `Content/`. User-supplied audio is not covered by the code's MIT license;
no redistribution permission is assumed and no sound files are published.
