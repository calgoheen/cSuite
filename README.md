# cSuite

<p align="center"><img src="cSuite.png"></p>

## Overview

cSuite is an effect rack audio plugin available in VST3, AU, and CLAP formats for Mac and Windows, built using C++ and JUCE.

It is currently in alpha and is subject to significant changes before the release, including changes that break saved presets and projects.

### Features

- Add as many effects and modulators as you need
- Drag-and-drop audio rate modulation
- Save and load presets
- Undo and redo support
- Resizable interface

### Effects

- Chorus
- Compressor
- Crush
- Delay
- Distortion
- Filter
- Flanger
- Gate
- Phaser
- Reverb
- Tape Stop
- Utility

### Modulators

- LFO
- Random
- Macro
- Envelope Follower

## Build Instructions

### Prerequisites

- [CMake](https://cmake.org/)
- [Ninja](https://ninja-build.org/)

### Build

```
# Clone the repo
git clone --recurse-submodules https://github.com/calgoheen/cSuite.git
cd cSuite

# Configure and build
cmake --preset release
cmake --build --preset release
```

## Contributing

Not currently accepting pull requests for this project. Feel free to report bugs and/or feature requests.

## External Dependencies

- [JUCE](https://github.com/juce-framework/JUCE)
- DSP modules from [chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils)
- CLAP plugin format is built with [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)
