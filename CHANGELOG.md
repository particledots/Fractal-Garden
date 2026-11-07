# Fractal-Garden: version history

The detailed notes for each milestone, newest first. A version is listed as tested only after it has been tested in Logic on both
test Macs and worked as described. The README describes the current version.

Version 0.3.0 (milestone 3, the Logic proving ground; **tested in Logic on both Macs on 7 November 2026: every check passed**). The trunk plays, locked to Logic.

- New engine file `fg/Clock.h` / `Clock.cpp`: `TrunkPlayer` turns Logic's song position (quarter notes at the start of each block), tempo, time signature, RATE, RATIO and LENGTH into note starts and stops with sample positions. Step number = position / step length, modulo LENGTH, so the pattern is locked to Logic's bars and two instances agree without talking to each other.
- The trunk plays: Forward order, each step's stored note, ON / OFF, gate length (5 % to 100 % of the step; 100 % runs to the next step), velocity 100, MIDI channel 1. No scale, key, order, ratchet, slew or mod yet (milestones 4 and 5).
- RATE, RATIO and LENGTH act from this version (the control list had them in milestone 4); all 46 ids and the saved format are unchanged.
- Safety: the sounding note ends exactly at a stop, a jump of more than 0.02 quarter notes (relocate or cycle), or when Logic stops giving a song position; bypass ends it if Logic calls the plugin while bypassed; the first block after Logic (re)starts its audio carries one all-notes-off; a note-on is only written when there is room for its note-off.
- Window: the playhead (overview strip and step columns), the teal STEP and amber GATE lamps, and the header STEP read-out follow playback; the window looks 30 times a second (was 20) and repaints only the header for the lamps. A small line under the header shows BAR and BEAT.
- Temporary test aids: clicking the STEP lamp sends one short test note (C4, a quarter of a second) while Logic is stopped (dropped if not picked up within 1.5 s); counters for blocks handled while stopped, test notes asked / sent, and blocks with no song position. Logic result: 0 test notes sent of 18 and of 28 asked (one per Mac), so Logic does not call a MIDI effect while stopped; no song-position gaps (NO POSITION 0). The aids are to be removed at the start of milestone 4.
- Approximations (our own design, the manual is silent on DAW sync): RATE is a note length against Logic's tempo ("1 bar" follows the time signature); RATIO multiplies the speed; step 1 sits on bar 1 beat 1.
- Checks: engine 834; clock 320,797; window and plugin test 219 (its "RESEED ALL turns it on" check no longer depends on the phase of the 20-a-second housekeeping timer).
- GitHub run on commit b6c36c1: engine test (834 checks), clock test (320,797 checks), universal build (arm64 + x86_64), ad-hoc signing, auval (aumi Fgdn Pdot) succeeded, pluginval level 5 passed for AU and VST3, package and draft release all green.

Version 0.2.0 (milestone 2, the first build with the styled panel; **tested in Logic on both Macs on 7 November 2026: every check passed, "nothing feels wrong at this stage"**). Nothing plays.

- AU + VST3 MIDI effect (AU type aumi, manufacturer Pdot, code Fgdn), universal (Apple Silicon + Intel), macOS 11+, ad-hoc signed, installer, automatic build.
- **46 controls** from the approved control list (ids frozen from now on).
- Styled panel matching pictures A, B and C; Plain Sliders window; Knob Mode swaps the three knob cards; REPEAT ENDS / CHANCE... slot follows Order; Chance mix pop-over.
- Step editor with sample data; step data, tab, page and window choice are saved with the project.
- MIDI passes through unchanged; the plugin adds nothing. Header shows Logic's play state and tempo.
- Approximations (manual vague): Gate / Slew / Ratchet / Mod ranges, eight Mod captions, knob read-outs, Ratchet FX zone 2 as "fixed x2...x8".
- Engine self-test 834 checks; window test 189 checks. GitHub run on commit 481750b: engine test, universal build, signing, auval (aumi Fgdn Pdot) succeeded, pluginval, package and draft release all green.
