# 🎬 UEVR RenderDoc Capture — Beginner's Guide

Capture a **real GPU frame** from almost any Unreal Engine game and open it in
[RenderDoc](https://renderdoc.org/) — no source code, no game cooperation needed.

This is [praydog's **UEVR**](https://github.com/praydog/UEVR) with an embedded
[RenderDoc](https://renderdoc.org/) capture system bolted on. UEVR injects into
the game; RenderDoc rides along inside the process; and a tiny file "trigger"
tells it to grab a frame. The result is a normal `.rdc` file you open in
RenderDoc like any other capture.

It works for **flat (2D) games** and for **VR** (UEVR's whole reason to exist) —
including running under the **Meta XR Simulator** so you don't even need a
headset.

---

## ⏱️ TL;DR (the 60-second version)

1. Download the release, unzip it somewhere (e.g. `C:\UEVR-RenderDoc`).
2. Open **PowerShell** in that folder.
3. Launch your game through the launcher:
   ```powershell
   .\UEVRRenderDocLauncher.exe --exe "C:\Path\To\YourGame-Shipping.exe" --wait
   ```
4. Once the game is rendering, take a capture:
   ```powershell
   .\Capture-RenderDoc.ps1
   ```
5. Open the `.rdc` it prints out in **RenderDoc** (`qrenderdoc.exe`). Done. 🎉

The rest of this guide explains each step, plus VR and troubleshooting.

---

## 📦 What's in the release

| File | What it is |
|------|------------|
| `UEVRBackend.dll` | UEVR itself (the thing injected into the game). |
| `renderdoc.dll` | The RenderDoc runtime that does the actual capture. |
| `openxr_loader.dll` | Needed for VR (OpenXR). Harmless for flat games. |
| `openvr_api.dll` | Needed for VR via SteamVR/OpenVR. |
| `UEVRRenderDocLauncher.exe` | **The easy button.** Launches your game with RenderDoc + UEVR already inside, before the game draws a single frame. |
| `UEVRRenderDocSmoke.exe` | A tiny test app that draws a triangle — handy for confirming capture works without a real game. |
| `Capture-RenderDoc.ps1` | Triggers one capture and tells you where the `.rdc` landed. |
| `tools\*.ps1` | Extra helper scripts (validate captures, smoke test). |

> You do **not** need to install RenderDoc separately to *capture* — the runtime
> is bundled. You only need RenderDoc installed to **open and look at** the
> `.rdc` afterwards. Grab it from <https://renderdoc.org/> (free).

---

## ✅ Requirements

- **Windows 10/11, 64-bit.**
- A **DirectX 12** (or DirectX 11) Unreal Engine game. Most modern UE4/UE5
  games qualify.
- **RenderDoc** installed if you want to *view* captures (optional for capturing).
- Captures can be **big** (hundreds of MB to a couple GB). Have some free disk.

---

## 🚀 Step-by-step: capture a flat (2D) game

### 1. Unzip the release

Put it anywhere with no spaces issues, e.g. `C:\UEVR-RenderDoc`. Keep all the
files together — the launcher loads `UEVRBackend.dll` and `renderdoc.dll` from
the same folder it lives in.

### 2. Find your game's real executable

Many games have a small launcher `.exe` that starts the *real* game in a
different folder. You want the **big** shipping executable, usually here:

```
...\<Game>\Binaries\Win64\<Game>-Win64-Shipping.exe
```

> **Tip:** Start the game normally, open Task Manager → Details, find the
> process using the most memory, right-click → *Open file location*. That's the
> exe you want.

### 3. Launch the game through the launcher

Open **PowerShell** in the unzip folder (Shift+Right-click → *Open PowerShell
window here*) and run:

```powershell
.\UEVRRenderDocLauncher.exe --exe "C:\Path\To\YourGame-Win64-Shipping.exe" --wait
```

- If the game needs to start from a specific folder, add `--cwd "C:\that\folder"`.
- If the game needs command-line arguments, put them after `--`, e.g.
  `... --exe "...\Game.exe" -- -dx12`.

What the launcher does for you, in order:
1. Starts the game **paused**.
2. Injects `renderdoc.dll` (so RenderDoc wraps the graphics objects *from the
   very beginning* — this is what makes captures clean).
3. Injects `UEVRBackend.dll`.
4. Waits until UEVR says "I'm ready", then **unpauses** the game.

You'll see the game start. Let it reach actual gameplay / a menu — anything
that's drawing frames.

### 4. Take a capture

In another PowerShell window in the same folder:

```powershell
.\Capture-RenderDoc.ps1
```

It writes a tiny trigger file, waits for the capture to finish, and prints the
path of the `.rdc`. By default captures land in:

```
%TEMP%\uevr_renderdoc\
```

Want to choose where it goes or capture multiple frames?

```powershell
.\Capture-RenderDoc.ps1 -OutDir "C:\captures\mygame" -Frames 1
```

### 5. Open the capture

Launch **RenderDoc** (`qrenderdoc.exe`) and open the `.rdc`, or just
double-click it. Explore the frame: every draw call, texture, shader, and
pipeline state is there. 🔬

---

## 🥽 Capturing VR frames (with the Meta XR Simulator)

UEVR turns flat UE games into VR. You can capture those **stereo** frames too —
and you don't need a real headset thanks to the **Meta XR Simulator**.

### 1. Install + activate the Meta XR Simulator

- Download it from Meta:
  <https://developer.oculus.com/downloads/package/meta-xr-simulator/>
- Run its `activate_simulator.ps1` (it ships with the simulator) so it becomes
  your active OpenXR runtime. You can confirm it's active here in PowerShell:
  ```powershell
  Get-ItemPropertyValue "HKLM:\SOFTWARE\Khronos\OpenXR\1" -Name ActiveRuntime
  ```
  It should point at `...\MetaXRSimulator\...\meta_openxr_simulator.json`.

### 2. Make sure `openxr_loader.dll` is reachable

UEVR loads `openxr_loader.dll` from **the game's own `Binaries\Win64` folder**.
Copy the bundled `openxr_loader.dll` next to the game's shipping exe:

```powershell
Copy-Item .\openxr_loader.dll "C:\Path\To\Game\Binaries\Win64\"
```

(If you skip this, UEVR logs `Could not load openxr_loader.dll` and stays flat.)

### 3. Launch and capture exactly like before

```powershell
.\UEVRRenderDocLauncher.exe --exe "C:\Path\To\YourGame-Win64-Shipping.exe" --wait
```

The Meta XR Simulator window pops up showing the game in stereo (two eye views).
Once it's rendering, run `.\Capture-RenderDoc.ps1`. The `.rdc` now contains the
full stereo VR frame — both eyes, the OpenXR swapchains, the lot.

---

## 🧪 No game handy? Run the smoke test

`UEVRRenderDocSmoke.exe` is a minimal D3D12 app that just clears the screen. Use
it to prove capturing works end-to-end on your machine:

```powershell
.\tools\run_renderdoc_smoke_capture.ps1 `
  -Launcher .\UEVRRenderDocLauncher.exe `
  -Smoke   .\UEVRRenderDocSmoke.exe
```

---

## 🔧 How the trigger actually works (for the curious)

The capture "button" is just a text file. While the game runs, UEVR watches:

```
%TEMP%\uevr_renderdoc_capture.req
```

Write that file and UEVR captures the next frame, then deletes the file. The
file format is:

```
<capture file path template>      <- line 1 (optional; e.g. C:\caps\game)
frames=1                          <- line 2 (optional)
```

`Capture-RenderDoc.ps1` just writes that file for you. So you can trigger a
capture from *anything* — a hotkey tool, another script, an AI agent, etc.

---

## ⚙️ Useful environment variables

The launcher sets these for you, but you can also set them yourself if you
inject UEVR a different way:

| Variable | Meaning |
|----------|---------|
| `UEVR_RENDERDOC_BOOTSTRAP=1` | Turn the whole RenderDoc system on. |
| `UEVR_LOAD_RENDERDOC_DLL=1` | Let UEVR load `renderdoc.dll` itself (late). |
| `UEVR_RENDERDOC_DLL=<path>` | Use a specific `renderdoc.dll`. |
| `UEVR_RENDERDOC_PREHOOK_D3D12=1` | Install D3D12 hooks before the game starts (the launcher does this). |
| `UEVR_RENDERDOC_TRACK_ACTIVE_PAIR=1` | Capture the exact game window/device instead of a guess. |
| `UEVR_DISABLE_RENDERDOC_BOOTSTRAP=1` | Hard-off switch. |

---

## 🆘 Troubleshooting

**"No `.rdc` appeared."**
- Make sure the game was actually drawing frames when you triggered.
- Check UEVR's log (see below) for `[RenderDoc] watcher: ...` lines.

**The capture is tiny / empty.**
- You probably injected *after* the game already created its graphics objects.
  Always use `UEVRRenderDocLauncher.exe` — it gets RenderDoc in first. The log
  should say `capture_safe=true`.

**VR doesn't engage / it stays flat.**
- `Could not load openxr_loader.dll` → copy `openxr_loader.dll` into the game's
  `Binaries\Win64` folder (see the VR section).
- Confirm the Meta XR Simulator (or your headset runtime) is the **ActiveRuntime**.

**Where's the UEVR log?**
```
%APPDATA%\UnrealVRMod\<GameExeName>\log.txt
```
Search it for `RenderDoc` and `watcher` to see exactly what happened.

**The game crashes when going into VR with RenderDoc on.**
- This build includes a fix for that (UEVR's stereo setup now understands
  RenderDoc-wrapped textures). If you built it yourself from source, make sure
  you applied the `patches/UESDK-StereoStuff-renderdoc.patch` change.

---

## 🙏 Credits & license

- **UEVR** is by [praydog](https://github.com/praydog/UEVR). All the heavy
  lifting (injecting into UE games, the VR mod, the D3D hooks) is theirs.
- **RenderDoc** is by Baldur Karlsson and contributors — <https://renderdoc.org/>.
- This repo only adds the embedded RenderDoc capture plumbing and these docs.

UEVR's source is **copyright praydog, all rights reserved**. Respect the
original project's terms.
