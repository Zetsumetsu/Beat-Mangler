# Beat Mangler — DSP core architecture

Draft 2026-10-10. Design only, no code yet. The core is plain portable C++; thin wrappers adapt it to the Daisy Patch (libDaisy) and to a JUCE VST3. No platform code inside the core.

## Signal flow

```
WAV file → Loader → Slicer → slice table
                                ↓
Clock → Generative sequencer → Voice (per-slice DSP) → Mixer → out
                ↑                        ↑
           Chaos macro              per-slice params
```

## 1. Loader

Reads the selected WAV (SD card on hardware, disk in the VST), validates the header (16/24/32-bit PCM, mono/stereo), resamples odd rates at load, caps length (~90s stereo), auto-normalizes to a target peak. Output: one float buffer + metadata. Runs once per file load, never in the audio thread.

## 2. Slicer

- **Transient mode (default):** onset detection over the buffer with an adaptive threshold, run once at load. Stores up to 16 slice markers.
- **Grid mode:** equal divisions, 16 default, tempo-aware via clock-in auto-fit. Used for sustained material with no clear attacks.
- **Auto-fallback:** fewer than a handful of real onsets → grid, with an OLED note.
- **One-shot:** file under ~2s with ~1 onset → the whole file is a single slice.
- Each slice stores start/end, peak, and RMS. Peak/RMS feed the sequencer's probability weights.

## 3. Generative sequencer

The heart. Steps advance on the clock (internal or clock-in, auto-fit to the source tempo).

Per step, with the chaos macro at 0 the source plays back faithfully in order. As chaos rises:
- **Reorder probability** rises: next slice picked by weighted random (weights from slice peak/RMS), from "mostly in order" to "any slice".
- **Pitch range** widens: per-slice pitch from subtle detune up to ±2 octaves.
- **Reverse probability** rises per slice.
- **Stutter/retrigger** probability and depth rise: slices repeat at subdivisions.
- **Degradation** rises: per-slice bitcrush/decimation (PBD lineage) creeps into the playback path.

All probabilities derive from the single chaos value plus per-parameter trim, so one knob (or one automation lane) performs the whole journey.

## 4. Voice (per-slice DSP)

Each triggered slice gets: pitch shift by resampling, optional reverse read, stutter/retrigger subdivision, bitcrush + decimation, and short fades at slice boundaries so nothing clicks. Voices are lightweight; polyphony is capped to keep the M7 comfortable.

## 5. Meat Bangler mode

Same engine, past the edge. Chaos behavior goes fully feral: extreme reorder/pitch/reverse/stutter probabilities and heavier degradation. Additionally, long crossfaded slice loops with pitch drift let the noise be shaped and softened into delicate pitched drones — so the mode spans from total destruction to fragile tone. (Exact control mapping TBD.)

## 6. Mixer / output

Voices sum to a stereo bus. A master degrade stage is optional (provisional). Keep the output path simple; the character lives in the voices.

## Parameter model (provisional)

Hardware has 4 knobs + 4 CVs + gates + encoder; the VST exposes the same parameters as automatable controls:

| Param | What it does |
|---|---|
| CHAOS | The macro: order → full Autechre |
| DENSITY | Stutter/retrigger rate and depth trim |
| PITCH | Pitch deviation range trim |
| DEGRADE | Bitcrush/decimation amount trim |
| (CV/gates) | Clock in, reset, chaos CV, slice trigger / freeze |

Encoder + OLED handle file selection, slice mode display, and scenes. No menu diving in performance.

## Future repo layout

```
beat-mangler/
  dsp/        # portable core (this document)
  hardware/   # Daisy Patch firmware wrapper (libDaisy)
  vst/        # JUCE VST3 wrapper
  docs/       # design docs
```
