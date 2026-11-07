# Fractal-Garden: version history

The detailed notes for each milestone, newest first. A version is listed as tested only after it has been tested in Logic on both
test Macs and worked as described. The README describes the current version.

Version 0.2.0 (milestone 2, the first build with the styled panel; **tested in Logic on both Macs on 7 November 2026: every check passed, "nothing feels wrong at this stage"**). Nothing plays.

- AU + VST3 MIDI effect (AU type aumi, manufacturer Pdot, code Fgdn), universal (Apple Silicon + Intel), macOS 11+, ad-hoc signed, installer, automatic build.
- **46 controls** from the approved control list (ids frozen from now on).
- Styled panel matching pictures A, B and C; Plain Sliders window; Knob Mode swaps the three knob cards; REPEAT ENDS / CHANCE... slot follows Order; Chance mix pop-over.
- Step editor with sample data; step data, tab, page and window choice are saved with the project.
- MIDI passes through unchanged; the plugin adds nothing. Header shows Logic's play state and tempo.
- Approximations (manual vague): Gate / Slew / Ratchet / Mod ranges, eight Mod captions, knob read-outs, Ratchet FX zone 2 as "fixed x2...x8".
- Engine self-test 834 checks; window test 189 checks. GitHub run on commit 481750b: engine test, universal build, signing, auval (aumi Fgdn Pdot) succeeded, pluginval, package and draft release all green.
