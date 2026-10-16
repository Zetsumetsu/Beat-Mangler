# Break Mangler — design brief

Working design document. Status: brainstorming — no build commitment yet.

## Concept

A generative re-composer, NOT a manual sampler. Loops load from disk; the engine auto-slices by transient detection, then probabilistically re-sequences slices with per-slice pitch, reverse, stutter, and retrigger. One-knob order-to-chaos macro: at zero it plays the source faithfully, turned up, total beautiful chaos.

## Key design constraints

1. Must clearly differentiate from the 1010music BitBox: generative-first re-composer vs touchscreen-operated sampler; auto-slicing vs manual slice editing; the chaos macro as a single performable gesture; his DSP DNA (per-slice degradation/decimation, PBD lineage) in the playback path; CV/gate native, no menu diving.
2. Immediately playable, near-zero futzing: on boot it loads the first break on the card, auto-slices, and starts playing something musical. Clock in auto-fits the source to tempo. Smart defaults plus a few curated scenes.
3. Dual slice modes: transient detection for drums/vocals, grid (equal divisions) for sustained material that has no clear attacks — with auto-fallback.

## Slice modes

- **Transient mode (default):** onset detection runs once at load with an adaptive threshold; slice markers stored. Each slice gets peak/RMS measured at load so the sequencer can weight probabilities (e.g. favor louder slices for accents).
- **Grid mode:** equal divisions, 16 slices default, tempo-aware via clock-in auto-fit. For sustained material with no clear attacks.
- **Auto-fallback:** if transient detection finds fewer than a handful of real onsets, switch to grid automatically and say so in the UI.
- **Slice cap (proposed):** 16 slices max, one-to-one with the 16-LED grid.

## Sample management

- Multiple samples supported: the loader reads WAVs from disk. Selecting one loads it with a brief "LOADING" pause, then auto-slices and resumes.
- Long samples: load capped at ~90s stereo; truncate with a UI note.
- One-shots: special-cased. If the file is short (~under 2s) with ~1 onset, the whole file becomes a single slice and the re-sequencer applies pitch/reverse/stutter/retrigger to it — a one-shot becomes a playable voice.
- Defensive loading: auto-normalize to a target peak on load; validate WAV (16/24/32-bit PCM mono/stereo); resample odd rates at load or reject with a message. Never a crash.

## Architecture

VST-first prototype in Ableton, with a Daisy Patch firmware adaptation to follow once the VST is finalized. The DSP is built from day one as a portable, platform-independent core (one source of truth shared by firmware and VST), same pattern as the FM-GRIME monorepo. See `docs/DSP_CORE.md` for the core architecture.

## Feasibility

Yes, the Daisy Patch can do all of this — SD slot, 64MB SDRAM for buffers, 480MHz M7 for detection/resequencing/DSP, 4 knobs + 4 CVs + gates + encoder/OLED + 16-LED grid for the interface.

## Decisions locked (2026-10-10, revised 2026-10-12)

- Break Mangler is a breaks-dedicated instrument: hyperfocused on being the
  best generative break re-composer, not a general sample mangler.
- Meat Bangler is a separate future instrument, shelved for now.
  See `docs/MEAT_BANGLER_PARKED.md` for its design notes.
- Architecture approved: one-knob chaos macro drives all probabilities + the per-slice voice.
- Pitch: musical first. Quantized default, with a switch for semitone snap, scale snap, or continuous glide. (Final pitch behavior is tentative — to be worked through together.)
- VST prototype: minimal first (load + auto-slice + chaos macro + play); fuller parameter set later.
- Knob mapping: CHAOS central + DENSITY/PITCH/DEGRADE trims, approved for now.
- Ring modulation + spectral noise: parked for the fuller-parameter round as seasoning in the voice chain; must earn its place by making breaks better.
- Efficiency mandate: leaner is better, no crashing systems. Prefer efficient approximations that achieve similar results; extras only if the computing budget allows.
