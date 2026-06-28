@echo off
title kOSeki Agent 4'7

echo windows launcher for kOSeki
echo.
echo Where is QEMU?
echo 1) PATH
echo 2) C:\Program Files\qemu\qemu-system-i386.exe
echo 3) custom path...
echo.

:MENU
set /p choice="1/2/3 >> "

if "%choice%"=="1" (
    set QEMU=qemu-system-i386.exe
    goto CHECK_QEMU
)
if "%choice%"=="2" (
    set QEMU=C:\Program Files\qemu\qemu-system-i386.exe
    goto CHECK_QEMU
)
if "%choice%"=="3" (
    set /p QEMU="Enter the path to qemu-system-i386.exe: "
    goto CHECK_QEMU
)

echo hoeh? retry that
goto MENU

:CHECK_QEMU
if not exist "%QEMU%" (
    echo ERROR: QEMU not found at "%QEMU%"
    pause
    goto MENU
)

set ISO=out\kOSeki.iso
set IMG=fat32.img

if not exist "%ISO%" (
    echo ERROR: ISO file not found: %ISO%
    pause
    exit /b 1
)

if not exist "%IMG%" (
    echo ERROR: disk image not found: %IMG%
    pause
    exit /b 1
)

echo target: %QEMU%
echo BIBOO'S HERE TO PLAY!! :D

if /I "%~1"=="whpx" goto RUN_WHPX

:RUN_NORMAL
"%QEMU%" ^
  -boot d ^
  -cdrom "%ISO%" ^
  -drive file="%IMG%",format=raw,if=ide ^
  -m 64 ^
  -netdev user,id=u1,hostfwd=udp::1234-:1234 ^
  -device e1000,netdev=u1 ^
  -audiodev dsound,id=snd0,out.frequency=44100 ^
  -device AC97,audiodev=snd0
goto DONE

:RUN_WHPX
"%QEMU%" ^
  -accel whpx,kernel-irqchip=off ^
  -boot d ^
  -cdrom "%ISO%" ^
  -drive file="%IMG%",format=raw,if=ide ^
  -m 64 ^
  -netdev user,id=u1,hostfwd=udp::1234-:1234 ^
  -device e1000,netdev=u1 ^
  -audiodev dsound,id=snd0,out.frequency=44100 ^
  -device AC97,audiodev=snd0
goto DONE

:DONE
echo.
echo 4'7 out...
pause
exit /b