@echo off
cd test2

if exist sometune.asm ( del /Q sometune.asm )
..\bin\rmt2atasm.exe sometune.rmt > sometune.asm
cd atasm
rmdir /S disk /Q
rmdir /S out /Q
mkdir disk
mkdir out
atasm.exe playit.asm -odisk/playit.xex
dir2atr -E -b PicoBoot406 out/cerebus.atr disk
cd ..
pause