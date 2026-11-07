# Fractal-Garden

An original AU / VST3 **MIDI effect** for Logic Pro, inspired by the feel of a Eurorack 3-channel, 64-step fractal CV / gate
sequencer (the Qu-Bit Electronix Bloom v2): 64 steps, branches that grow from a trunk, and knobs that mutate the pattern. All code is
our own; it is not a port or copy of anything, and it is not affiliated with Qu-Bit Electronix.

**Status: version 0.4.0 (milestone 4: trunk editing and the pitch controls). Built and checked without Logic; not yet tested in Logic.** Milestone 3 (0.3.0) was tested in Logic on both test Macs (M4 Mac mini, Logic 11.2.2; Intel Mac, macOS 12.7.8, Logic 10.7.8) on 7 November 2026: every check passed and it is finished. Its one experiment showed that Logic does not call a MIDI effect while stopped (test notes 0 sent of 18 and 0 of 28 asked), so the MIDI version only sounds while Logic plays.
**The trunk now has its pitch controls and all eleven orders.** LENGTH, SCALE (28) and KEY, ORDER (11), REPEAT ENDS, CHANCE... , ROOT, ROTATE, TRANSPOSE, RESET and the page chips work, and you can edit steps by scale degree. The temporary test aids from 0.3.0 (clickable STEP lamp, test note, counters) are gone. Branches, Path, Mutate, ratchet, slew and mod still only move and are saved; they start working in later milestones.

## First five minutes (after installing, see *Getting the plugin*)

1. In Logic, make a **software instrument** track and choose a sound. In its **MIDI FX** slot choose **Audio Units > particledots > Fractal-Garden** (Logic may list it as "MIDI Controlled / particledots").
2. The window opens at about 90 % size; drag a corner to resize it (the shape stays 3:2).
3. Press **play** in Logic. The sample pattern (now stored as scale degrees of C major) plays 16th notes; the teal STEP lamp blinks on every step, the amber GATE lamp lights while a note sounds, and the teal highlight runs along the step columns and the overview strip. The header reads LOGIC PLAYING with the tempo and, in the first line under it, STEP n of LENGTH.
4. Press **stop**: the note ends at once and the lamps go dim. Move the playhead to another bar, or turn on Logic's cycle: the pattern stays on Logic's bars and no note is left sounding.
5. Change **RATE** (1 bar to 1/32), **RATIO** (/8 to x8) and **LENGTH** while it plays. Click a step's **ON / OFF** and drag inside a step's box to change its note: the next time that step comes round you hear it.
6. Put a second instance on a second track and a different sound: both play in step with each other.
7. Try the new controls while it plays: **SCALE** and **KEY** (the pattern changes pitch at once and your steps are not altered), **ORDER** (Reverse, Pendulum, Converge...), **ROOT**, and the buttons **ROTATE**, **TRANSPOSE** and **RESET**. Double-click a step's box to put its value back to the default.
8. Save the project, close it, reopen it: every control, the step data, the tab, the page and the window you were using come back.

## The window

Top: title, a header showing Logic's play state and tempo, the step now playing (STEP n/LENGTH), the STEP and GATE lamps, and the PLAIN SLIDERS button. Under the header a small line shows Logic's song position (BAR and BEAT).
The step editor (tabs, Mod Out chips, page chips, an overview of all 64 steps, eight step columns). Three knob cards (Branch, Path, Mutate; or Micro Ratchet, Micro Slew, Micro Mod; or Ratchet FX, Slew FX, Mutate FX). Knob Mode, Reseed, Tree. Then Length, Scale, Key, Order, Rate, Ratio, and Pattern, Root, Transpose, Rotate, Resize, Reset, Unmutate.

## Things worth knowing

- **46 controls**, ids frozen from 0.2.0; new controls will be appended. Logic's own plain view and the automation list show all 46.
- **Locked to Logic's bars.** The step that plays is worked out from Logic's song position (step number = position / step length, then modulo LENGTH), not from a counter of our own. Step 1 sits on bar 1, beat 1 of the song, so the pattern keeps its place when you start from the middle, jump, or loop with cycle. If LENGTH is not a whole number of bars, the pattern still lines up with the song, so a pass can start in the middle of a bar.
- **Speed.** RATE is the length of one step against Logic's tempo (1 bar follows Logic's time signature; T means triplet). RATIO makes it faster (x2, x3, x4, x8) or slower (/2, /3, /4, /8). The Bloom v2 manual says nothing about syncing to a DAW, so this is our own design.
- **Stops, jumps and bypass.** The sounding note ends the moment Logic stops, relocates or cycles. If Logic gives the plugin no song position while playing, it is treated like a stop. Bypassing the track's MIDI effect ends the note if Logic calls the plugin while bypassed (to be checked in Logic). Incoming MIDI (keys played into the track) passes through unchanged, next to the sequencer's notes.
- **Notes are stored as places in the scale.** A step stores an octave (five octaves) and a degree, not a MIDI note. The note that plays is worked out as it starts from SCALE, KEY and ROOT: MIDI note = 48 + KEY + 12 x octave + the scale's interval (48 is C2 in Logic's naming, 60 is C3), limited to 0 to 127. Changing SCALE never alters stored steps, so Major to Minor Pentatonic and back loses nothing; a step whose degree the scale does not have (degree 7 in a five-note scale) plays the scale's top degree. Dragging in the Note box moves by scale degrees and the read-out shows the note name for the current SCALE, KEY and ROOT. Projects from 0.2.0 / 0.3.0 (which stored MIDI notes) are converted on load to the nearest degree of C major; the other step data is unchanged.
- **ORDER (11).** Forward, Reverse, Pendulum (REPEAT ENDS plays the two end steps twice), Random (a step may repeat), Converge (1 8 2 7 3 6 4 5), Diverge (that pass backwards), Converge & Diverge (one pass of each, alternating), Page Jump (play a page of eight steps, then jump to a random page), Drunk walk, Random no-repeat (every step once per pass, new order each pass) and Chance mix (runs in orders picked by the CHANCE... weights). Every order is worked out from Logic's song position, so it stays locked to Logic and two instances with the same settings agree. The random orders use a seed made when an instance is first created and saved with the project: the same bars replay the same way, and a second instance gets its own seed unless you copy the project state. Drunk walk and Chance mix restart their walk every 256 steps (so any bar can be found directly).
- **ROOT** shifts every note up or down by scale steps (-24 to +24 as the control allows), after the notes are worked out, clamped to MIDI 0 to 127.
- **ROTATE** moves the steps inside LENGTH for good (it changes the stored steps, like editing). RIGHT: step 2 becomes step 1 (the start point moves forward). LEFT is the opposite. Steps beyond LENGTH are not touched.
- **TRANSPOSE** moves the first LENGTH steps up or down one step of the current SCALE (for good), stopping at the lowest and highest of the five octaves.
- **RESET** makes the next step that plays step 1 of the order, and the pattern goes on from there. It re-locks to Logic's bars when Logic stops, jumps or cycles. Pressed while Logic is stopped it does nothing.
- **Page chips.** Pages beyond LENGTH are greyed and cannot be clicked; if LENGTH shrinks, the page moves down to the last page LENGTH reaches. Steps beyond LENGTH cannot be edited.
- Loading a project is never counted as a button press.
- The 12 buttons still to come (Reseed, Unmutate, Resize, Pattern Load / Save and so on) **do nothing yet**: they glow briefly and pop back off.
- **Where the Bloom v2 manual is vague, and what we did (approximations):** the manual's pictures of Converge, Diverge and Converge & Diverge did not come out as text, so the shapes above are our reading of its words. Page Jump is only described loosely; ours is pages of eight, random page each visit (the last page plays round inside its own steps if shorter than eight). The manual's "Random" (no step twice in a row until all are played) matches our Random no-repeat; our Random order is the simple one that may repeat. ROOT's range and edge behaviour are not given (we clamp to MIDI 0 to 127). ROTATE's wording is "moves the start point": we move the data inside LENGTH. RESET's exact timing is not given: we use "next step is step 1".
- The ranges of Gate, Slew, Ratchet and Mod, the eight Mod captions, and the wording under each knob are **first guesses**; where the Bloom v2 manual is vague (for example the exact Performance zones) we approximate and will tune it with you.
- Window and tab choices are saved in the project but are not controls (no automation).

## What has been checked

- Engine self-test: 1,214 checks (control table, menus, the 28 scales and pitch, step data save / load, reading old 0.2.0 / 0.3.0 data, rotate, transpose, all eleven orders: shapes, repeat, determinism and seeds).
- Clock self-test: 575,208 checks. Everything from 0.3.0 (all RATEs and RATIOs, block sizes 1 to 8192, 44.1 to 96 kHz, tempos, count-in, stop / start, relocate, cycle, time signatures, two instances) plus every order across several block sizes, the shown step in every order, SCALE / KEY / ROOT, and RESET (once, twice, from the start, while stopped, after a jump, after stop and start), and two instances in every order staying in step.
- Window and plugin test on a Linux computer: 246 checks (the milestone 3 checks, adapted, plus: no test aids left; SCALE / KEY / ROOT re-pitch the notes without changing steps; Reverse, Converge, Pendulum and Chance mix in the playing plugin; ROTATE and TRANSPOSE both ways, once per press; RESET; loading a project does not press buttons; the order seed is saved and old projects get one; page chips follow LENGTH; step editing by degree, double-click reset, no editing beyond LENGTH).
- **Not yet tested in Logic.** Milestone 3 was (7 November 2026, both Macs, everything passed).

## What changed in 0.4.0

New engine files `fg/Pitch.h` (the 28 scales and the pitch of a stored note) and `fg/Orders.h` (the eleven orders); the trunk player and the saved step format (version 2; old data is converted) extended; the plugin wires LENGTH, SCALE, KEY, ORDER, REPEAT ENDS, CHANCE..., ROOT, ROTATE, TRANSPOSE and RESET; step editing by scale degree with note names; page chips follow LENGTH; the milestone 3 test aids are removed. No new controls; all 46 ids are unchanged. See CHANGELOG.

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
