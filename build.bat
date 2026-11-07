@echo off
set "PATH=C:\Program Files\CodeBlocks\MinGW\bin;%PATH%"
cmake -S . -B build -G "Ninja"
cmake --build build

