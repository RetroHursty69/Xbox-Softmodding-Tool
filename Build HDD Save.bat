@Echo off & SetLocal EnableDelayedExpansion & Mode con:cols=56 lines=7 & Color 0B
Title Build HDD Save

::Build save files for Xbox.
Set "Winrar=%CD%\Tools\Winrar\winrar.exe"

CD "Files"
"%Winrar%" a -x*.db "..\Files.rar" "*.rar"
CD ..\
RD /S /Q "..\Test Build\UDATA"
XCopy /s /y /e "Save Folder\*" "..\Test Build\UDATA\21585554\000000000000\"
Move "Files.rar" "..\Test Build\UDATA\21585554\000000000000\softmod files\"
XCopy /s /y "Game Saves\Softmod\*" "..\Test Build\UDATA\21585554\"
Explorer "..\Test Build\UDATA\21585554\000000000000\"