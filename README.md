# Fractal-Garden

An original AU / VST3 **MIDI effect** for Logic Pro, inspired by the feel of a Eurorack 3-channel, 64-step fractal CV / gate
sequencer (the Qu-Bit Electronix Bloom v2): 64 steps, branches that grow from a trunk, and knobs that mutate the pattern. All code is
our own; it is not a port or copy of anything, and it is not affiliated with Qu-Bit Electronix.

**Status: version 0.2.0 (milestone 2: the first build with the styled panel). 0.2.0 was tested in Logic on both test Macs (M4 Mac mini, Logic 11.2.2; Intel Mac, macOS 12.7.8, Logic 10.7.8) on 7 November 2026: every check passed and the window matched the approved pictures, so milestone 2 is finished.**
**Nothing plays yet.** The window draws, the controls move and are saved with the Logic project, and the step editor shows (and lets you edit) a sample pattern that is also saved. MIDI that arrives from Logic passes through unchanged; the plugin adds no notes of its own. The sequencer starts in milestone 3.

## First five minutes (after installing, see *Getting the plugin*)

1. In Logic, make a **software instrument** track. In its **MIDI FX** slot choose **Audio Units > particledots > Fractal-Garden** (Logic may list it as "MIDI Controlled / particledots").
2. The window opens at about 90 % size; drag a corner to resize it (the shape stays 3:2).
3. Move the controls, click the five tabs (NOTE, GATE LENGTH, SLEW, RATCHET, MOD), the page numbers 1 to 8, and a step's ON / OFF button. Drag inside a step's box to change its value.
4. Press **KNOB MODE: MICRO MUTATE** and **PERFORMANCE**: the three knobs change their names and colour.
5. Set **ORDER** to **Pendulum** (REPEAT ENDS lights up) and to **Chance mix** (a CHANCE... button appears and opens the mixing pop-over).
6. Press **PLAIN SLIDERS** for a window with one ordinary slider or button for each of the 46 controls; **STYLED PANEL** goes back.
7. Save the project, close it, reopen it: every control, the step data, the tab, the page and the window you were using come back.

## The window

Top: title, a header showing Logic's play state and tempo (STEP / GATE lights stay dim until milestone 3), and the PLAIN SLIDERS button.
The step editor (tabs, Mod Out chips, page chips, an overview of all 64 steps, eight step columns). Three knob cards (Branch, Path, Mutate; or Micro Ratchet, Micro Slew, Micro Mod; or Ratchet FX, Slew FX, Mutate FX). Knob Mode, Reseed, Tree. Then Length, Scale, Key, Order, Rate, Ratio, and Pattern, Root, Transpose, Rotate, Resize, Reset, Unmutate.

## Things worth knowing

- **46 controls**, ids frozen from this version; new controls will be appended. Logic's own plain view and the automation list show all 46.
- The 17 buttons (Reseed, Reset, Unmutate, Rotate, Resize, Transpose, Pattern Load / Save and so on) **do nothing yet**: they glow briefly and pop back off. They start working in milestones 4 and 5.
- The sample step pattern is only a demonstration. The ranges of Gate, Slew, Ratchet and Mod, the eight Mod captions, and the wording under each knob are **first guesses**; where the Bloom v2 manual is vague (for example the exact Performance zones) we approximate and will tune it with you.
- Window and tab choices are saved in the project but are not controls (no automation).

## What has been checked (without Logic)

- Engine self-test: 834 checks (control table, menus, note names, step-data save / load and rejection of damaged data).
- Window test on a Linux computer: 189 checks (all 46 controls exist once in each window, switching windows, saving and reloading controls / steps / tab / page / window, momentary buttons popping back, Knob Mode titles and colours, the Order slot, the Chance pop-over, no MIDI added). Pictures drawn from the real window were compared with pictures A, B and C.
- GitHub Actions: engine test, macOS build (universal Apple Silicon + Intel), auval, pluginval level 5 (see CHANGELOG for the result of the run).
- **Tested in Logic on 7 November 2026, both Macs (M4 Mac mini, Logic 11.2.2; Intel Mac, macOS 12.7.8, Logic 10.7.8):** installs, appears in the MIDI FX slot as Fractal-Garden, window matches pictures A, B and C, controls / tabs / pages / step edits / Knob Mode / Order slot / Chance pop-over / Plain Sliders work, 46 parameters listed, everything is restored after saving and reopening a project.

## What changed in 0.2.0

First build: AU + VST3 MIDI effect shell, all 46 controls, the styled panel from pictures A, B and C, the plain-slider window, sample step data saved in the state, the installer and the automatic build.

## Folders

- `engine/` plain C++ (control table, step data), with its self-test; no plugin code.
- `plugin/` the JUCE wrapper, the styled panel, the plain window, fonts, and `preview/` (the offline window checks).
- `.github/workflows/` the automatic build. `tools/install/` the installer script.

## Getting the plugin

Open the GitHub **Actions** tab of this repository, open the latest green run and download the artifact **FractalGarden-macOS** (a zip). Unzip it and double-click **Install-Unofficial-Plugin.command** (INSTALL.txt explains what to do if macOS asks you to confirm). Restart Logic.

## Notes

Original work by particledots; inspired by, and not affiliated with, Qu-Bit Electronix. JUCE is used under its licence; the fonts (Space Grotesk, IBM Plex Mono) under the SIL Open Font Licence (copies in `plugin/Fonts`).
