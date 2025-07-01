@echo off
rem Enter the x64 MSVC environment of whichever Visual Studio is installed.
rem Never hardcode the edition/year: the runner image changes it (2022 -> 18)
rem and a failed `call` is not fatal in cmd, so the build would silently fall
rem back to another compiler found on PATH (gcc/clang) with a different set of
rem detected features -- see the WrapRt/Qt6Core find_package failure.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo error: vswhere.exe not found
    exit /b 1
)

set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"

if not defined VSPATH (
    echo error: no Visual Studio installation with the x64 C++ toolset
    exit /b 1
)

if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" (
    echo error: vcvars64.bat not found in "%VSPATH%"
    exit /b 1
)

echo Using Visual Studio from "%VSPATH%"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

where cl.exe >nul 2>&1 || (
    echo error: cl.exe is not in PATH after vcvars64.bat
    exit /b 1
)

cl.exe 2>&1 | findstr /r /c:"Version"
exit /b 0
