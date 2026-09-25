@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  Campaign save backup.
REM
REM  Reoccupation's author recommends backing up at least once a
REM  day: campaign saves can be corrupted by a mod update, and a
REM  corrupted save is unrecoverable without a copy. We have lost
REM  vehicles to this before.
REM
REM  Run it from the server root (next to config.json). Schedule
REM  it daily with Task Scheduler, or call it before any mod
REM  update. Keeps the last 14 days, deletes older ones.
REM ============================================================

set ROOT=%~dp0
set SRC=%ROOT%profile\profile
set DEST=%ROOT%SAVEGAME_BACKUPS

if not exist "%SRC%\.db" (
    echo ERROR: no save database at %SRC%\.db
    echo Check that your -profile path matches. Reforger nests it:
    echo   -profile .\profile  gives  .\profile\profile\
    exit /b 1
)

REM Sortable timestamp, locale-independent (WMIC returns YYYYMMDDHHMMSS).
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value 2^>nul') do set LDT=%%I
set STAMP=%LDT:~0,4%-%LDT:~4,2%-%LDT:~6,2%_%LDT:~8,2%h%LDT:~10,2%
set TARGET=%DEST%\%STAMP%

echo Backing up to %TARGET% ...
robocopy "%SRC%\.db"   "%TARGET%\.db"   /E /NFL /NDL /NJH /NJS /R:1 /W:1 >nul
robocopy "%SRC%\.save" "%TARGET%\.save" /E /NFL /NDL /NJH /NJS /R:1 /W:1 >nul
copy "%SRC%\FFRX_*.json" "%TARGET%\" >nul 2>&1
copy "%ROOT%config.json" "%TARGET%\" >nul 2>&1

if not exist "%TARGET%\.db" (
    echo ERROR: backup failed, nothing was copied.
    exit /b 1
)

REM Prune anything older than 14 days. Directory names sort chronologically,
REM so this is just "keep the newest 14 entries".
set /a KEEP=14
set /a N=0
for /f "delims=" %%D in ('dir /b /ad /o-n "%DEST%" 2^>nul') do (
    set /a N+=1
    if !N! GTR %KEEP% (
        echo   pruning old backup %%D
        rd /s /q "%DEST%\%%D"
    )
)

echo Done. Backups kept: %KEEP% most recent.
endlocal
