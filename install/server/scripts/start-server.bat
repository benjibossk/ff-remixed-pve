@echo off
set ROOT=%~dp0

if not exist "%ROOT%server\ArmaReforgerServer.exe" (
    echo ERROR: the server is not installed.
    echo Run install-server.bat first.
    pause
    exit /b 1
)

if not exist "%ROOT%config.json" (
    echo ERROR: config.json is missing.
    echo Copy config.example.json to config.json and fill it in.
    pause
    exit /b 1
)

echo ============================================================
echo  Starting the Arma Reforger server
echo  Config  : %ROOT%config.json
echo  Profile : %ROOT%profile
echo  Ports   : 2001/UDP (game)  17777/UDP (A2S query)
echo            RCON: 127.0.0.1:19999 (loopback only)
echo ============================================================
echo.

REM IMPORTANT: we must cd into server\ so the engine resolves addons\core and
REM addons\data through the relative ./addons path. Launching from anywhere else
REM fails with "Game addon '58D0FB3206B6F859' not found".
cd /d "%ROOT%server"

"%ROOT%server\ArmaReforgerServer.exe" -config "%ROOT%config.json" -profile "%ROOT%profile" -maxFPS 60 -logStats 60000 -logVoting -logFPS

echo.
echo The server has stopped.
pause
