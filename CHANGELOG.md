# Fractal-Garden: version history

The detailed notes for each milestone, newest first. A version is listed as tested only after it has been tested in Logic on both
test Macs and worked as described. The README describes the current version.

Version 1.3.0 (ALL GATES 100%, CLEAR RATCHETS and the new starting pattern; **built 9 November 2026, waiting for the Logic test**).

- From Bryn's wish list of 9 November 2026 (items 1 to 3), approved by Bryn on 9 November 2026.
- Two controls appended after the 70 (72 in all): `gatesFull` (id 71, "All Gates 100%") and `ratchetsClear` (id 72, "Clear Ratchets"), momentary buttons. The 70 ids and the order of every menu are unchanged.
- Engine: `setGatesFull` and `clearRatchets` in `fg/Steps.h` / `Steps.cpp` (a span of steps: gate length to 100 %, or ratchet to x1; nothing else is touched, ON / OFF included) and `StepData::makeDefault` (every step ON, note 12 = C3 at the default settings, gate 100 %, ratchet x1, slew 0, Mod 0, fine pitch 0). `makeSample`, the old starting melody, is kept for the automatic checks only.
- Plugin: the two buttons are counted like the other buttons and carried out on the window thread. They use RESEED SCOPE (the page the editor shows, or all 64, steps past LENGTH included), they do not move the UNMUTATE point (so UNMUTATE undoes them) and they leave the branches as they were, exactly like the RESEED buttons. Loading a project never presses them.
- Plugin: a new instance starts with the default pattern instead of the sample melody; so does a project whose step data is missing or damaged. A project saved by an earlier version keeps the pattern it was saved with (the step data is in the project). With the default pattern and the default settings the plugin plays one held C3: the same note tied to itself sends no new note (as before, ledger G-34).
- Panel: ALL GATES 100% and CLEAR RATCHETS on the title line of the RESEED card. The plain-slider window shows 72 controls (24 rows per column, three columns, now full).
- No change to the saved step format (still 4), the clock, the pattern bank or the shared pattern file.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-160 to G-165; G-87 and G-159 revised).
- Automatic checks: engine 180,479; clock 6,406,793 (unchanged); window and plugin test 504 (new: a new instance holds the default pattern and plays one held C3 that ends when Logic stops; UNMUTATE after the first RESEED returns to it; a project saved with the old melody opens with it; each button with scope PAGE and ALL 64 changes only what it says; both are saved with the project, are not pressed by loading, leave the branches alone and are undone by UNMUTATE; both buttons exist on the panel and work when clicked).
- GitHub run 42727039093 (commit 5d42445, 9 November 2026 Phoenix time): green. Engine self-test and the macOS build of AU + VST3 with its checks and package passed; private draft release test-build-27. The window and plugin test (504 checks) was run on a Linux computer with a virtual screen and passed.

Version 1.2.0 (Range and Pitch snap; **tested in Logic on 8 November 2026: Bryn reported "truly incredible and incredibly musical"; both Range and Pitch snap work**).

- Three controls appended after the 67 (70 in all): `rangeOctaves` (id 68, 1 to 5, default 2), `pitchSnap` (id 69, STEP / SEMITONES / FREE, default STEP) and `bendRange` (id 70, 1 to 24 semitones, default 2). The 67 ids and the order of every menu are unchanged.
- Range: counted up from the Base note. It limits RESEED NOTE and ALL, the Randomize (?) branches, Mutate, Micro Mutate never touches notes, and Mutate FX (note changes). Hand edits, existing patterns, the T and I branches and ornaments are not limited. A note already above the range stays until something mutates it, then it is pulled inside. Old projects open with Range 2, so an old pattern with high notes keeps playing them until mutated. At Range 5 the random draws are the same as in 1.1.0.
- Pitch snap: every step gets a fine pitch in cents (saved step format 4; a project saved by 1.1.0 or earlier opens exactly as it was, fine pitch 0; pattern bank and shared pattern file carry it automatically). STEP plays the scale notes (fine pitch ignored, no pitch bend ever sent: the output is byte-for-byte that of 1.1.0 at Range 5). SEMITONES plays the nearest whole semitone (50 cents rounds up) with no bend. FREE plays the nearest semitone and sends the rest as pitch bend, scaled by Bend range. The bend goes back to the centre when Logic stops, on bypass and on a MIDI channel change.
- Panel: SETTINGS page has Range, Bend range and Pitch snap (a new row at the bottom; the page is taller). Note tab: STEP as before; SEMITONES click or drag gives whole semitones anywhere in the range; FREE drags relative to where the note is (10 cents a pixel, Shift 1 cent), the mouse wheel moves by a semitone (SEMITONES) or 10 cents (FREE, Shift 1 cent), double-click puts the note back; the read-out shows cents in FREE. The plain-slider window has 24 rows per column.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-07, G-08, G-67, G-74, G-82, G-104, G-136, G-147 revised; G-148 onward).
- Automatic checks: engine and clock tests pass (new: step format 4, pitch maths, Range limits, same draws at Range 5, no bend in STEP, bend before the note in FREE, centre on stop); window and plugin test 481 (new: defaults, STEP, SEMITONES, FREE and stopping, Range 1 reseed). The window and plugin test was run on a Linux computer with a virtual screen.

Version 1.1.0 (SHARE and GRAB, the canon patch; **tested in Logic on 8 November 2026: worked as described**).

- Two controls appended after the 65 (67 in all): `patternShare` and `patternGrab`, momentary buttons. The 65 ids and the order of every menu are unchanged; the defaults leave every existing behaviour as it was, so a 1.0.0 project sounds the same.
- Engine: `sharedPatternToBytes` / `sharedPatternFromBytes` in `fg/Patterns.h` / `Patterns.cpp`: one pattern (64 steps and LENGTH) as bytes ("FGSP", version 1, LENGTH, size, step data). Anything that is not exactly that (wrong start, other version, LENGTH 0 or over 64, cut short, extra bytes) is refused and changes nothing.
- Plugin: SHARE writes the steps as they are now (mutations included, Performance effects never) and LENGTH to `shared-pattern.fgpattern` in a `Fractal-Garden` folder in the Music folder, replacing a file already there. GRAB reads it and does what LOAD does: replaces the steps and LENGTH, is the new UNMUTATE point, makes the branches again, whatever the branch-regeneration filter says. A missing or damaged file changes nothing. Scale, Key, Root, Order, Rate, Ratio and the knobs are not part of the file. Loading a project never presses SHARE or GRAB, even if one was on when the project was saved.
- Panel: SHARE and GRAB next to LOAD and SAVE on the PATTERN card (slot menu and buttons are a little narrower, the ROOT card too); a line at the bottom right shows the shared file's state ("SHARED FILE: n STEPS" or "NONE") or why the last SHARE or GRAB did not work. The plain-slider window now has 23 rows per column.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-138 revised; G-139 onward).
- Automatic checks: engine 179,996; clock 6,405,691; window and plugin test 467 (new: SHARE writes the file, the other instance sees it, GRAB brings the steps and LENGTH and leaves the scale alone, GRAB is an edit that UNMUTATE returns to, ROTATE after GRAB, a missing, damaged and unwritable file, loading a project with SHARE or GRAB on). A message about a failed GRAB goes away once a good file is there.
- GitHub run 42488865264 (commit 67eda15, 8 November 2026): green. Engine self-test, universal build check (Apple Silicon + Intel), ad-hoc signing, auval, pluginval (AU and VST3), package and private draft release (test-build-21) all passed. The window and plugin test (467 checks) was run on a Linux computer with a virtual screen and passed.

Version 1.0.0 (milestones 9 and 10, release; **0.8.0 was tested in Logic on 7 November 2026 and worked; 1.0.0 changes only the version number and the documents**).

- Version number 1.0.0. No change to the engine, the panel, the 65 controls or the saved state: a 0.8.0 project opens in 1.0.0 exactly as it was.
- README and CHANGELOG finished; the manual gaps ledger is complete (G-01 to G-138). Open items there: G-08 (unquantized scale / pitch bend, LATER) and G-138 (canon patch: not built; Logic's copy of plug-in settings plus ROTATE is the suggested way).
- Automatic checks unchanged from 0.8.0: engine 179,965; clock 6,405,691; window and plugin test 441.

Version 0.8.0 (milestones 9 and 10, first part: pattern bank and settings page; **tested in Logic on 7 November 2026: "sounds incredible and phenomenal. all good."**).

- New `fg/Patterns.h` / `Patterns.cpp`: the 16-slot pattern bank. A slot holds the 64 steps and LENGTH; the saved form is versioned and damaged data is refused without changing anything.
- Plugin: PATTERN LOAD and SAVE are counted like the other buttons and carried out on the window thread. SAVE stores the steps and LENGTH; LOAD replaces them (an empty slot does nothing), sets the UNMUTATE point and makes the branches again. The bank is saved with the project; loading a project resets the pending button counts and never acts on a button that was on when it was saved.
- 19 controls appended after the 46 (65 in all): `midiChannel`, `ccSlew`, `ccMod`, `baseNote`, `resizeMethod`, five Reseed filter switches (`reseedFilterNote`, `Gate`, `Slew`, `Ratchet`, `Mod`) and nine branch-regeneration switches (`regenNote`, `regenGate`, `regenSlew`, `regenRatchet`, `regenStepOnOff`, `regenLength`, `regenResize`, `regenRotate`, `regenTranspose`). The defaults give exactly the behaviour of 0.7.0 (channel 1, CC 84 and CC 1, base note 48, Spread, everything on). The 46 ids and the order of every menu are unchanged.
- Engine: `PlayerSettings` gains `slewCc`, `modCc` and `baseNote`; a change of CC numbers while playing sends the current values again on the new numbers. `pitchFor` takes a base note (default 48). `resizeExpand` takes a method (Spread, Stretch, Clone). `reseedSteps` takes a mask for RESEED ALL.
- Plugin: a change of MIDI channel while a note sounds ends it on the old channel (note-off and all-notes-off on the old channel).
- Panel: SETTINGS... in the header opens the settings page (a pop-over like CHANCE...); the PATTERN card shows the slot's state; the plain window fits 65 controls (three columns of 22).
- Not built: sharing patterns between instances (the canon patch). Proposed: Logic's copy and paste of plug-in settings for now; a shared pattern file only if that fails.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-122 onward).
- Automatic checks: engine 179,965; clock 6,405,691; window and plugin test 441. GitHub run 42425937643 (commit b883c97, 7 November 2026): green. Engine self-test, universal build check (Apple Silicon + Intel), ad-hoc signing, auval, pluginval (AU and VST3), package and private draft release all passed.

Version 0.7.0 (milestone 8, Performance mode and ornamentation; **tested in Logic on 7 November 2026: "sounds fabulous", finished**).

- New `fg/Perform.h` / `Perform.cpp`: the zones of Ratchet FX, Slew FX and Mutate FX, the fifteen ornaments (seven two-note, seven four-note, the eight-note trill) and `performStep`, a pure function of (stored step, step counter, seed, knobs): no running random stream, so a step's effect is the same at any block size and when the player looks ahead at the next step.
- Trunk player: when a step starts, a copy gets the effects and the copy plays (ratchet, slew CC 84 and the glide hold use it). Stored steps, the mutation log, Unmutate and the saved pattern are untouched. Ornaments play eighth notes (half a step each, 80 % long) from the start of their step and take over the steps they cover. With the three knobs at 0 nothing is drawn: the output is identical to 0.6.0.
- Plugin: the three Performance knobs reach the player (they act whichever Knob Mode is showing); the read-outs name the zone, the fixed ratchet amount (x1 to x32), the fixed slew and the ornament group.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-97 onward; G-05, G-14, G-20 and G-21 revised).
- No new controls; all 46 ids unchanged.
- GitHub run 42423864247 (7 November 2026): Engine self-test passed (18 s); macOS AU + VST3 build passed (5 min), installer zip and draft release test-build-14 made.
- Checks: engine 179,652; clock 6,404,027; window and plugin test 338 (new: Performance knobs reach the player, stored steps unchanged, knobs back at 0 restore the notes); byte-for-byte comparison with 0.6.0 at knobs 0 (120 runs).

Version 0.6.0 (milestones 6 and 7 merged: Branch, Path, Mutate, Micro Mutate, Reseed, Unmutate, TREE letters; **tested in Logic on 7 November 2026: "all looks good", finished**).

- New `fg/Mutate.h` / `Mutate.cpp`: seeded random generator, Default Mutate (two zones), Micro Mutate, Reseed ranges for every kind.
- New `fg/Branch.h` / `Branch.cpp`: Reverse, Inverse, Transpose, Mutate, Randomize; Path as a binary tree with the seed, so a path repeats; branches are a pure function of (snapshot, LENGTH, SCALE, count, path, seed).
- Trunk player: sections (trunk, then branches); Mutate and Micro Mutate change the step as it starts and log trunk changes for the window thread; with branches 0 and knobs 0 the output equals 0.5.0 exactly.
- Plugin: mutations are saved by the window thread (changes made before a newer edit are dropped); edits set the Unmutate point and the branch snapshot; Rotate, Transpose and Resize make the branches again; Unmutate restores steps and LENGTH; seven Reseed buttons with Page / All 64 scope; branch seed, Unmutate point and branch snapshot are saved with the project; TREE letters come from the same function as the player; the header says TRUNK or BRANCH n.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-64 onward; G-01 to G-04, G-07, G-14, G-16, G-18, G-33 now BUILT).
- No new controls; all 46 ids unchanged.
- Checks: engine 166,852; clock 1,002,437; window and plugin test 333.
- GitHub run 7 November 2026 (draft release test-build-11): engine self-test passed; macOS build (AU + VST3, universal, auval, package) passed.

Version 0.5.0 (milestone 5, gate length with ties, ratchet, slew, mod and resize; **tested in Logic on 7 November 2026: "all good", finished**).

- New `fg/Mod.h` / `Mod.cpp`: CC numbers (Mod 1, Slew 84), the eight Mod shapes, the sixteen Mod rates (one-shot, ratchet, halves, quarters, two fixed levels), Envelope and Smooth curves, 0..1 to 0..127.
- Trunk player: ties at gate 100 % (new note first, old note-off at the same sample right after; same pitch = no new note; tie into OFF ends at that step's start; ratcheted steps never tie); ratchet amounts x1 x2 x3 x4 x6 x8 x16 x32 with notes of half a slot (at least 1 ms); CC 84 before the note when the value changes, with legato hold when the next step has slew; CC 1 per Mod mode, moving values on a 10 ms grid, only changes sent; at most 100 CC messages in any one second; everything identical for any block size.
- Saved step format version 3 (ratchet = place in the list, 16 Mod rates); versions 1 and 2 convert on load.
- Resize: SHRINK keeps steps 1, 3, 5... and halves LENGTH (rounded up); EXPAND follows each step with an OFF copy and doubles LENGTH (refused above 64); done on the window thread like ROTATE, LENGTH is set to match.
- Plugin: Mod mode handed to the player, CC events written in the player's order; editor shows the ratchet list, the real Shapes curve at its rate, click the caption strip for the next Mod rate (shift / right click back), double-click resets shape and rate.
- Manual gaps: see the project file "Fractal-Garden manual gaps ledger" (G-34 to G-57 plus later additions).
- Known: in the 100 per second cap test the last 10 ms tick before a step boundary can be dropped by the cap, so the Smooth test accepts a peak of 120 or more in that window.
- No new controls; all 46 ids unchanged.
- Checks: engine 2,111; clock 1,002,337.

Version 0.4.0 (milestone 4, trunk editing and the pitch controls; **tested in Logic on 7 November 2026: everything worked as expected and as described**).

- Removed the milestone 3 test aids: the clickable STEP lamp, the test note and the diagnostic counters (milestone 3 result recorded: Logic does not call a MIDI effect while stopped).
- New `fg/Pitch.h` / `Pitch.cpp`: the 28 scales (the Dotriaconta-tone table, same order). A step stores octave x 12 + degree (0 to 59); the played MIDI note is 48 + KEY + 12 x octave + scale interval (after ROOT), clamped 0 to 127. SCALE and KEY apply as each note plays, so stored steps never change with them.
- Step data format version 2; versions 1 (MIDI notes, from 0.2.0 / 0.3.0) are converted to the nearest degree of C major when loaded. Default step note and the sample pattern now use degrees.
- New `fg/Orders.h` / `Orders.cpp`: 11 orders as pure functions of the step counter (so they stay locked to Logic and block-size independent): Forward, Reverse, Pendulum (+ REPEAT ENDS), Random, Converge, Diverge, Converge & Diverge, Page Jump, Drunk walk, Random no-repeat, Chance mix. The last three, Pendulum with repeated ends, Random and the hashing are carried over from Dotriaconta-tone. Random orders use a seed made per instance and saved with the project (property "orderSeed").
- LENGTH, SCALE, KEY, ORDER, REPEAT ENDS, CHANCE... , ROOT, RESET, ROTATE LEFT / RIGHT and TRANSPOSE DOWN / UP act. ROTATE and TRANSPOSE change the stored steps inside LENGTH (RIGHT: step 2 becomes step 1; TRANSPOSE counts steps of the current scale, stopping at the ends of the 5 octaves). RESET: the next step is step 1 until Logic stops, jumps or cycles. Momentary buttons are counted when pressed on any thread and acted on once; loading a project is not a press.
- Editor: Note box moves by scale degrees with a note-name read-out; double-click puts a step's value back to default; page chips beyond LENGTH are greyed and unclickable; the page follows LENGTH when it shrinks; steps beyond LENGTH cannot be edited.
- Manual gaps and approximations: Converge / Diverge / Converge & Diverge shapes (1 8 2 7 3 6 4 5, its reverse, alternating); Page Jump (pages of eight, random page per visit); the manual's "Random" matches our Random no-repeat; ROOT clamp to MIDI 0 to 127; ROTATE meaning; RESET timing.
- No new controls; all 46 ids unchanged.
- Checks: engine 1,214; clock 575,208; window and plugin test 246.

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
