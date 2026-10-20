@echo off
rem Builds MysteryCore and its parity test with MSVC (no Unreal needed) and runs it.
rem   Tests\MysteryCoreTests\run.bat
rem Needs Saved\MysteryGolden\golden.ndjson; regenerate it with Tools\MysteryExport\export.mts.
setlocal
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\Binaries\MysteryCoreTests"
if not exist "%OUT%" mkdir "%OUT%"

if defined VCINSTALLDIR goto build
rem Find vcvars64.bat: vswhere when it exists, else any edition under Program Files.
rem (Kept outside parenthesised blocks: the "(x86)" in the path would end them.)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS for /d %%v in ("%ProgramFiles%\Microsoft Visual Studio\*") do for /d %%e in ("%%v\*") do if exist "%%e\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%e\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS (echo Visual Studio with C++ tools not found & exit /b 1)
call "%VCVARS%" >nul || exit /b 1

:build
cl /nologo /std:c++20 /EHsc /O2 /W4 /utf-8 /permissive- ^
  /I "%ROOT%\Source\MysteryCore\Public" ^
  "%ROOT%\Source\MysteryCore\Private\Json.cpp" ^
  "%ROOT%\Source\MysteryCore\Private\Text.cpp" ^
  "%ROOT%\Source\MysteryCore\Private\Content.cpp" ^
  "%ROOT%\Source\MysteryCore\Private\Blueprint.cpp" ^
  "%ROOT%\Source\MysteryCore\Private\Case.cpp" ^
  "%ROOT%\Source\MysteryCore\Private\Staging.cpp" ^
  "%~dp0ParityTest.cpp" ^
  /Fo"%OUT%\\" /Fe"%OUT%\ParityTest.exe" || exit /b 1

"%OUT%\ParityTest.exe" "%ROOT%\Content\Data\Mystery\content.json" "%ROOT%\Saved\MysteryGolden\golden.ndjson"
