@echo off
mkdir build\temp 2> NUL

set CommonCompilerFlags=/nologo /Zi /Fdbuild\ /Fobuild\ /Wall
set CommonLinkerFlags=/out:build\win32_main.exe /pdb:build\ /subsystem:windows gdi32.lib user32.lib opengl32.lib dsound.lib
REM set Timestamp=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set Timestamp=%random%%random%

del *.pdb > NUL 2> NUL

cl %CommonCompilerFlags% code\win32_main.c /link %CommonLinkerFlags% -incremental:no
