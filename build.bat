@echo off
setlocal

set "MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%MSBUILD%" (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -find "MSBuild\**\Bin\MSBuild.exe"`) do (
        set "MSBUILD=%%i"
    )
)

echo [BUILD] Using MSBuild: "%MSBUILD%"
echo [BUILD] Building PrintDLL.sln (Release x64)...
"%MSBUILD%" "%~dp0PrintDLL.sln" /p:Configuration=Release /p:Platform=x64
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed with code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build completed successfully.
exit /b 0
