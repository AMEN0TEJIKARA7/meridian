# Meridian EQ — v0.1 (VST3, JUCE)

Dynamic-band parametric EQ in the Pro-Q workflow: add bands on the curve, drag them, edit in a panel that floats under the node.

## What's in v1

- Up to **24 bands**, added and removed freely
- **9 shapes:** Bell, Low Shelf, Low Cut, High Shelf, High Cut, Notch, Band Pass, Tilt Shelf, Flat Tilt
- **Slopes** 6 / 12 / 18 / 24 / 30 / 36 / 48 / 72 / 96 dB/oct + Brickwall (cuts, shelves, tilt shelf, band pass)
- **Per-band stereo placement:** Stereo, Left, Right, Mid, Side
- Resonant cuts and shelves (Q above 0.707 adds a bump at the corner)
- Pre / post **spectrum analyzer** (8192-point FFT, adjustable speed and tilt)
- **Auto Gain**, **Gain Scale** (0–200 %), output gain, output balance, peak meters
- **Solo** a band to hear the region it affects
- **A/B** comparison, **undo/redo**, presets (saved as `.mrdn` files)
- Display range ±3 / 6 / 12 / 30 dB, resizable window
- Zero latency

## Using it

| Action | How |
|---|---|
| Add a band | Double-click empty space (near the left edge → Low Cut, right edge → High Cut, else Bell) |
| Move a band | Drag the node (Shift = fine) |
| Change Q | Scroll over the node, drag the side handles, or (on cuts/notch/band pass) drag vertically |
| Bypass a band | Double-click the node, or the power button in the panel |
| Delete a band | Alt-click the node, Delete key, or × in the panel |
| Band menu | Right-click a node |
| Undo / redo | Ctrl+Z / Ctrl+Shift+Z (or Ctrl+Y) |

When any band is set to Left/Right/Mid/Side, a second dashed curve shows the Right/Side response; the solid curve is Left/Mid.

## Build on Windows

1. Install **Visual Studio 2022 Community** with "Desktop development with C++", plus **Git**.
2. In "Developer PowerShell for VS 2022", inside this folder:
   ```
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release --target Meridian_VST3
   ```
   The first configure downloads JUCE (a few minutes).
3. Copy `build\Meridian_artefacts\Release\VST3\Meridian.vst3` (a folder) into `C:\Program Files\Common Files\VST3\`.
4. Ableton: Settings → Plug-Ins → "Use VST3 Plug-In System Folders" on → Rescan. It's under Plug-Ins → Seraph.

Presets live in `%APPDATA%\Seraph\Meridian\Presets`.

## Tests

```
cmake -B build -DMERIDIAN_BUILD_TESTS=ON
cmake --build build --config Release --target MeridianTest
build\MeridianTest_artefacts\Release\MeridianTest.exe screenshot.png
```
Measures the processed audio against the filter maths (so the drawn curve is what you hear), checks M/S and L/R routing, Gain Scale, Auto Gain, 24-band stability under automation, CPU, and renders a screenshot.

## Code map

```
Source/FilterDesign.*     all filter maths (shared by audio + display)
Source/Params.h           parameter IDs and ranges
Source/PluginProcessor.*  audio engine, presets, A/B
Source/EQDisplay.*        curve, nodes, analyzer, mouse handling
Source/BandPanel.*        floating band editor
Source/PluginEditor.*     header + footer, window layout
Source/LookAndFeel.*      knobs, buttons, menus
Source/Theme.h            colours and bundled fonts
```

## Roadmap

- **v2:** dynamic EQ per band; Natural Phase and Linear Phase modes; analog-matched filter design (removes the bilinear "cramping" of bells and cuts above ~10 kHz)
- **v3:** EQ Match, sidechain analyzer with collision display, spectral dynamics

## Fonts

IBM Plex Mono (OFL), Source Serif 4 (OFL), Cormorant Garamond (OFL) are bundled; licences in `Resources/Fonts`.
