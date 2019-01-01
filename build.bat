@echo off
setlocal enableextensions enabledelayedexpansion

set configuration=debug
set platform=LOX_PLATFORM_WINDOWS
set compileflags=-std=c99 -D!platform!=1 -I../src
set linkflags=
set source=../src/main.c ../src/chunk.c ../src/memory.c ../src/debug.c ../src/value.c ../src/vm.c ../src/compiler.c ../src/scanner.c ../src/object.c ../src/table.c
if not exist bin mkdir bin

for %%a in (%*) do (
	if "%%a"=="debug" (
		set configuration=debug
	) else if "%%a"=="release" (
		set configuration=release
	)
)

if "%configuration%"=="release" (
	set compileflags=!compileflags! -O2 -mwindows /DLOX_RELEASE_BUILD=1
) else (
	set compileflags=!compileflags! -g -Wall
)

echo Configuration: !configuration!
echo Flags:         !compileflags!
echo Linker:        !linkflags!

pushd bin
	echo Compiling app...
	gcc !source! !compileflags! -o lox.exe !linkflags!
popd
