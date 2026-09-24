@echo off
cd /d "%~dp0"

echo ============================================================
echo  Panel Arma Reforger
echo  Une fois lance, ouvre: http://localhost:8080
echo  Ctrl+C dans cette fenetre pour quitter le panel.
echo ============================================================
echo.

python "%~dp0panel.py"

pause
