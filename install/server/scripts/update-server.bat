@echo off
cd /d "%~dp0"

echo Updating the Arma Reforger server...
echo.

if not exist ".\steamcmd\steamcmd.exe" (
    echo ERROR: SteamCMD is not installed. Run install-server.bat first.
    pause
    exit /b 1
)

".\steamcmd\steamcmd.exe" +force_install_dir "%~dp0server" +login anonymous +app_update 1874900 validate +quit

echo.
echo Update finished.
echo.
echo Reminder: after a game update, the mods usually need to re-download on the
echo next start. The first launch will be slow and players cannot join until it
echo is done.
pause
