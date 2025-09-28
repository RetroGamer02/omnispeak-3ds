@echo off
cls
echo           ÚÉÍËÍ»¿ÚË   Ë¿ÚËÍÍÍË¿  ÚËÍÍÍË¿ÚË  ÉÍ¿  ÚËÍÍÍË¿ÚË   Ë¿ÚË   Ë¿
echo              º   ÃÎÍÍÍÎ´ÃÎÍÍ     ³º     ÃÎÍÍ¹    ³º ÉÍË¿³º   º³ÀÈÍÍÍÎ´
echo              Ê   ÀÊ   ÊÙÀÊÍÍÍÊÙ  ÀÊÍÍÍÊÙÀÊ  ÈÍÙ  ÀÊÍÍÍÊÙÀÊÍÍÍÊÙÀÈÍÍÍÊÙ
echo.
echo                             -=-  K E E N  5 . 5  -=-
echo.
echo                -=-  A N  E N H A N C E D  L E V E L  P A C K  -=-
echo.
echo                         -=-  R E L E A S E  F I V E  -=-
echo.
echo.
if not exist keen5e.exe goto noexe
echo                      Press any key to load the level pack.
pause > nul
ck5patch ck55.pat
pause > nul
goto end
:noexe
echo       You don't have a Keen 5 exe in here! What am I supposed to do?!
pause > nul
:end
echo.
