# UEVRPureDark — build & deploy notes

## ⚠️ Plugin deploy location (DO NOT get this wrong)

UEVR loads its plugins (e.g. `afw_frame_resources.dll`) from the **GLOBAL** dir:

```
%APPDATA%\UnrealVRMod\UEVR\plugins\afw_frame_resources.dll
```

**NOT** the game-specific `%APPDATA%\UnrealVRMod\<Game>-Win64-Shipping\plugins\` dir
(that one holds `uevr_mcp.dll`, installed by `uevr_setup_game`).

After rebuilding a plugin you MUST copy the new DLL to the **global** `UEVR\plugins\` path,
or the game silently keeps running the **old** DLL and your change has no effect. Verify which
DLL is actually loaded:

```powershell
(Get-Process AFW2-Win64-Shipping).Modules | ? { $_.ModuleName -match 'afw_frame' } |
  % { $_.FileName; (Get-Item $_.FileName).LastWriteTime }   # compare to your build's timestamp
```

## Build recipes

UEVR backend (core + shaders). The cmake step runs `compile_shaders.bat` (fxc → `.inc`) then relinks.
**The game must be closed first** (the loaded `UEVRBackend.dll` is file-locked → `LNK1104`).

```
cmake --build build --config Release --target uevr
```

`afw_frame_resources` plugin (the depth/velocity provider). Builds to
`build\Release\afw_frame_resources.dll` — then copy it to the global plugins dir above:

```
cmake --build build --config Release --target afw_frame_resources
```

(Generator: `cmake -B build -G "Visual Studio 17 2022" -A x64`. `vcvars64.bat` is broken in this
shell — rely on the VS generator. Transient `CL.exe -1` during a parallel build = OOM; just re-run.)

## More context

Deep project state, AFW combine status, the MetaXR-sim testing gotchas, and the DLSS
velocity-recipe findings live in the auto-memory at
`C:\Users\ellio\.claude\projects\E--Github-UEVRPureDark\memory\` (see `MEMORY.md`).
