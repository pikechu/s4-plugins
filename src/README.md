# Source directory

This directory will contain the Win32 C++ implementation of the ASI plugin.

Planned modules:

- `dllmain.cpp` — minimal DLL entry point and delayed initialization;
- `Plugin.cpp` — plugin lifecycle and dependency detection;
- `VictoryDetector.cpp` — local-player victory detection;
- `MapClassifier.cpp` — multiplayer and random-map exclusion;
- `CompletionDatabase.cpp` — JSON persistence and backups;
- `MenuInspector.cpp` — campaign and scenario entry discovery;
- `MarkerRenderer.cpp` — in-game completion marker rendering;
- `Hotkeys.cpp` — manual completion controls.

No production source code has been implemented yet.
