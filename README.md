# Line — Dexter U.S.

**Dexter U.S. line** is a public MIT-licensed fork of [pd3v/line](https://github.com/pd3v/line).
It keeps the live-coding command-line MIDI sequencer and adds a Windows build and a
VST3 instrument. The VST3 faceplate reads **Dexter U.S. line**.

The VST3 editor uses VSTGUI's `CTextEdit`. VSTGUI is licensed under the 3-clause BSD
license, a permissive license compatible with an MIT-licensed project. Its notice is
included in the VST3 SDK checkout used to build the plug-in.

## Windows command-line build

Install CMake and Visual Studio 2022 with the C++ desktop workload, then run from
the repository root in a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\line.exe
```

The Windows command-line version uses the console for input and does not require
GNU Readline. MIDI output is provided through RtMidi's Windows backend. At the
`line>` prompt, `em` exports the current phrase to `line.mid`; `emmysong.mid`
exports it to `mysong.mid` for import into a DAW.

## VST3 instrument

Build the VST3 plug-in and command-line program together:

```powershell
cmake -S . -B build-vst3 -A x64 -DLINE_BUILD_VST3=ON -DBUILD_TESTING=ON
cmake --build build-vst3 --config Release --parallel
ctest --test-dir build-vst3 -C Release --output-on-failure
```

The plug-in is named **Line**, lists **Dexter U.S.** as its manufacturer, provides
stereo audio and MIDI outputs, and follows the DAW transport and tempo. The editor
accepts up to 255 characters in this supported pattern subset:

- MIDI note numbers or note names such as `45`, `c3`, `as3`, and `eb4`
- Chords such as `(c3 e3 g3)` and rests using `-`
- Eighth-note steps with a leading dot, dotted-quarter steps with a trailing dot
- Note velocity using `~.7` and tempo using `bpm120`

The editor's **Load** and **Save** buttons use the native file picker and read or
write a single pattern line in a plain-text `.line` file.

The VST3 pattern editor is intentionally a focused live-coding input, not a port of
every command in the original CLI's Lua parser. The Windows CI workflow runs the
parser tests and validates the built plug-in with pluginval. Each successful run
also uploads a `dexter-us-line-windows` artifact containing `line.exe` and
`Line.vst3`; download it from the workflow run's **Artifacts** section.

---

Original project: **A tiny command-line midi sequencer for live coding.**
(**Ableton Link** compatible version)

+ Sends MIDI messages to a chosen MIDI channel. Default is 1.

+ 1 instrument or 1 CC

To some videos live coding with it [go here.](https://x.com/search?q=%23linelivecoding&src=hashtag_click&f=live)<span style ="font-size:8pt">(on X)</span>

<img src="https://github.com/pd3v/line/blob/develop/line 0.4.3 (7 running).png" alt="7 instances of **line** running simultaneously. 1 synth. 6 cc." style="width:60%;height:60%"/>

7 instances of **line** running simultaneously. 1 for notes, 6 cc. (Using tmux to split macOS terminal into 7 terminals)

---
### MacOS ready-to-run builds

#### Up-to-date Mac/Intel and Linux builds

+ <a href="https://pd3v.github.io/downloads" target="_blank">Download them from https://pd3v.github.io/downloads</a>
+ Go to the **build** folder, type `./line` and press Enter

<!--#### There's a Mac/M1/M2 build ready

+ Go to [Actions page](https://github.com/pd3v/line/actions) > latest workflow run > section *Artifacts*, download and unzip the file
+ Go to the **build** folder, type `./line` and press Enter
-->

#### There's a Mac/Intel build ready

+ Just unzip **line_0.5.25_ablt_lnk_Mac_Intel_build.zip** file
+ Go to the **build** folder, type `./line` and press Enter

### What you need to build *line* yourself

+ **readline** library (should be already on your system, if not, you have to install it)
+ **jack** library (**only** for Linux. It is not installed by default. If the computer has no audio/music production sofware succefully used then a *jack* installation is needed)
+ **CMake**

<!-- ### Get *line* binaries -->

<!-- [macos binaries](https://github.com/pd3v/line/actions/runs/1802392423) -->

### To clone this repo with *rtmidi* and *link* submodules included

`git clone --recursive https://github.com/pd3v/line.git`

#### Build and run *line* yourself

(CMake is necessary)

+ Go to *line*'s folder

In command-line, type each of the following + Enter:

1. `./build.sh` 

2. `cd build`

3. `./line`

**Now run your favorite synth, sampler, DAW, or other MIDI receiver!**

## Manual

With *line* running, type:


**Send a MIDI message:**

4 1/4 midi notes

`45 46 47 48` or `a3 as3 b3 c4`

1/4 1/8 1/8 1/4 1/4 notes

`45 .34 35. 48 49` or `a3 .as2 b2. c4 cs4`

'-' for a silent note

`36 .37 38. 41 .- 46.` 

C Major chord + C Major arpeggio. All 1/4

`(c3 e3 g3) g5 e6 c2`

d note with 0.5 of amplitude

`d3~.5`

C Minor chord with 0.7 of amplitude

`(c4 eb4 g4)~.7`

*Note: amplitude is 1.0, by default*

**Set a MIDI channel:**  

channel 2

`ch2`

**Set bpm:**  

120 bpm

`bpm120`

**Set a range of values (MIDI default):**

minimum is 100

`mi100`

maximum is 1000

`ma1000`

**Set overall relative amplitude:**

50% of previous overall amplitude

`am50`

**Concatenate phrase n times:**

concat phrase 4x 

`*4`

**Set phrase duration n times:**

Increase phrase duration 8x 

`/8`

**Mute and unmute:**

`m` and `um`

**Reverse:**  

`r`

**Scramble:**  

scrambles notes within each rhythmic part and rhythmic structure

`s`

**Extra scramble:**  

scrambles notes across the phrases and rhythmic structure

`x`

**Amplitude Scramble and Extra Scramble:**

`sa` and `xa`

**Save phrase to a queue:**

place it on top of the queue; position 0

`sp`

replace phrase in position 3

`sp3`

**Load phrase:**  

load postion 0 phrase; it will play next

`lp0` or `:0`

**List saved phrases**

`l`

**Save queued phrases to *.line* (= txt) file**

Will assume prompt text as file name

`sf`

Save file with *mysynth.line* name

`sfmysynth`

*Note: Will also save line instance parameters (prompt, channel, notes/cc, range of values)*

**Export the current phrase as a standard MIDI file**

`em` exports the current phrase once to `line.mid`; `emmysong.mid` exports it once
to `mysong.mid`. Add a duration in seconds to repeat the phrase and cut the final
pass at that duration: `em 15` writes a 15-second `line.mid`, and
`em mysong.mid 15` writes a 15-second `mysong.mid`. The file includes the current
tempo and MIDI channel and can be imported into a DAW.

**Load *.line* file to line**

Load sampler.line

`lfsampler`

*Note: Will apply to line instance saved parameters*

**Switch between notes and cc modes:**  

notes mode

`n` 

cc mode on channel 2

`cc2`

**Set cc sync:**  

In-sync or Out-sync

`i` or `o`

**Relabel prompt:** 

from default **line>** to **_my_synth>**, type:

`lbmy_synth`

**Exit:**

`ex`  

**Display commands menu/extended menu:**

`ms` or `me`

**Set latency:** (ableton link's compatible *line* version only)

subtract 10 ms 

`lt10`

