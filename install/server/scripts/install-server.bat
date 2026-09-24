@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo  Arma Reforger dedicated server - installation
echo ============================================================
echo.

REM --- 1) Fetch SteamCMD if we do not have it yet ---
if not exist ".\steamcmd\steamcmd.exe" (
    echo [1/2] Downloading SteamCMD...
    if not exist ".\steamcmd" mkdir ".\steamcmd"
    powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip' -OutFile '.\steamcmd\steamcmd.zip'"
    if errorlevel 1 (
        echo ERROR: SteamCMD download failed.
        pause
        exit /b 1
    )
    echo Extracting SteamCMD...
    powershell -NoProfile -Command "Expand-Archive -Path '.\steamcmd\steamcmd.zip' -DestinationPath '.\steamcmd' -Force"
    del ".\steamcmd\steamcmd.zip"
) else (
    echo [1/2] SteamCMD already present, skipping.
)

echo.
echo [2/2] Downloading the dedicated server (~11 GB).
echo       This takes a while depending on your connection.
echo.

REM First pass lets SteamCMD update itself. Without it, the very first app_update
REM fails with "Missing configuration" on a fresh install.
".\steamcmd\steamcmd.exe" +quit >nul 2>&1

REM App ID 1874900 = Arma Reforger Server. No account needed.
".\steamcmd\steamcmd.exe" +force_install_dir "%~dp0server" +login anonymous +app_update 1874900 validate +quit

if errorlevel 1 (
    echo.
    echo ERROR while downloading the server.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Done.
echo.
echo  Next:
echo    1. copy config.example.json to config.json and fill it in
echo    2. run start-server.bat
echo ============================================================
pause
