# De-Saturator

A free audio plugin that thins a sound: the opposite of a saturator. It takes out harmonic density, body and squashed peaks. An [Aria Ashrafi](https://ariaashrafi.com) plugin.

Status: in development. Nothing is released yet.

- Formats: VST3 on Windows and macOS, AU on macOS.
- Two engines: **HQ** (multi-resolution spectral, with latency) and **Live** (zero latency).
- Two ranges: **Clean** (transparent) and **Character** (goes further).
- A thinning curve drawn on the analyser sets how much thinning happens at each frequency.

## Layout

- `src/dsp/`: the engines, plain C++20 with no JUCE.
- `src/plugin/`: the JUCE plugin around them.
- `src/ui/`: the window, drawn from `design/spec/`.
- `design/`: the design kit the window is built from. It is changed by the design system, not here.
- `tools/`, `tests/`: the offline render rig and the test suite.

## Building

Needs CMake 3.25+ and a C++20 compiler (Visual Studio 2022 Build Tools on Windows, Xcode on macOS). JUCE is fetched by CMake.

```
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Licence

GPL-3.0. See `LICENSE`. The fonts in `design/fonts/` are under the SIL Open Font License 1.1.
