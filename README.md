# Beat Mangler

*Garbage in, music out.*

A generative re-composer for the Daisy Patch (IDM territory: Aphex / Autechre / Plaid). Not a manual sampler.

Breaks — or any beat, or any sample, including a bar of vocals — load from SD. The module auto-slices by transient detection, then probabilistically re-sequences slices with per-slice pitch, reverse, stutter, and retrigger. One knob sweeps order into chaos: at zero it plays the source faithfully; turned up, total beautiful chaos.

## Key constraints

- **Generative-first, not a manual sampler.** Must clearly earn its place next to the 1010music BitBox: auto-slicing vs manual slice editing, the chaos macro as a single performable gesture, per-slice degradation in the playback path (PBD lineage), CV/gate native, no menu diving.
- **Immediately playable.** Boots, loads the first break on the card, auto-slices, starts making music. Clock in auto-fits the source to tempo. Smart defaults plus a few curated scenes.
- **VST-first iteration.** The DSP is built from day one as a portable, platform-independent core shared between the hardware firmware and a VST. Iterate on the design in Ableton; the Daisy Patch firmware is developed in tandem immediately after.

## Meat Bangler

Hold the encoder on boot to unlock Meat Bangler mode — the feral twin hiding inside. Where Beat Mangler stays usable in an IDM sense, Meat Bangler is pure chaos: experimental noise that can be shaped and softened into delicate pitched drones.

## Status

Design phase. Brainstorming and design only — no code until given the word.

See `DESIGN.md` for the full working brief.
