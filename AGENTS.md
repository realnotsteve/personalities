# Project: PRISM Personalities (JUCE audio plugin, macOS)
Slug: personalities

## Project identity (pin context)
- This workspace is a JUCE/C++ audio plugin project.
- Do not apply WordPress / PHP / Elementor assumptions here.
- If a request clearly refers to WordPress, respond that it is out of scope for this workspace and ask the user to switch to the relevant WP workspace.

## Current target + workflow
- Proof-of-concept plugin for local use only.
- Primary host: Studio One on macOS.
- Primary format: VST3 first (AU optional later).
- Development model: iterative edits driven by Codex; keep changes small and test frequently.

## What the plugin does (core behavior)
- MIDI in → (fixed delay in milliseconds + processing) → MIDI out.
- Delay must be sample-accurate across audio blocks.
- Delay all relevant MIDI messages consistently (note-on, note-off, CC64 sustain, pitch bend, etc.) to avoid stuck notes.

## Non-negotiables (real-time audio safety)
- Audio thread must be real-time safe:
  - No heap allocations (no new/delete; no growing vectors/strings).
  - No locks/mutexes/waiting; no file IO; no network; no logging/printf in the callback.
  - Avoid blocking calls and OS APIs in `processBlock`.
- Preallocate in `prepareToPlay`; reuse buffers.
- Parameter reads must be thread-safe (APVTS/atomics as appropriate).

## JUCE conventions
- Prefer AudioProcessorValueTreeState (APVTS) for parameters unless the project already uses a different pattern.
- Keep DSP and UI separated (Processor vs Editor responsibilities).
- UI (message thread) must not touch audio state unsafely; use lock-free handoff or atomics.

## Performance expectations
- Assume 48kHz and 64–256 sample buffers; multiple instances possible.
- Minimize denormals risk; use ScopedNoDenormals where appropriate.
- Prefer per-block work over per-sample when possible.

## Implementation guidance (for the MIDI delay)
- Represent delay as `delayMs` parameter; compute `delaySamples = round(sampleRate * delayMs / 1000.0)`.
- Maintain a monotonically increasing sample timeline (e.g., `uint64_t timelineSample`).
- Use a preallocated queue/ring buffer of scheduled MIDI events with an absolute `dueSample`.
- In each `processBlock`, enqueue incoming events with `dueSample = timelineSample + sampleOffset + delaySamples`, then emit all queued events whose dueSample falls within the current block.
- Choose and document an overflow policy (preferred for live use: pass-through without delay when full, or drop newest).

## Testing checklist (minimum)
- Build Debug and Release if feasible.
- Verify in Studio One (live):
  - Track A: this plugin (MIDI in from controller), Monitor ON.
  - Track B: target synth, input from Track A/plugin MIDI output, Monitor ON.
  - Confirm no stuck notes; confirm timing matches delayMs.
- Stress: rapid playing + sustain pedal + automation of delayMs (if supported) without glitches.

## Output style (how to respond)
- Do NOT print entire files unless explicitly requested.
- When proposing changes: specify file path + class/function + why it is real-time safe.
- If a change touches the audio thread, explicitly state how allocations and locks are avoided.