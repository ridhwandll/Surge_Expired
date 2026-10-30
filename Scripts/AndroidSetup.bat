@echo off
:: Copyright (c) - SurgeTechnologies - All rights reserved
:: Installs everything needed to build Surge for Android, no Android Studio required:
::   JDK 17, Android SDK command-line tools, platform-tools, SDK platform, build-tools, NDK, CMake (versions read from Android/app/build.gradle)
:: then sets ANDROID_HOME / ANDROID_SDK_ROOT / JAVA_HOME / PATH for the current user, writes Android/local.properties and warms up Gradle.
:: Asks before every download and every change to your system. Safe to re-run: installed parts are skipped.
:: Scripts/AndroidClean.bat removes it all again.
::
:: Usage: AndroidSetup.bat [install dir] [/y]
::   install dir  Default: the folder above an existing ANDROID_HOME, else %LOCALAPPDATA%\Android. Layout: <dir>\Sdk, <dir>\jdk-17
::   /y           Yes to every question (accepts the SDK licenses, skips Android Studio), no pause at the end
::   NO_COLOR=1   Plain output
setlocal EnableExtensions

call :InitColors

:: Before parsing the arguments: shift moves %%0 too
for %%i in ("%~dp0..") do set "REPO_DIR=%%~fi"

set "ASSUME_YES="
set "INSTALL_ARG="
:ParseArgs
if "%~1"=="" goto :ArgsDone
if "%~1"=="/?" goto :Usage
if /i "%~1"=="/y" (set "ASSUME_YES=1") else if /i "%~1"=="-y" (set "ASSUME_YES=1") else (set "INSTALL_ARG=%~f1")
shift
goto :ParseArgs
:ArgsDone

set "GRADLE_FILE=%REPO_DIR%\Android\app\build.gradle"
if not exist "%GRADLE_FILE%" (
    call :Error "Android\app\build.gradle not found, run this script from the Surge repository."
    goto :Fail
)

:: Defaults, overridden by what Android/app/build.gradle asks for
set "NDK_VERSION=27.3.13750724"
set "CMAKE_VERSION=3.22.1"
set "SDK_PLATFORM=36"
set "BUILD_TOOLS=34.0.0"
set "CMDLINE_TOOLS_URL=https://dl.google.com/android/repository/commandlinetools-win-16111833_latest.zip"
set "JDK_URL=https://api.adoptium.net/v3/binary/latest/17/ga/windows/x64/jdk/hotspot/normal/eclipse"

for /f tokens^=2^ delims^=^" %%v in ('findstr /c:"ndkVersion" "%GRADLE_FILE%"') do set "NDK_VERSION=%%v"
for /f tokens^=2^ delims^=^" %%v in ('findstr /c:"version = " "%GRADLE_FILE%"') do set "CMAKE_VERSION=%%v"
for /f "tokens=2 delims=(," %%v in ('findstr /c:"compileSdk Math.max(" "%GRADLE_FILE%"') do set "SDK_PLATFORM=%%v"

:: Install location
set "SDK_DIR="
if defined INSTALL_ARG set "SDK_DIR=%INSTALL_ARG%\Sdk"
if not defined SDK_DIR if defined ANDROID_HOME if exist "%ANDROID_HOME%\" set "SDK_DIR=%ANDROID_HOME%"
if not defined SDK_DIR set "SDK_DIR=%LOCALAPPDATA%\Android\Sdk"
for %%i in ("%SDK_DIR%") do set "SDK_DIR=%%~fi"
for %%i in ("%SDK_DIR%\..\jdk-17") do set "JDK_DIR=%%~fi"
set "SDKMANAGER=%SDK_DIR%\cmdline-tools\latest\bin\sdkmanager.bat"
set "DOWNLOAD_DIR=%TEMP%\SurgeAndroidSetup"
set "STEP_COUNT=6"

echo.
echo %C_TITLE%============================================================%C_RESET%
echo %C_TITLE% Surge Android setup%C_RESET%
echo %C_TITLE%============================================================%C_RESET%
echo  %C_DIM%Android SDK :%C_RESET% %SDK_DIR%
echo  %C_DIM%JDK 17      :%C_RESET% %JDK_DIR%
echo  %C_DIM%Platform    :%C_RESET% android-%SDK_PLATFORM%   %C_DIM%Build-tools:%C_RESET% %BUILD_TOOLS%
echo  %C_DIM%NDK         :%C_RESET% %NDK_VERSION%   %C_DIM%CMake:%C_RESET% %CMAKE_VERSION%
echo  %C_DIM%Disk space  :%C_RESET% about 5 GB on the first run
echo.
echo  Every download and every change to your system is confirmed first.
echo  Nothing is installed system-wide and no administrator rights are needed.
echo %C_TITLE%============================================================%C_RESET%
if not "%SDK_DIR: =%"=="%SDK_DIR%" call :Warn "The SDK path contains spaces, the NDK toolchain may fail. Pass a path without spaces."

where curl.exe >nul 2>&1 || (call :Error "curl.exe not found, needs Windows 10 1803 or newer." & goto :Fail)
where tar.exe >nul 2>&1 || (call :Error "tar.exe not found, needs Windows 10 1803 or newer." & goto :Fail)

echo.
call :Confirm "Start the setup?" || goto :Cancelled
if not exist "%DOWNLOAD_DIR%" mkdir "%DOWNLOAD_DIR%"

:: ---------------------------------------------------------------------------------------------------------------------
call :Step 1 "JDK 17 (Eclipse Temurin)"
if not exist "%JDK_DIR%\bin\java.exe" goto :JdkInstall
call :Ok "Already installed"
goto :JdkDone

:JdkInstall
call :Confirm "Download JDK 17 (about 190 MB) into %JDK_DIR%?" || goto :JdkDeclined
curl.exe -fL --retry 3 --progress-bar -o "%DOWNLOAD_DIR%\jdk17.zip" "%JDK_URL%" || (call :Error "Download failed." & goto :Fail)
if not exist "%JDK_DIR%" mkdir "%JDK_DIR%"
tar.exe -xf "%DOWNLOAD_DIR%\jdk17.zip" -C "%JDK_DIR%" --strip-components=1 || (call :Error "Extraction failed." & goto :Fail)
if not exist "%JDK_DIR%\bin\java.exe" (call :Error "JDK extraction failed, bin\java.exe is missing." & goto :Fail)
call :Ok "Installed"
goto :JdkDone

:JdkDeclined
if not defined JAVA_HOME goto :JdkMissing
if not exist "%JAVA_HOME%\bin\java.exe" goto :JdkMissing
set "JDK_DIR=%JAVA_HOME%"
call :Warn "Using the existing JAVA_HOME (%JAVA_HOME%). The Android build needs JDK 17 or newer."
goto :JdkDone

:JdkMissing
call :Error "Gradle needs a JDK 17+ and JAVA_HOME is not set to one."
goto :Fail

:JdkDone
for %%i in ("%JDK_DIR%") do set "JDK_DIR=%%~fi"
set "JAVA_HOME=%JDK_DIR%"
set "PATH=%JDK_DIR%\bin;%PATH%"

:: ---------------------------------------------------------------------------------------------------------------------
call :Step 2 "Android SDK command-line tools"
if not exist "%SDKMANAGER%" goto :ToolsInstall
call :Ok "Already installed"
goto :ToolsDone

:ToolsInstall
call :Confirm "Download the SDK command-line tools (about 150 MB) into %SDK_DIR%?" || (call :Error "The SDK manager is needed to install the rest of the SDK." & goto :Fail)
curl.exe -fL --retry 3 --progress-bar -o "%DOWNLOAD_DIR%\cmdline-tools.zip" "%CMDLINE_TOOLS_URL%" || (call :Error "Download failed." & goto :Fail)
if not exist "%SDK_DIR%\cmdline-tools\latest" mkdir "%SDK_DIR%\cmdline-tools\latest"
tar.exe -xf "%DOWNLOAD_DIR%\cmdline-tools.zip" -C "%SDK_DIR%\cmdline-tools\latest" --strip-components=1 || (call :Error "Extraction failed." & goto :Fail)
if not exist "%SDKMANAGER%" (call :Error "Command-line tools extraction failed." & goto :Fail)
call :Ok "Installed"

:ToolsDone
set "ANDROID_HOME=%SDK_DIR%"
set "ANDROID_SDK_ROOT=%SDK_DIR%"

:: ---------------------------------------------------------------------------------------------------------------------
call :Step 3 "Android SDK packages"
call :FindMissingPackages
if defined MISSING goto :PackagesInstall
call :Ok "All installed"
goto :PackagesDone

:PackagesInstall
call :Info "Missing:%MISSING%"
call :Info "Installing them accepts the Android SDK license: https://developer.android.com/studio/terms"
call :Confirm "Accept the license and install them (about 3 GB, most of it is the NDK)?" || (call :Warn "Skipped. Gradle can only download them on the first build once the license is accepted." & goto :PackagesDone)
:: Command-line tools 23+ forward sdkmanager to the Android CLI, which accepts the license by installing (it writes licenses\android-sdk-license,
:: --licenses is gone). Older versions ask per license during the install: the piped answers cover both
(for /l %%i in (1,1,40) do @echo y) | "%SDKMANAGER%" --sdk_root="%SDK_DIR%" "platform-tools" "platforms;android-%SDK_PLATFORM%" "build-tools;%BUILD_TOOLS%" "ndk;%NDK_VERSION%" "cmake;%CMAKE_VERSION%" "extras;google;usb_driver"
if errorlevel 1 (call :Error "sdkmanager failed, see the output above." & goto :Fail)
call :FindMissingPackages
if defined MISSING (call :Error "Still missing after the install:%MISSING%" & goto :Fail)
call :Ok "All installed"

:PackagesDone

:: ---------------------------------------------------------------------------------------------------------------------
call :Step 4 "Environment variables (your user account only)"
call :Info "JAVA_HOME          = %JDK_DIR%"
call :Info "ANDROID_HOME       = %SDK_DIR%"
call :Info "ANDROID_SDK_ROOT   = %SDK_DIR%"
call :Info "PATH += %JDK_DIR%\bin"
call :Info "PATH += %SDK_DIR%\platform-tools"
call :Info "PATH += %SDK_DIR%\cmdline-tools\latest\bin"
call :Confirm "Set these for your user?" || (call :Warn "Skipped. gradlew needs JAVA_HOME or java.exe on PATH, set them yourself." & goto :EnvDone)

:: Path is edited through the registry as REG_EXPAND_SZ: setx truncates it at 1024 chars and merges the system Path in,
:: [Environment]::SetEnvironmentVariable would store it as REG_SZ and break %%VAR%% entries. Setting the other variables
:: through .NET afterwards broadcasts the change to Explorer, so newly started programs see it
set "SG_SDK=%SDK_DIR%"
set "SG_JDK=%JDK_DIR%"
set "SG_ADD=%JDK_DIR%\bin;%SDK_DIR%\platform-tools;%SDK_DIR%\cmdline-tools\latest\bin"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $k=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Environment'); $p=[string]$k.GetValue('Path','',[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames); $parts=[System.Collections.Generic.List[string]]::new(); foreach($e in $p.Split(';')){ if($e.Trim()){ $parts.Add($e) } }; foreach($a in $env:SG_ADD.Split(';')){ $dup=$false; foreach($e in $parts){ if([Environment]::ExpandEnvironmentVariables($e).TrimEnd('\') -ieq $a.TrimEnd('\')){ $dup=$true } }; if(-not $dup){ $parts.Add($a) } }; $k.SetValue('Path',($parts -join ';'),[Microsoft.Win32.RegistryValueKind]::ExpandString); $k.Close(); [Environment]::SetEnvironmentVariable('JAVA_HOME',$env:SG_JDK,'User'); [Environment]::SetEnvironmentVariable('ANDROID_HOME',$env:SG_SDK,'User'); [Environment]::SetEnvironmentVariable('ANDROID_SDK_ROOT',$env:SG_SDK,'User')"
if errorlevel 1 (call :Error "Could not update the environment variables." & goto :Fail)
call :Ok "Set"

:EnvDone

:: ---------------------------------------------------------------------------------------------------------------------
call :Step 5 "Android\local.properties"
:: Java properties escaping: sdk.dir=C\:\\Users\\...
set "SDK_DIR_ESCAPED=%SDK_DIR:\=\\%"
set "SDK_DIR_ESCAPED=%SDK_DIR_ESCAPED::=\:%"
call :Info "sdk.dir=%SDK_DIR_ESCAPED%"
call :Confirm "Write it (machine specific, gitignored)?" || (call :Warn "Skipped. Gradle then finds the SDK through ANDROID_HOME." & goto :LocalPropertiesDone)
> "%REPO_DIR%\Android\local.properties" echo ## Generated by Scripts/AndroidSetup.bat, machine specific: do not commit
>> "%REPO_DIR%\Android\local.properties" echo sdk.dir=%SDK_DIR_ESCAPED%
call :Ok "Written"

:LocalPropertiesDone

:: ---------------------------------------------------------------------------------------------------------------------
call :Step 6 "Gradle"
call :Confirm "Download Gradle and the Android Gradle Plugin now (about 400 MB, otherwise the first build does it)?" || (call :Info "Skipped." & goto :GradleDone)
pushd "%REPO_DIR%\Android"
call gradlew.bat --no-daemon --console=plain help
set "GRADLE_RESULT=%ERRORLEVEL%"
popd
if "%GRADLE_RESULT%"=="0" (call :Ok "Ready") else (call :Warn "Gradle warm-up failed, exit code %GRADLE_RESULT%. The toolchain is installed, check the output above.")

:GradleDone

:: ---------------------------------------------------------------------------------------------------------------------
if defined ASSUME_YES goto :StudioDone
if exist "%ProgramFiles%\Android\Android Studio\bin\studio64.exe" goto :StudioDone
where winget.exe >nul 2>&1 || goto :StudioDone
echo.
echo %C_STEP%[Optional]%C_RESET% %C_BOLD%Android Studio%C_RESET%
call :Info "Only needed for on-device native debugging, the Editor exports without it."
call :Confirm "Install Android Studio with winget?" || goto :StudioDone
winget.exe install --id Google.AndroidStudio -e --accept-package-agreements --accept-source-agreements
call :Info "On first launch choose 'Custom' setup and point it at %SDK_DIR% so it reuses this SDK."

:StudioDone
if exist "%DOWNLOAD_DIR%" rmdir /s /q "%DOWNLOAD_DIR%"

echo.
echo %C_OK%============================================================%C_RESET%
echo %C_OK% Android setup complete%C_RESET%
echo %C_OK%============================================================%C_RESET%
echo  - Open a new terminal so it picks up JAVA_HOME / ANDROID_HOME. The Editor finds them without a restart.
echo  - Build and run the Windows Editor once: it compiles the shaders to SPIR-V and copies
echo    Engine/Assets into Android/app/src/main/assets, which the Android build packages.
echo  - Build: Editor Export panel, "Android\gradlew assembleDebug", or open Android/ in Android Studio.
echo  - Device: enable Developer options and USB debugging, then check it shows up with "adb devices".
echo.
if not defined ASSUME_YES pause
exit /b 0

:Cancelled
echo.
call :Warn "Cancelled, nothing was changed."
if not defined ASSUME_YES pause
exit /b 1

:Fail
if exist "%DOWNLOAD_DIR%" rmdir /s /q "%DOWNLOAD_DIR%"
echo.
echo %C_ERR%Setup failed, see the messages above. Re-running the script resumes where it stopped.%C_RESET%
if not defined ASSUME_YES pause
exit /b 1

:Usage
echo Usage: AndroidSetup.bat [install dir] [/y]
echo   install dir  Default: the folder above an existing ANDROID_HOME, else %%LOCALAPPDATA%%\Android
echo   /y           Yes to every question (accepts the SDK licenses, skips Android Studio), no pause at the end
exit /b 0

:: =====================================================================================================================
:: Helpers. Messages are passed quoted: no ^& ^< ^> or %% in them

:FindMissingPackages
set "MISSING="
if not exist "%SDK_DIR%\platform-tools\adb.exe" set "MISSING=%MISSING% platform-tools"
if not exist "%SDK_DIR%\platforms\android-%SDK_PLATFORM%\android.jar" set "MISSING=%MISSING% platforms;android-%SDK_PLATFORM%"
if not exist "%SDK_DIR%\build-tools\%BUILD_TOOLS%" set "MISSING=%MISSING% build-tools;%BUILD_TOOLS%"
if not exist "%SDK_DIR%\ndk\%NDK_VERSION%\source.properties" set "MISSING=%MISSING% ndk;%NDK_VERSION%"
if not exist "%SDK_DIR%\cmake\%CMAKE_VERSION%\bin\cmake.exe" set "MISSING=%MISSING% cmake;%CMAKE_VERSION%"
if not exist "%SDK_DIR%\extras\google\usb_driver\android_winusb.inf" set "MISSING=%MISSING% extras;google;usb_driver"
exit /b 0

:InitColors
:: ANSI colors through cmd's own output (echo, set /p): Windows 10+ consoles and Windows Terminal render them.
:: Everything colored is amber, errors are red
for %%c in (RESET TITLE STEP OK WARN ERR DIM ASK BOLD) do set "C_%%c="
if defined NO_COLOR exit /b 0
for /f %%a in ('echo prompt $E^| cmd') do set "ESC=%%a"
set "C_RESET=%ESC%[0m"
set "C_TITLE=%ESC%[38;5;214m"
set "C_STEP=%ESC%[38;5;214m"
set "C_OK=%ESC%[38;5;214m"
set "C_WARN=%ESC%[38;5;214m"
set "C_ERR=%ESC%[91m"
set "C_DIM=%ESC%[38;5;214m"
set "C_ASK=%ESC%[38;5;214m"
set "C_BOLD=%ESC%[1m"
exit /b 0

:Step
echo.
echo %C_STEP%[%~1/%STEP_COUNT%]%C_RESET% %C_BOLD%%~2%C_RESET%
exit /b 0

:Ok
echo   %C_OK%OK%C_RESET% %~1
exit /b 0

:Info
echo   %C_DIM%%~1%C_RESET%
exit /b 0

:Warn
echo   %C_WARN%WARNING%C_RESET% %~1
exit /b 0

:Error
echo   %C_ERR%ERROR %~1%C_RESET%
exit /b 0

:: Prompt "question" "[keys]": printed by set /p (cmd's own output keeps the colors, choice.exe's /m would not)
:Prompt
<nul set /p "=%C_RESET%  %C_ASK%?%C_RESET% %~1 %C_ASK%%~2%C_RESET% "
exit /b 0

:: Confirm "question": errorlevel 0 = yes, 1 = no. /y answers yes
:Confirm
if not defined ASSUME_YES goto :ConfirmAsk
echo   %C_ASK%?%C_RESET% %~1 %C_DIM%yes ^(/y^)%C_RESET%
exit /b 0
:ConfirmAsk
call :Prompt "%~1" "[Y/N]"
choice /c YN /n
if errorlevel 2 exit /b 1
exit /b 0
