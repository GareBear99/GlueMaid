GlueMaid (JUCE) — Automatic Bus Glue / Smart Compressor

Controls:
- Glue: amount (drives target GR + ratio bias)
- Punch: preserves transients (attack bias slower + SC HP higher recommended)
- Soft: gentler knee + lower ratio
- SC HP: sidechain highpass (Hz) to avoid low-end pumping
- Mix: parallel dry/wet
- Output: output gain
- Clip: safety soft clip on output

RT-safe:
- No allocations in processBlock.
- Sidechain HP coefficients update only on parameter changes.
- No mutexes, no file IO, no FFT.

Build (CMake):
cmake -B build -S . -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release

Or:
cmake -B build -S . -DJUCE_PATH=../JUCE
cmake --build build --config Release

Formats: AU + VST3 + Standalone
