@echo off
pushd ..\..\build
rem disable warning 4100: unreferenced formal parameter
rem disable warning 4201: nameless struct/union
rem SOMETIMES disable warning 4189: local var initialized but not used
cl -MT -Gm- -nologo -Oi -Od -GR- -FC -Zi -W4 ^
    -wd4100 -wd4201 ^
    -DHANDMADE_SLOW ^
    ..\handmade\code\win32_handmade.cpp user32.lib gdi32.lib winmm.lib
popd
