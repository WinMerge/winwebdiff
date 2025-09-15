cd /d "%~dp0"

del /s BuildTmp\*.res

setlocal
for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)
if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
  call "%InstallDir%\Common7\Tools\vsdevcmd.bat
)

if "%1" == "" (
  call :BuildBin x86 || goto :eof
  call :BuildBin x64 || goto :eof
  call :BuildBin ARM64 || goto :eof
) else (
  call :BuildBin %1 || goto :eof
)

endlocal

goto :eof

:BuildBin

del /s Build\%1\Release\WinWebDiff\*.exe
MSBuild WinWebDiff.sln /t:restore /p:RestorePackagesConfig=true || pause
MSBuild WinWebDiff.sln /t:rebuild /p:Configuration=Release /p:Platform="%1" || pause

if exist "%SIGNBAT_PATH%" (
  call "%SIGNBAT_PATH%" Build\%1\Release\WinWebDiff\WinWebDiff.exe
  call "%SIGNBAT_PATH%" Build\%1\Release\WinWebDiff\WinWebDiffLib.dll
)
goto :eof
