@echo off
rem Plays Noir Mystery as a standalone game window, starting on the title screen.
rem -LogInput writes the keys the game receives to Saved\Logs\NoirMystery.log (for input problems).
start "" "C:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0NoirMystery.uproject" -game -windowed -ResX=1920 -ResY=1080 -LogInput
