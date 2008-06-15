@echo off

REM Values to change include VERSION and INSTALLDIR, both below.

REM The subdirectory to install into
SET INSTALLDIR="Gda-4.0"

if NOT "%1"=="" SET VERSION="%1"
if NOT "%1"=="" GOTO GOT_VERSION

REM The full version number of the build in XXXX.XX.XX format
SET VERSION="3.99.3"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

echo.
echo Compiling individual .wxs files

for %%f in (*.wxs) do (
candle -nologo -dVERSION=%VERSION% -dINSTALLDIR=%INSTALLDIR% "%%f"
IF ERRORLEVEL 1 GOTO ERR_HANDLER
)

echo.
echo Creating Gda merge module...
light -nologo -out gda-module-%VERSION%.msm gda-module.wixobj glib-fragment.wixobj gda-share.wixobj bdb.wixobj mdb.wixobj mysql.wixobj postgres.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Creating Gda-sql installer
light -nologo -out gda-sql-%VERSION%.msi -dVERSION=%VERSION%  -ext WixUIExtension -cultures:en-us gda-sql.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Done!
GOTO EXIT

:ERR_HANDLER
echo.
echo Aborting build!
GOTO EXIT

:EXIT
