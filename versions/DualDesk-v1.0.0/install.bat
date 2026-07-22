cd C:\Users\VICTUS\DualDesk\versions\DualDesk-v1.0.0

echo @echo off > install.bat
echo echo ======================================== >> install.bat
echo echo     DUALDESK INSTALLER                 >> install.bat
echo echo ======================================== >> install.bat
echo echo. >> install.bat
echo echo [1/3] Installing EitherMouse... >> install.bat
echo start /wait "EitherMouse" "EitherMouse Setup.exe" /S >> install.bat
echo echo [2/3] Installing Driver... >> install.bat
echo copy dualdesk.sys C:\Windows\System32\drivers\ >> install.bat
echo sc create DualDesk type= kernel binPath= \SystemRoot\System32\drivers\dualdesk.sys >> install.bat
echo sc start DualDesk >> install.bat
echo echo [3/3] Done! >> install.bat
echo echo. >> install.bat
echo echo Run DualDesk.exe as Administrator >> install.bat
echo pause >> install.bat