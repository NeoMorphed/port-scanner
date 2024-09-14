@echo off

set obj="bin-int/debug/"
set executable="bin/debug/"
set pdb="bin/debug"

set start=%time%



set codePaths= src/*.cpp

set Libs= kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib
set CompilerFlags= /nologo /JMC /Oi /WX /wd4100 /wd4458 /wd4127 /wd4201 /wd4189 /wd4505 /wd4700 /wd4312 /Od /MP /W4 /D_ITERATOR_DEBUG_LEVEL=2 /diagnostics:column /MDd /EHsc -Zi 
rem set obj_path=bin-int/debug/*.obj

REM /d2cgsummary
rem src/*.c src/vendor/stb_image/*.cpp src/vendor/imgui/*.cpp src/vendor/imguizmo/*.cpp
rem Only need to compile these files if compiler options change
rem cl /c %CompilerFlags% /Fo%obj% /Fe%executable% /Fd%pdb% /I dependencies/include /I src /I dependencies/include/physx /I src/vendor/imgui src/*.c src/vendor/stb_image/*.cpp src/vendor/imgui/*.cpp src/vendor/imguizmo/*.cpp
if [%1] == [] GOTO DEBUG
if %1 == -r GOTO RELEASE

:DEBUG
echo debug
cl %CompilerFlags% /Fo%obj% /Fe%executable% /Ycpch.h /Fd%pdb% /I src pch.cpp /D __WINDOWS__ /link /ignore:4099 /LIBPATH:dependencies/lib %Libs% %*
GOTO END

:RELEASE
echo release
set obj="bin-int/release/"
set executable="bin/release/"
set pdb="bin/release"
set CompilerFlags= /nologo /JMC /Oi /WX /wd4100 /wd4458 /wd4127 /wd4996 /wd4201 /wd4189 /wd4505 /wd4700 /wd4312 /O2 /MP /W4 /diagnostics:column /MT /EHsc -Zi 
cl %CompilerFlags% /Fo%obj% /Fe%executable% /Ycpch.h /I src pch.cpp /D __WINDOWS__ /link /ignore:4099 /LIBPATH:dependencies/lib %Libs% 
rem set level=%errorLevel%

rem if %level% == 0 run.bat

rem set end=%time%
:END
