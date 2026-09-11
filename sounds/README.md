# Sound masters

The rider supplied these five final WAV masters on 2026-09-11 and stated that
they were made with Adobe Firefly. They are the selected
sound sources for the application, not placeholders.

- `catch.wav`: oar entering the water.
- `pull.wav`: oar pulling through the water.
- `boat.wav`: hull cutting through waves.
- `silent-water.wav`: quiet lake surface.
- `silent-wind.wav`: quiet wind (the actual extension is `.wav`).

Keep the local originals unchanged. `tools/prepare_audio.ps1` prepares derived
PCM files under ignored `artifacts/audio/` and imports SoundWave assets under
ignored `Content/Row/Audio/`. Raw WAVs, editing sidecars and the separate
`Firefly_audio/` source collection stay local and outside Git. The code's MIT
license does not assert a separate redistribution license for these recordings.

See [Japanese sound guide](../docs/AUDIO.ja.md) for playback and volume.
