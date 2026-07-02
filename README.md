# fastbreak: Basketball Manager Simulation Engine

This project implements a high-fidelity, possession-based basketball manager simulation engine. It mirrors the decoupled C++ engine + Unity subprocess bridge architecture from [3up3down](file:///Users/atharvarao/Documents/3up3down).

---

## 🛠️ Architecture & Tech Stack

1. **C++ Core Engine**: Models players, rosters, clock, shot-clocks, play-by-play actions, rebounds, fouls, free throws, blocks, and manager tactical adjustments.
2. **Bazel Build System**: Uses Bazel 9 modules (`Bzlmod`) to compile C++ library and binaries.
3. **Subprocess Bridge**: Communicates state transitions using line-based standard I/O in JSON format (`fastbreak_bridge`).
4. **Unity Integration**: C# client scripts to start the bridge, issue manager commands, and read reactive game snapshots.

---

## 💻 How to Build & Run

Ensure you have **Bazel** installed (via Bazelisk wrapper).

### 1) Build the binaries
Run from project root:
```bash
bazelisk build //...
```
This produces the following binaries:
- Smoke Test: `bazel-bin/fastbreak_smoke`
- Subprocess Bridge: `bazel-bin/fastbreak_bridge`

### 2) Run the Smoke Test
To verify the simulation logic (which runs a full 4-quarter basketball simulation from jump ball to buzzer, compiling stats):
```bash
bazelisk run //:fastbreak_smoke
```

---

## 🎮 Unity Frontend Setup

1. Copy `unity/Assets/Scripts/*.cs` into your Unity project's `Assets/Scripts/`.
2. In your active scene, create an empty GameObject (e.g. `GameBridge`).
3. Attach the components:
   - `SimulationBridgeClient`
   - `SimulationDebugController`
4. Set the `SimulationBridgeClient.bridgeExecutablePath` to point to your compiled bridge binary:
   - macOS Example: `/Users/atharvarao/Documents/fastbreak/bazel-bin/fastbreak_bridge`
   - Windows Example: `C:\path\to\fastbreak\bazel-bin\fastbreak_bridge.exe`
5. Press **Play** in Unity. 

### Keyboard Debug Controls:
- `Space` = Step simulation play action
- `T` = Call a team timeout
- `P` = Set strategy to Fast Pace
- `O` = Set strategy to Perimeter Focus
- `D` = Set strategy to Press Defense
