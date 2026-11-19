# Famous Beats Mic Model

**Makes a Neumann TLM 103 sound like a Sony C-800G tube condenser.**

Put it on your vocal track in Ableton (or any DAW) and your TLM 103
takes on the C-800G's character: the broad presence edge tamed, warm
low mids, smooth open air on top, and a gentle 6AU6-style tube glow.

## What it does

A difference EQ built from the two microphones' **published response
curves**, plus a tube saturation stage:

| Stage | What it does |
|---|---|
| **Presence tamer** | Peaking cut −2.5 dB at 5.5 kHz — takes the edge off the TLM 103's broad +4 dB presence shelf (Neumann: "flat up to about 5 kHz, and above that, a wide flat 4 dB presence boost") |
| **Warmth** | Peaking boost at 250 Hz, 0 to +3 dB (Warmth knob) — the lower-mid body the C-800G's tube and transformer add |
| **Air** | High shelf at 12 kHz, 0 to +4 dB (Air knob) — the C-800G's open, airy top end |
| **HF rolloff** | Gentle lowpass at 16 kHz — the C-800G's response ends at 18 kHz, so the extreme top falls away smoothly |
| **Tube** | Asymmetric tanh saturator emulating the 6AU6 tube's even-harmonic warmth (Tube Drive knob) |

The editor shows the live model curve so you can see exactly what
it's doing to your mic.

## Controls

- **Model Amount** — dry/wet blend, 0–100% (default 100%). At 0% the
  plugin passes your mic through untouched.
- **Warmth** — low-mid body, 0–100% (default 50% = +1.5 dB at 250 Hz).
- **Air** — top-end openness, 0–100% (default 50% = +2 dB shelf at
  12 kHz).
- **Tube Drive** — 6AU6-style saturation, 0–100% (default 30%).
- **Output** — output gain, −12 to +12 dB.
- **Bypass** — passes the input through untouched.

Start with the defaults, then turn Warmth and Air to taste — every
voice, distance, and room is a little different.

## Install (Mac)

1. Download `FamousBeatsMicModel-macOS.zip` and unzip it.
2. Copy `Famous Beats Mic Model.vst3` to
   `~/Library/Audio/Plug-Ins/VST3/`
   (Finder → Go → Go to Folder, paste that path).
3. Copy `Famous Beats Mic Model.component` to
   `~/Library/Audio/Plug-Ins/Components/`
4. If your Mac blocks the plugin, open Terminal and run:
   ```
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Famous Beats Mic Model.vst3"
   xattr -cr ~/Library/Audio/Plug-Ins/Components/"Famous Beats Mic Model.component"
   ```
5. Restart your DAW and rescan plug-ins if it doesn't show up.

In Ableton Live 11 it appears under **Audio Effects** (it's an audio
effect, not an instrument) — drop it on your vocal track, first in the
chain, before your EQ/compressor.

Requires macOS 10.15 (Catalina) or newer. Works on Intel and Apple
Silicon Macs (universal binary).

## Build it yourself (Mac, no Xcode project needed)

Push this project to GitHub — the workflow in
`.github/workflows/build-macos.yml` builds the universal `.vst3` and
`.component` on a Mac in the cloud and uploads them as the
`FamousBeatsMicModel-macOS` artifact. No Xcode needed on your own Mac.

To build locally on a Mac with CMake installed:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build --config Release --parallel
```

## Tests (Linux)

```
cmake -B build -DFBS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel --target mic_model_test
./build/mic_model_test
```

The harness feeds generated audio through the plugin and checks:
silence stays silent, no clipping or NaN, the sine-sweep gains follow
the intended difference curve (warmth up, presence dip, air recovery,
HF rolloff), the Warmth/Air knobs move their bands, the tube stage
adds 2nd-harmonic warmth, and Bypass / Amount 0 pass audio untouched.

## Honest limits — please read

- This is a **character model** built from the manufacturers'
  **published response curves**, not a measurement of your actual
  TLM 103 in your room. Two TLM 103s, two voices, two rooms — all a
  little different.
- It is **not a clone of Sony's microphone** and it does not turn a
  TLM 103 into a C-800G. It moves your mic's tone in the C-800G's
  direction: tamer presence, warmer low mids, smoother air, gentler
  top end, and a touch of tube glow.
- How close it sounds depends on your voice, how close you sing to
  the mic, and your room. If it sounds too dark, turn Air up; if it
  sounds too thick, turn Warmth down; if it sounds too processed,
  pull Model Amount back to 70–80%.

## Project layout

```
FamousBeatsMicModel/
  CMakeLists.txt            CMake + JUCE 8.0.8 (FetchContent)
  Source/
    PluginProcessor.h/.cpp  Difference EQ + tube stage, parameters
    PluginEditor.h/.cpp     Black-and-gold GUI, live model curve
  test/
    mic_model_test.cpp      Linux test harness (see Tests above)
  .github/workflows/
    build-macos.yml         GitHub Actions macOS build (macos-15)
  README.md
```

Famous Beats Studio — black and gold.
