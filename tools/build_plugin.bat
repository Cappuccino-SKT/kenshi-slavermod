@echo off
rem Builds a RE_Kenshi plugin with the VC++ 2010 x64 compiler (toolset v100),
rem the one KenshiLib requires.
rem Usage: tools\build_plugin.bat <source_dir> <DllName>
rem Example: tools\build_plugin.bat . SlaverMod
rem
rem Paths can be overridden with environment variables:
rem   KENSHI_VC   - Visual C++ 2010 folder (default: C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC)
rem   KENSHI_SDK  - Windows SDK 7.1 folder (default: C:\Program Files\Microsoft SDKs\Windows\v7.1)
rem   KENSHI_DEPS - KenshiLib_Examples_deps folder (default: C:\KenshiDev\deps\KenshiLib_Examples_deps)
setlocal
set SRC=%~1
set NAME=%~2
if "%SRC%"=="" goto usage
if "%NAME%"=="" goto usage

set VC=%KENSHI_VC%
if "%VC%"=="" set VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC
set SDK=%KENSHI_SDK%
if "%SDK%"=="" set SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1
set DEPS=%KENSHI_DEPS%
if "%DEPS%"=="" set DEPS=C:\KenshiDev\deps\KenshiLib_Examples_deps
set KENSHILIB_DIR=%DEPS%\KenshiLib
set BOOST=%DEPS%\boost_1_60_0
set FIX=%~dp0include_fix

set PATH=%VC%\bin\amd64;%SDK%\Bin\x64;%SDK%\Bin;%PATH%
set INCLUDE=%FIX%;%DEPS%;%KENSHILIB_DIR%\Include;%KENSHILIB_DIR%\Include\ogre;%BOOST%;%VC%\include;%SDK%\Include
set LIB=%KENSHILIB_DIR%\Libraries;%BOOST%\stage\lib;%VC%\lib\amd64;%SDK%\Lib\x64

if not exist "%SRC%\build" mkdir "%SRC%\build"
pushd "%SRC%\build"
cl.exe /nologo /c /EHsc /MD /O2 /GL /W3 /Zi /DNDEBUG /D_WINDOWS /D_USRDLL /DUNICODE /D_UNICODE ..\*.cpp
if errorlevel 1 goto fail
link.exe /nologo /DLL /LTCG /DEBUG /OPT:REF /OPT:ICF /OUT:%NAME%.dll *.obj kenshilib.lib MyGUIEngine_x64.lib OgreMain_x64.lib user32.lib
if errorlevel 1 goto fail
popd
echo BUILD OK: %SRC%\build\%NAME%.dll
exit /b 0
:fail
popd
echo BUILD FAILED
exit /b 1
:usage
echo Usage: build_plugin.bat ^<source_dir^> ^<DllName^>
exit /b 2
