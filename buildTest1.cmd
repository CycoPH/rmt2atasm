@echo off
cd test1

if exist tune.asm ( del /Q tune.asm )
..\bin\rmt2atasm.exe sample.rmt > tune.asm
cd atasm
rmdir /S disk /Q
rmdir /S out /Q
mkdir disk
mkdir out
atasm.exe test1.asm -odisk/test1.xex
dir2atr -E -b PicoBoot406 out/cerebus.atr disk
cd ..
pause