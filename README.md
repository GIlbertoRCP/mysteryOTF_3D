# Noir Mystery 3D (working title)

A semi-realistic, cinematic noir murder mystery in Unreal Engine 5. It grows out of the web game
Detective Board Sync ([GIlbertoRCP/MysteryOTF](https://github.com/GIlbertoRCP/MysteryOTF)), which stays a separate project.

## Toolchain

| Item | Version |
| --- | --- |
| Engine | Unreal Engine **5.8.3** (launcher build, `C:\UnrealEngine\UE_5.8`) |
| Compiler | Visual Studio 2026 Community, MSVC 14.51 |
| Version control | Git + Git LFS (`.uasset`/`.umap` and source art are LFS, lockable) |

## Build

```bash
"C:/UnrealEngine/UE_5.8/Engine/Build/BatchFiles/Build.bat" NoirMysteryEditor Win64 Development -Project="B:/mysteryOTF/NoirMystery.uproject" -WaitMutex
```

Then open `NoirMystery.uproject`. The editor starts in `L_LookDev`.

## Layout

```
Source/NoirMystery/   game module (player, interaction, search, interrogation, case board)
Source/MysteryCore/   (milestone 3) plain C++ port of the seeded case generator
Content/Maps/         L_LookDev: milestone 1 look-development room
Content/Materials/    M_Master and its instances
Content/Data/         case text in English and Spanish
Content/Art/          imported kits, kept as bought
Scripts/              editor Python that builds and captures the look-dev level
```

## Look development (milestone 1)

`Scripts/build_lookdev.py` rebuilds `L_LookDev` (clearing its actors) and the `M_Master` instances: an 8 x 6 m
placeholder room with oxblood walls, a window letting in cold teal moonlight, warm practicals (desk lamp,
fireplace, case-board lamp), a torch preview, volumetric fog and a global post-process volume (manual
exposure, teal shadows / warm highlights, grain, vignette).

```bash
"C:/UnrealEngine/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "B:/mysteryOTF/NoirMystery.uproject" -ExecutePythonScript="B:/mysteryOTF/Scripts/build_lookdev.py" -unattended -nosplash -nullrhi
```

`Scripts/capture_lookdev.py` renders `Cam_Hero` to `Saved/Screenshots/` for comparison against the mood board (run it with `-ExecCmds="py Scripts/capture_lookdev.py"`; `-ExecutePythonScript` quits before it can render).
Re-running the build script overwrites the level, so copy any values you tune in the editor back into the script.
