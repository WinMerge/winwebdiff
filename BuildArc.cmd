cd /d "%~dp0"

set DISTDIR=.\Dist
set path="%ProgramFiles%\7-zip";"%ProgramFiles(x86)%\7-zip";%path%

for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [15.0^,16.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)

if "%1" == "" (
  call :BuildArc x86 || goto :eof
  call :BuildArc x64 || goto :eof
  call :BuildArc ARM64 || goto :eof
) else (
  call :BuildArc %1 || goto :eof
)

goto :eof

:BuildArc

mkdir "%DISTDIR%\%1\WinWebDiff\" 2> NUL

copy Build\%1\Release\WinWebDiff\WinWebDiff.exe "%DISTDIR%\%1\WinWebDiff\"
copy Build\%1\Release\WinWebDiff\WinWebDiffLib.dll "%DISTDIR%\%1\WinWebDiff\"
copy Build\%1\Release\WinWebDiff\cidiff.exe "%DISTDIR%\%1\WinWebDiff\"
call :GET_EXE_VERSION %~dp0Build\%1\Release\WinWebDiff\WinWebDiff.exe
copy LICENSE*.txt "%DISTDIR%\%1\WinWebDiff"

7z.exe a -tzip "%DISTDIR%\winwebdiff-%EXE_VERSION%-%1.zip" "%DISTDIR%\%1\WinWebDiff"

goto :eof

:GET_EXE_VERSION

SET EXE_PATH=%1
WMIC Path CIM_DataFile WHERE Name='%EXE_PATH:\=\\%' Get Version | findstr /v Version > _tmp_.txt
set /P EXE_VERSIONTMP=<_tmp_.txt
set EXE_VERSION=%EXE_VERSIONTMP: =%
del _tmp_.txt
goto :eof

