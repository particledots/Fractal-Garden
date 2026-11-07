# Fractal-Garden

An original AU / VST3 **MIDI effect** for Logic Pro, inspired by the feel of a Eurorack 3-channel, 64-step fractal CV / gate
sequencer (the Qu-Bit Electronix Bloom v2): 64 steps, branches that grow from a trunk, and knobs that mutate the pattern. All code is
our own; it is not a port or copy of anything, and it is not affiliated with Qu-Bit Electronix.

**Status: version 0.3.0 (milestone 3: the Logic proving ground). Tested in Logic on both test Macs on 7 November 2026: every check passed and it is finished.** The one experiment, a test note while Logic is stopped, showed that Logic does not call a MIDI effect while stopped (test notes 0 sent of 18 and 0 of 28 asked). Milestone 2 (0.2.0) was tested in Logic on both test Macs (M4 Mac mini, Logic 11.2.2; Intel Mac, macOS 12.7.8, Logic 10.7.8) on 7 November 2026, every check passed, and it is finished.
**The first trunk notes now play, locked to Logic's transport.** Press play in Logic and the plugin plays your 64-step trunk (Forward order, each step's note, ON / OFF and gate length, as stored: no scale, key, order, ratchet, slew or mod yet) on MIDI channel 1. RATE, RATIO and LENGTH already work. The playhead, the STEP and GATE lamps and the header STEP read-out follow Logic's playback. Everything else (the 17 buttons, Scale, Key, Order, Root, Branch, Path, Mutate and the rest) still only moves and is saved; those start working in milestones 4 to 9.

## First five minutes (after installing, see *Getting the plugin*)

1. In Logic, make a **software instrument** track and choose a sound. In its **MIDI FX** slot choose **Audio Units > particledots > Fractal-Garden** (Logic may list it as "MIDI Controlled / particledots").
2. The window opens at about 90 % size; drag a corner to resize it (the shape stays 3:2).
3. Press **play** in Logic. The sample pattern plays 16th notes; the teal STEP lamp blinks on every step, the amber GATE lamp lights while a note sounds, and the teal highlight runs along the step columns and the overview strip. The header reads LOGIC PLAYING with the tempo and, in the first line under it, STEP n of LENGTH.
4. Press **stop**: the note ends at once and the lamps go dim. Move the playhead to another bar, or turn on Logic's cycle: the pattern stays on Logic's bars and no note is left sounding.
5. Change **RATE** (1 bar to 1/32), **RATIO** (/8 to x8) and **LENGTH** while it plays. Click a step's **ON / OFF** and drag inside a step's box to change its note: the next time that step comes round you hear it.
6. Put a second instance on a second track and a different sound: both play in step with each other.
7. **Test note (temporary):** with Logic stopped, click the teal **STEP lamp** in the header. If the plugin sounds one short note (C4), a MIDI effect can make sound while Logic is stopped. The small line under the header counts the blocks Logic has handed the plugin while stopped and the test notes asked for and sent (see *Things worth knowing*).
8. Save the project, close it, reopen it: every control, the step data, the tab, the page and the window you were using come back.

## The window

Top: title, a header showing Logic's play state and tempo, the step now playing (STEP n/LENGTH), the STEP and GATE lamps, and the PLAIN SLIDERS button. Under the header a small line shows Logic's song position (BAR and BEAT) and the temporary test counters.
The step editor (tabs, Mod Out chips, page chips, an overview of all 64 steps, eight step columns). Three knob cards (Branch, Path, Mutate; or Micro Ratchet, Micro Slew, Micro Mod; or Ratchet FX, Slew FX, Mutate FX). Knob Mode, Reseed, Tree. Then Length, Scale, Key, Order, Rate, Ratio, and Pattern, Root, Transpose, Rotate, Resize, Reset, Unmutate.

## Things worth knowing

- **46 controls**, ids frozen from 0.2.0; new controls will be appended. Logic's own plain view and the automation list show all 46.
- **Locked to Logic's bars.** The step that plays is worked out from Logic's song position (step number = position / step length, then modulo LENGTH), not from a counter of our own. Step 1 sits on bar 1, beat 1 of the song, so the pattern keeps its place when you start from the middle, jump, or loop with cycle. If LENGTH is not a whole number of bars, the pattern still lines up with the song, so a pass can start in the middle of a bar.
- **Speed.** RATE is the length of one step against Logic's tempo (1 bar follows Logic's time signature; T means triplet). RATIO makes it faster (x2, x3, x4, x8) or slower (/2, /3, /4, /8). The Bloom v2 manual says nothing about syncing to a DAW, so this is our own design.
- **Stops, jumps and bypass.** The sounding note ends the moment Logic stops, relocates or cycles. If Logic gives the plugin no song position while playing, it is treated like a stop. Bypassing the track's MIDI effect ends the note if Logic calls the plugin while bypassed (to be checked in Logic). Incoming MIDI (keys played into the track) passes through unchanged, next to the sequencer's notes.
- **Step data used so far:** note (as stored, 48 to 108), ON / OFF and gate length (a percentage of the step; 100 % runs into the next step; never shorter than 5 %). Velocity is fixed at 100.
- **Test note and counters (temporary):** clicking the STEP lamp asks for one short note while Logic is stopped. It is dropped if Logic does not call the plugin within about a second and a half, so it can never sound late. STOPPED BLOCKS counts how often Logic called the plugin while stopped (0 means it never does, so nothing could ever sound while stopped); TEST NOTES sent/asked shows how many clicks reached the sound; NO POSITION counts blocks where Logic was playing but gave no song position.
- The 17 buttons (Reseed, Reset, Unmutate, Rotate, Resize, Transpose, Pattern Load / Save and so on) **do nothing yet**: they glow briefly and pop back off. They start working in milestones 4 and 5.
- The ranges of Gate, Slew, Ratchet and Mod, the eight Mod captions, and the wording under each knob are **first guesses**; where the Bloom v2 manual is vague (for example the exact Performance zones) we approximate and will tune it with you.
- Window and tab choices are saved in the project but are not controls (no automation).

## What has been checked (without Logic)

- Engine self-test: 834 checks (control table, menus, note names, step-data save / load and rejection of damaged data).
- Clock self-test: 320,797 checks. A simulated Logic hands the player its song position block by block; the test checks every note start and stop to the sample: all 9 RATEs with all 9 RATIOs, block sizes from 1 to 8192, 44.1 / 48 / 88.2 / 96 kHz, tempos from 60 to 300 bpm, positions a hair off from block to block, start in the middle of a step, a count-in before bar 1, stop in the middle of a note and start again, relocate, cycle (gate 50 % and 100 %), tempo changes, RATE / RATIO / LENGTH changed while playing, time signatures 3/4, 6/8, 7/8 and 5/4, two instances (one starting later, with a different block size) staying in step, a full output list never losing a note-off, and that no note is ever left sounding. The test was also run against deliberately broken versions of the clock to see that it notices.
- Window and plugin test on a Linux computer: 219 checks (all 46 controls exist once in each window, switching windows, saving and reloading, momentary buttons popping back, Knob Mode titles and colours, the Order slot, the Chance pop-over, and a simulated Logic transport driving the real plugin: notes and their timing, the header STEP read-out and lamps, stop, relocate, bypass, no song position, the temporary test note, RATE / RATIO / LENGTH and an edited step, two instances, incoming MIDI passing through). Pictures drawn from the real window were compared with pictures A, B and C.
- GitHub Actions (commit b6c36c1): engine test and clock test, macOS build (universal Apple Silicon + Intel), auval, pluginval level 5 for AU and VST3: all passed (details in CHANGELOG).
- **Tested in Logic (7 November 2026, both Macs: M4 Mac mini, Logic 11.2.2; Intel Mac, macOS 12.7.8, Logic 10.7.8).** Everything passed: Logic gave a song position every time (NO POSITION stayed 0), including cycle, relocation and tempo changes; bypass behaved; the playhead, lamps and header followed playback. **Result of the stopped-test note:** 0 sent of 18 asked on one Mac and 0 sent of 28 asked on the other, so Logic does not call a MIDI effect while stopped. The MIDI version can only make sound while Logic plays; "click a step to hear it" would need the built-in sound engine. The test note, the clickable STEP lamp and the counters are to be removed at the start of milestone 4.

## What changed in 0.3.0

The sequencer clock and the trunk player (`engine/include/fg/Clock.h`), wired into the plugin: reads Logic's play state, tempo, time signature and song position, plays the trunk locked to them, ends notes safely on stop, jump and bypass, and drives the playhead, lamps and STEP read-out. Header text now shows the step (STEP n/LENGTH). The temporary test note and counters. No new controls; the saved project format is unchanged, so 0.2.0 projects open as they were. See CHANGELOG for the full notes and 0.2.0.

## Folders

- `engine/` plain C++ (control table, step data, the sequencer clock), with its self-tests; no plugin code.
- `plugin/` the JUCE wrapper, the styled panel, the plain window, fonts, and `preview/` (the offline window checks).
- `.github/workflows/` the automatic build. `tools/install/` the installer script.

## Getting the plugin

Open the GitHub **Actions** tab of this repository, open the latest green run and download the artifact **FractalGarden-macOS** (a zip). Unzip it and double-click **Install-Unofficial-Plugin.command** (INSTALL.txt explains what to do if macOS asks you to confirm). Restart Logic.

## Notes

Original work by particledots; inspired by, and not affiliated with, Qu-Bit Electronix. JUCE is used under its licence; the fonts (Space Grotesk, IBM Plex Mono) under the SIL Open Font Licence (copies in `plugin/Fonts`).
