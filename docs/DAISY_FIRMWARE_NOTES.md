# Daisy Patch firmware notes

Future adaptation of the Break Mangler VST to the Daisy Patch hardware.
Written 2026-10-16 from the VST ear-testing sessions. Not started; revisit
once the VST is finalized.

## Feasibility

Yes. 90s stereo float is ~33MB < 64MB SDRAM. 8 voices are trivial for the
480MHz M7. The DSP core is portable C++17 with no JUCE inside, no audio-path
allocations, and a free-running `process()` already exists for firmware use.

## Clock plan (decided 2026-10-16)

Priority order, auto-detecting with fallback:

1. **MIDI clock in** (24 PPQN) — preferred when present; also brings
   start/stop/continue transport.
2. **Gate 1 = modular clock in** — sample-tight with the rack, no MIDI
   jitter. Needs a clock-division setting (16ths/8ths/quarters in).
3. **Tap tempo** — fallback when no external clock is present.

**Gate 2 = reset.** Re-arms the loop to slice 0, quantized to the next
downbeat (the engine's launch logic already supports this).

## Control mapping

- 4 knobs → CHAOS / DENSITY / PITCH / DEGRADE (1:1).
- 4 CV inputs → modulation targets for the same four (the hardware answer
  to the LFO-modulation workflow: random-waveform LFOs into these are where
  the instrument comes alive).
- GRID / SNAP / RATE → encoder menu or button combos (TBD).
- Waveform → downsampled to the 128x64 OLED from the 512-column snapshot.
- File loading → SD card (WAV parser already portable; needs FatFS wiring).
