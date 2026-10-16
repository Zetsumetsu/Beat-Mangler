# Break Mangler — User Manual

Break Mangler is a generative re-composer. Drop in a loop and it slices the
audio apart and re-sequences it live, with one knob sweeping from faithful
playback to total beautiful chaos. It is not a manual sampler: you do not
place the slices, you conduct the probabilities and let the machine play.

## Loading a sample

Two ways, your choice:

- **LOAD button** — the primary workflow. Opens a file browser. Pick a
  WAV, AIFF, FLAC, or MP3.
- **Drop zone** — drag a file from your file manager straight onto the
  waveform display. Handy when you are auditioning lots of samples quickly.

On load the engine measures your loop against the host tempo and fits it to
a whole bar count from 1 to 32 (odd counts welcome). Anything that lands
within a small tolerance of a whole bar count gets grid-sliced; anything
else falls back to transient slicing. The readout tells you what happened,
for example "4 BARS - 64 SLICES".

The engine only makes sound while the host transport is running. If you
hear nothing, check the readout: **HOST STOPPED** means press play in your
DAW; **HOST PLAYING** (red) means the engine is live.

## The controls

**CHAOS** — the macro. Controls how often slices get reordered. At zero the
loop plays in order, faithful. Nudged up, the groove stays but surprises
start landing. Turned up, everything is fair game.

**DENSITY** — per-slice chance of rhythmic mangling: a stutter (2 to 4
repeats) or a reversed slice, coin flip between the two.

**PITCH** — per-slice chance of an interval jump. Jumps favor musical
intervals (fifths and octaves); wilder ones (fourths, sevenths, double
octaves) land less often, as surprises.

**DEGRADE** — per-slice chance of a bitcrush hit: a short burst of 8-bit
grit.

**ERA** — the global character macro. At zero the output is clean and
modern, exactly as the engine renders it. Turn it up and three things move
together: bit depth falls toward 12-bit, the input drive saturates harder,
and the top end darkens. Full up is the S950: crunchy, hot, and dark. It
sits on the output stage, after everything else, so it colors the whole
performance at once. It pairs beautifully with DEGRADE: crushed slices
through a saturated output get truly filthy.

**RATE** — step density, relative to the slice grid. 1 is one step per
slice: the loop as sliced. 2 is twice as dense, 1/4 is quarter density.

**GRID (16TH / BEAT)** — the slice layout. 16TH chops each bar into 16
slices; BEAT chops it into 4 (quarter notes). Changing it re-slices the
loaded loop.

**SNAP (GLIDE / SEMI / SCALE)** — how pitch jumps get quantized. GLIDE
leaves them raw and smooth, SEMI snaps them to the nearest semitone, SCALE
pulls them into the minor pentatonic so pitched slices stay in key. The
wilder PITCH gets, the more SCALE earns its keep.

**RESET** — the circular-arrow icon in the header zeroes all five knobs,
back to the clean sample. Your DAW project already remembers the knob
positions, so there is no preset system to manage.

All five knobs are built to be modulated. They are automatable and respond
to LFOs like any plugin parameter.

## The waveform display

The fitted loop drawn on its grid, with slice boundaries and brighter bar
lines. The red line is the playhead following the grid position; the slice
currently sounding glows red. It is also the drop zone for loading samples.

## Sample selection

Break Mangler was built for break-style beats, but the fitting is generous
and curiosity is rewarded:

- **Breaks and drum loops** — the home turf. Dense transients slice
  cleanly and re-sequence musically.
- **One-shots** — a short file with a single onset becomes one slice, and
  the re-sequencer turns it into a playable voice: pitch it, reverse it,
  stutter it.
- **Vocals and melodies** — sustained material slices into longer pieces.
  Expect choppier, more abstract results; sometimes strikingly good.
- **Regular beats** — straight loops can produce surprising patterns once
  the slices start reconfiguring.

Two things to know: very long files are capped at 90 seconds, and the
engine normalizes everything on load, so quiet samples and loud samples
start on equal footing.

## Playing it: three starting points

**The faithful loop.** All knobs at zero, RATE at 1. The loop plays as
sliced. This is home base; everything else is a departure from here.

**The pitched break.** CHAOS at zero, PITCH up a little. The break plays
straight but slices keep jumping octaves and fifths. Classic jungle energy
with no reordering at all.

**The modulation performance.** This is where the instrument comes alive.
Put a random-waveform LFO on CHAOS, another (slower) on DENSITY, dial the
amounts until the loop breathes between order and chaos on its own. Ride
PITCH manually for accents. Record the output and cut the best bars.

## Troubleshooting

- **No sound:** the transport is probably stopped. Look for HOST PLAYING.
- **A loop sounds sped up or chopped:** RATE and GRID need to agree. A
  loop sliced at BEAT (quarter notes) wants RATE around 1/4 to play 1:1;
  a 16TH-sliced loop wants RATE at 1.
- **Clicks on some samples:** short slices can click when they loop inside
  a step. The engine fades these automatically; if you hear them, try a
  denser GRID setting.
- **A sample will not load:** it must be a readable WAV, AIFF, FLAC, or
  MP3 under 90 seconds.
