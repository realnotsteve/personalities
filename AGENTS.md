# Project: PRISM Personalities (VST/AU) plugin using JUCE (C++)

## Identity check (always do this first)
- Confirm the working directory is this JUCE plugin project: personalities
- Do not apply WordPress/PHP assumptions here.

## Non-negotiables (real-time audio safety)
- Audio thread must be real-time safe:
  - No heap allocations (no new/delete, no growing std::vector, no std::string building).
  - No locks/mutexes/waiting, no file IO, no network, no logging/printf in the callback.
  - Avoid blocking calls and OS APIs in processBlock.
- Prefer preallocation in prepareToPlay; reuse buffers.
- Any parameter access must be thread-safe (use AudioProcessorValueTreeState / atomics as appropriate).

## JUCE conventions
- Use AudioProcessorValueTreeState (APVTS) for parameters unless the project already uses another pattern.
- UI (message thread) must not touch audio state unsafely; use atomics or lock-free handoff.
- Keep DSP and UI separated (e.g., dsp/ module or Processor vs Editor responsibilities).

## Performance expectations
- Assume 48kHz, 64–256 sample buffers, multiple instances.
- Minimize denormals risk; use ScopedNoDenormals where appropriate.
- Avoid per-sample expensive operations unless necessary; prefer vectorized or per-block math.

## Code style and workflow
- Keep diffs small and localized. Avoid refactors unless requested.
- Do not introduce new third-party dependencies without asking.
- Do NOT print entire files unless explicitly requested.

## Testing checklist (minimum)
- Build plugin (Debug and Release if feasible).
- Load in a host (Logic/Reaper/Ableton etc.) and verify:
  - audio passes without crackles
  - parameters automate smoothly
  - no UI thread hangs
- Run a quick “stress”: rapid parameter automation + multiple instances.

## Output style (how to respond)
- When proposing changes: specify file path + class/function + why it’s real-time safe.
- If a change touches the audio thread, explicitly state how allocations and locks are avoided.