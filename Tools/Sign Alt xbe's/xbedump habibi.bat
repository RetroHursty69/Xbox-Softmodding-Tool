@echo off

:main
echo;
echo                     浜様様様様様様様様様様様様様様様様様融
echo                     �                                    �
echo                     �        Sign edited xbe files       �
echo                     �                                    �
echo                     藩様様様様様様様様様様様様様様様様様夕
echo              浜様様様様様様様様様様様様様様様様様様様様様様様様融
echo              �                                                  �
echo              �      Put the edited xbe file in this folder      �
echo              �   and enter the name of the xbe file to Sign...  �
echo              �                                                  �
echo              藩様様様様様様様様様様様様様様様様様様様様様様様様夕
echo;
::timeout /t 2 >NUL

for /f "tokens=*" %%a in ('dir /b /s "disabled.xbe"') do (
xbedump "%%a" -habibi
echo:
echo  Disabled.xbe
echo  %%~pa
echo:
del "%%a"
ren out.xbe disabled.xbe >NUL
move "disabled.xbe" "%%~pa" >NUL
timeout /t 0 >NUL
)
for /f "tokens=*" %%a in ('dir /b /s "enabled.xbe"') do (
xbedump "%%a" -habibi
echo:
echo  Enabled.xbe
echo  %%~pa
echo:
del "%%a"
ren out.xbe enabled.xbe >NUL
move "enabled.xbe" "%%~pa" >NUL
timeout /t 0 >NUL
)