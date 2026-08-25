# DelayMXA

A regenerative stereo delay. Success: a stranger dials a musical echo in seconds and understands the ping-pong routing at a glance.

![DelayMXA — the sheet](Captures/DelayMXA.png)

Audio plugin (AU / VST3 / Standalone) built with [JUCE](https://juce.com). Part of the [MXA plugin suite](https://mxaudio.mescalina.fr/). macOS 11+ and Windows — Windows builds (VST3 + Standalone) are available in [Releases](https://github.com/uprod/DelayMXA/releases).

## Build

```sh
git clone --recurse-submodules https://github.com/uprod/DelayMXA.git
cd DelayMXA
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If you already have the MXA suite checked out with a shared `../JUCE` folder, the submodule is optional — the build falls back to the sibling folder automatically.
