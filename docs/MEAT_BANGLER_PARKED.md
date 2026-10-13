# Meat Bangler (PARKED)

Status: separate future instrument. Shelved 2026-10-12 so Break Mangler can
stay hyperfocused on break re-composition. Nothing here is built; these are
the design notes carried over from the Break Mangler repo, preserved for
when this gets its own project.

## Concept

Meat Bangler is the feral twin of Break Mangler: pure chaos, experimental
noise that can be shaped and softened into delicate pitched drones. Where
Break Mangler stays usable in an IDM sense, Meat Bangler goes past the edge.

Original easter-egg idea (from the shared-engine days): hold the encoder on
boot to unlock Meat Bangler mode. If it becomes its own instrument, this
becomes its own boot behavior instead.

## Leading design (under discussion, not locked)

Granular slicing and blending of slices into a smearable texture:

- Slices dissolve into overlapping grains with crossfades.
- A smear dimension morphs grain size, density, and overlap from gritty
  texture into melted pitched wash.
- Long crossfaded grain loops with pitch drift shape the noise into
  delicate pitched drones.
- Chaos behavior stays feral underneath: extreme reorder/pitch/reverse/
  stutter probabilities and heavier degradation.

## Character split (from the original brief)

- Break Mangler: rhythmic use first.
- Meat Bangler: drone/ambient texture use first (can be rhythmic at times).

## Open questions for when this unparks

- Own DSP core or a fork of the Break Mangler core?
- Hardware target: Daisy Patch like its sibling, or something else?
- Does the boot-hold easter egg survive as its own thing, or get dropped?
