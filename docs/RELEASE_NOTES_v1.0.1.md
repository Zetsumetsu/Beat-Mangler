# Break Mangler v1.0.1

A generative re-composer for breaks. Load a break or any sample, perform the knobs while
it plays, record the output, chop it up later. That is the whole instrument.
The intent of the instrument is to make quick work with those vanilla flavored loops that 
never change and re-compose them so they have variation throughout an arrangement.

## The panel

Five knobs do the work. **CHAOS** is the macro: order on the left, beautiful
chaos on the right. **DENSITY** stutters and reverses slices. **PITCH** throws
per-slice interval jumps. **DEGRADE** crushes slices down to grit. **ERA** is
the S950-style output stage: drive, 12-bit crush, and a darkening filter that
goes from clean modern to full vintage punch.

**RATE** (1/4, 1/2, 1, 2) sets the trigger rate against the host tempo.
**GRID** chops the loop into 16ths or beats. **SNAP** quantizes the pitch
jumps: glide, semitones, or minor pentatonic. **LOAD** (or drag and drop onto
the waveform) pulls in any WAV, AIFF, FLAC, or MP3 under 90 seconds.
**PLAY/STOP** runs the internal transport, quantized to the bar. **RESET**
zeroes the five knobs back to the clean sample. **AUTO** switches between
following your DAW's automation and a manual snapshot you can ride freely.

Near-tempo loops are fitted to the bar grid on load. Everything is
automatable and responds to LFOs like any plugin parameter.

## Install

**Windows:** copy `Break Mangler.vst3` to `C:\Program Files\Common Files\VST3`,
rescan in your DAW.

**macOS:** copy `Break Mangler.vst3` to `/Library/Audio/Plug-Ins/VST3`,
rescan in your DAW. This is a universal binary (Intel + Apple Silicon).
The plugin is unsigned, so macOS will ask you to allow it once:
right-click the plugin in Finder, choose Open, and confirm.

Built with JUCE 8. DSP core is portable C++17 with no allocations in the
audio path.

## v1.0.1

Fixes bar detection failing on some systems: sample loading now uses the
host tempo tracked on the audio thread instead of querying it from the UI
thread, so loops are sliced to the bar grid reliably and play back
faithfully with all knobs at zero.

---

Break Mangler is free. I build these tools for my own music first. If one
earns a place in yours and you'd like to support more of them, the Sponsor
button is there if you feel like it.
