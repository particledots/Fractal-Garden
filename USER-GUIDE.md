# Fractal-Garden: user guide

For version 1.3.0. Written from the README, CHANGELOG, control list and manual-gaps ledger, nothing else.

## 1. What it is

Fractal-Garden is a **MIDI effect** for Logic Pro: it sits in a track's MIDI FX slot and plays a 64-step pattern into that track's instrument (it makes no sound of its own). The pattern can grow **branches** (changed copies of itself) and **mutate** as it plays, a bit like the Qu-Bit Bloom v2 sequencer that inspired it. It is not affiliated with Qu-Bit.

Words used below: a **step** is one slot in the pattern (note, length, and so on). The **trunk** is your own pattern. An **instance** is one copy of the plugin on one track. One instance plays one channel of 64 steps; for three voices, use three instances on three tracks.

**How tested:** version 1.3.0 (the ALL GATES 100% and CLEAR RATCHETS buttons and the new starting pattern) was tested by Bryn in Logic on 9 November 2026 and worked. Bryn tested 1.1.0 and 1.2.0 in Logic on 8 November 2026 and reported that everything worked as described (1.2.0: "truly incredible and incredibly musical"). Earlier versions were tested item by item on 7 November 2026. The VST3 version is built every time but is only checked occasionally (in Reaper), not in Logic.

## 2. What to touch first

Set up: make a software instrument track, pick a sound, then in its **MIDI FX** slot choose **Audio Units > particledots > Fractal-Garden** (Logic may list it as "MIDI Controlled / particledots"). Press play in Logic. It only plays while Logic is playing, and it stays locked to Logic's bars and tempo.

**A new instance starts plain (from 1.3.0):** every step is the same note (C3) with its gate at 100 % and no ratchets, so at first you hear one long held note. Set RESEED's SCOPE to **ALL 64** and press **RESEED ALL** (or RESEED: NOTE for new notes only): that is your starting point. Projects you saved before 1.3.0 open with the pattern they had.

Try these in this order, while it plays:

1. **SCALE** and **KEY**: change the pitch of the whole pattern at once (28 scales). Your steps are not altered, so you can switch back and forth freely.
2. **ORDER**: the pattern plays its steps Forward, Reverse, Pendulum, Random and so on (11 orders).
3. **RATE**, **RATIO** and **LENGTH**: how fast a step is, a speed-up or slow-down on top of that, and how many steps long the loop is.
4. Click a step's **ON / OFF** and drag inside a step's box to change its note. You hear it the next time that step comes round.
5. **BRANCH**: turn it up and the loop becomes trunk, then changed copies, then trunk again. Then turn **PATH** to pick a different set of changes.
6. **MUTATE**: turn it up slowly and the notes start to wander.
7. **RESEED** and **UNMUTATE** when you want fresh dice or want to go back (section 5).

## 3. The step editor

The big area in the middle. Tabs along the top choose what you are editing for every step: **Note, Gate, Slew, Ratchet, Mod**. Page chips pick which eight steps you see (pages past LENGTH are greyed out), and an overview strip shows all 64.

- **Note:** drag in a step's box to move the note up or down the current scale. The read-out shows the note name. Double-click puts a step back to its default. With **Pitch snap** on SEMITONES or FREE (Settings) the Note tab works differently: see section 6.
- **Gate** (how long the note sounds, 5 to 100 % of the step): at 100 % the step is **tied** to the next one, so a mono synth plays smoothly without re-hitting.
- **Slew:** how much a synth should glide into the step. It is sent as MIDI **CC 84** (a CC is a MIDI control message). You may need to map CC 84 to your synth's glide time in Logic.
- **Ratchet:** splits a step into x1, x2, x3, x4, x6, x8, x16 or x32 quick repeats.
- **Mod:** sends a moving value as **CC 1**, in four modes (Shapes, Velocity, Envelope, Smooth). In Shapes, click the top of a step's box for the next shape and the caption strip at the bottom for the next speed; shift or right-click goes back.

Edits to Gate, Slew and Ratchet are made in the same step boxes; the project notes only spell out the exact mouse gestures for Note and Mod.

The teal highlight shows the step playing. While a branch plays, the highlight hides and the header says BRANCH n.

## 4. The knobs

**KNOB MODE** chooses which set of three knobs is on show: Default, Micro Mutate or Performance. All nine knobs work all the time; the mode only changes which ones you see.

**Default: Branch, Path, Mutate**
- **BRANCH** (0 to 7): how many changed copies play after the trunk. Each is made from the one before by one of five changes, shown as letters in the TREE: **R** reverse, **I** inverse (mirror the notes), **T** transpose, **M** mutate, **?** random.
- **PATH:** with 7 branches there are up to 128 paths. Each path is a different set of changes, and the same path always gives the same branches.
- **MUTATE:** changes steps as they play, and the changes are kept. First half of the knob: only notes change, more often as you turn up. Second half: gates change too (new lengths, or steps switched on and off).

**Micro Mutate: Micro Ratchet, Micro Slew, Micro Mod.** Each knob is the chance, from none at the left to every step at the right, that a step gets a new ratchet, slew amount or Mod value. Notes never change here.

**Performance: Ratchet FX, Slew FX, Mutate FX.** These change what you hear **without touching the stored steps**. Turn a knob back to 0 and the pattern is exactly as it was. Each knob has two halves: the first is random (a chance that rises as you turn up), the second is a fixed amount on every step. In Mutate FX, the second half adds **ornaments** (little flourishes: short runs, arpeggios, and at the very top, trills). The read-out under each knob names what it is doing.

## 5. Reseed and Unmutate

- **RESEED** buttons throw new random values: **ALL, NOTE, GATE, SLEW, RATCHET, MOD**. They change the steps for good. **SCOPE** chooses the eight steps of the page you see (PAGE) or all 64. **RESEED: BRANCHES** gives the branches a new set of random choices and leaves the trunk alone.
- **ALL GATES 100%** and **CLEAR RATCHETS** (on the RESEED card's title line, from 1.3.0): one click sets every gate length to 100 %, or every ratchet to x1 (no ratchet). They use the same SCOPE as RESEED. ALL GATES 100% does not switch OFF steps on. At 100 % each step is joined to the next, so a mono synth glides from note to note without re-hitting, and the same note twice in a row is one long note.
- **UNMUTATE** puts the steps and LENGTH back to the state of your last manual edit (editing a step, or changing LENGTH by hand). It undoes Mutate, Micro Mutate, Rotate, Transpose, Resize, Reseed, ALL GATES 100% and CLEAR RATCHETS. It does not undo RESEED: BRANCHES.
- Editing a step sets a new point to go back to, and earlier ones can no longer be undone.

Related buttons: **ROTATE** (moves the steps left or right inside LENGTH), **TRANSPOSE** (moves every note up or down one scale step), **RESIZE** (SHRINK halves LENGTH, EXPAND doubles it) and **RESET** (the next step played is step 1; it changes no steps). Rotate, Transpose and Resize change the stored steps; UNMUTATE takes them back.

## 6. Pattern bank and settings

**Pattern bank.** PATTERN picks one of 16 slots. **SAVE** puts the current steps and LENGTH into the slot (replacing what was there, without asking). **LOAD** brings them back; on an empty slot it does nothing. Slots hold steps and LENGTH only; Scale, Key, Order, Rate, Ratio and the knobs stay as you set them. The slots are saved inside the Logic project, and each instance has its own bank. Loading counts as an edit, so UNMUTATE returns to the loaded pattern.

**SETTINGS...** (top of the window; DONE closes it):
- **MIDI channel** (1 to 16, default 1), **Slew CC** (default 84), **Mod CC** (default 1), **Base note** (default 48, which Logic calls C2).
- **Range (octaves)** (1 to 5, default 2): how many octaves above the Base note RESEED, the random branches and Mutate may use. It stops new notes going too high. It does not change notes you set yourself or a pattern you already have; a note that is already above the range stays until something mutates it. 5 is the old full range.
- **Pitch snap:** STEP (default) plays the scale notes, as always. SEMITONES lets each step sit on any whole semitone, ignoring the scale. FREE also allows cents between semitones; the cents are sent as pitch bend. **Bend range** (1 to 24 semitones, default 2) must match the pitch-bend range of your instrument or the cents come out wrong. In the Note tab: SEMITONES, click or drag to a height; FREE, drag moves the pitch from where it is (10 cents a pixel, hold Shift for 1 cent); the mouse wheel moves a semitone (SEMITONES) or 10 cents (FREE, Shift 1 cent); double-click puts the step back to its default note. The bend returns to centre when Logic stops.
- **Resize method:** Spread (default, leaves gaps), Stretch (repeats each step), Clone (repeats the whole pattern).
- **Reseed filter:** which of Note, Gate, Slew, Ratchet, Mod the RESEED: ALL button changes.
- **Branch filter:** which kinds of edit make the branches rebuild. Switch one off and branches keep their old version when you make that kind of edit.

With Range 5 and Pitch snap STEP, everything behaves as in 1.1.0 (the default Range is 2, which only limits new random notes).

## 7. SHARE and GRAB (the canon patch)

For several instances playing the same pattern, offset in time (a canon):

1. On the instance with the pattern you like, press **SHARE** (on the PATTERN card, next to LOAD and SAVE). It writes the 64 steps and LENGTH to one file, `shared-pattern.fgpattern`, in a `Fractal-Garden` folder inside your Music folder (Logic may keep that Music folder inside its own container folder; all instances in Logic still find it).
2. On each other instance, press **GRAB**. It takes the pattern from the file. Scale, Key, Order, Rate, Ratio and knobs are **not** copied, so each instance keeps its own sound and settings.
3. Press **ROTATE** on each grabbed instance by a different amount to offset them.

Good to know:
- There is only one shared file. The next SHARE replaces it without asking, from any instance. GRAB never changes the file.
- It is not a live link. Changing a step on the sharing instance changes nothing elsewhere until you SHARE and GRAB again.
- GRAB counts as an edit, like LOAD, so UNMUTATE returns to the grabbed pattern.
- The line at the bottom right of the window says "SHARED FILE: n STEPS" or "NONE", or why the last SHARE or GRAB failed.
- Opening a project never presses SHARE or GRAB for you.

## 8. If something sounds wrong

- **A new instance plays one long note:** that is the plain starting pattern (from 1.3.0). Press RESEED ALL with SCOPE on ALL 64.
- **Nothing plays:** Logic must be playing. The plugin makes no notes while Logic is stopped. Check the header says LOGIC PLAYING and that the instrument is on the same MIDI channel as the Settings page (default 1).
- **Notes are all wrong or very low or high:** check SCALE, KEY, ROOT and Base note in Settings (default 48). Too high: lower Range. Slightly out of tune with FREE: check Bend range matches your instrument.
- **No glide or no effect from Slew or Mod:** the plugin sends CC 84 and CC 1, but your instrument has to listen to them. Map them in the instrument (Smart Controls or MIDI learn), or change the CC numbers in Settings.
- **The pattern keeps changing and you did not ask it to:** MUTATE, Micro Mutate or Performance knobs are above 0. Set them to 0 and press UNMUTATE to go back to your last edit.
- **Branches do not follow my edit:** a Branch filter switch (Settings) may be off for that kind of edit, or the edit was a Mutate or Reseed, which do not rebuild branches. RESEED: BRANCHES or any step edit makes them again.
- **Ratcheted steps ignore Gate length:** by design; ratchets use half a slot per note.
- **GRAB did nothing, or says NOTHING HAS BEEN SHARED YET or NOT READABLE:** press SHARE on another instance first. If instance B stays at "SHARED FILE: NONE" after SHARE on A, tell Bryn: the folder is not being shared and that is a bug.
- **Stuck note:** stopping, jumping or cycling in Logic ends the note at once. A stuck note is a bug to report.
- **Two instances out of step:** they follow Logic's bars, so they should stay locked. If they drift, report it.

Reporting: say which Mac and Logic version, which control, and what you heard.
