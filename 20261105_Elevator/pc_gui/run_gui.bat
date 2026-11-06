@echo off
cd /d "%~dp0"

rem use "py" launcher if present (avoids the Microsoft Store python alias)
set PY=python
where py >nul 2>nul && set PY=py

%PY% --version >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Python not found. Install Python 3.10+ from python.org with "Add python.exe to PATH" checked.
    pause
    exit /b 1
)

%PY% -c "import PySide6, serial" >nul 2>nul
if errorlevel 1 (
    echo Installing pyside6, pyserial ...
    %PY% -m pip install pyside6 pyserial
)

%PY% elevator_gui.py
if errorlevel 1 pause
