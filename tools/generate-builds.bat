@echo off
REM Generate both Ninja and Visual Studio build trees
REM
REM Usage:
REM   generate-builds.bat [options]
REM
REM Examples:
REM   generate-builds.bat
REM   generate-builds.bat -ProfileHost profiles/windows-msvc-asan.ini -BuildType Debug
REM
REM See generate-builds.ps1 for full documentation

powershell -ExecutionPolicy Bypass -File "%~dp0generate-builds.ps1" %*
