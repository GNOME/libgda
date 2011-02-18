; NSIS script to install GdaBrowser

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "GdaBrowser"
!define PRODUCT_PUBLISHER "Gnome-Db"
!define PRODUCT_WEB_SITE "http://www.gnome-db.org"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\gda-browser-4.0.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor lzma

; MUI 1.67 compatible ------
!include "MUI.nsh"
!include "config.nsh"
!include "core.nsh"
!include "prov_bdb.nsh"
!include "prov_mysql.nsh"
!include "prov_postgresql.nsh"
!include "prov_web.nsh"
!include "prov_mdb.nsh"
!include "prov_oracle.nsh"
!include "prov_sqlite.nsh"
!include "uninst.nsh"

; MUI Settings
!define MUI_ABORTWARNING
;!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_ICON "gda-browser.ico"
!define MUI_UNICON "gda-browser.ico"

; Language Selection Dialog Settings
!define MUI_LANGDLL_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_LANGDLL_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "NSIS:Language"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "gpl.rtf"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\gda-browser-4.0.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "French"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "GdaBrowserSetup.exe"
InstallDir "$PROGRAMFILES\GdaBrowser"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd


Section -AdditionalIcons
  SetOutPath $INSTDIR
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\GdaBrowser\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\GdaBrowser\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\bin\gda-browser-4.0.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\gda-browser-4.0.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd


LangString DESC_core ${LANG_ENGLISH} "Main features"
LangString DESC_core ${LANG_FRENCH} "Fonctions principales"
LangString DESC_prov_mysql ${LANG_ENGLISH} "MySQL database provider"
LangString DESC_prov_mysql ${LANG_FRENCH} "Fournisseur pour les bases de données MySQL"
LangString DESC_prov_bdb ${LANG_ENGLISH} "Berkeley DB database provider"
LangString DESC_prov_bdb ${LANG_FRENCH} "Fournisseur pour les bases de données Berkeley DB"
LangString DESC_prov_mdb ${LANG_ENGLISH} "Ms Access database provider"
LangString DESC_prov_mdb ${LANG_FRENCH} "Fournisseur pour les bases de données MS Access"
LangString DESC_prov_postgresql ${LANG_ENGLISH} "PostgreSQL database provider"
LangString DESC_prov_postgresql ${LANG_FRENCH} "Fournisseur pour les bases de données PostgreSQL"
LangString DESC_prov_oracle ${LANG_ENGLISH} "Oracle database provider"
LangString DESC_prov_oracle ${LANG_FRENCH} "Fournisseur pour les bases de données Oracle"
LangString DESC_prov_sqlite ${LANG_ENGLISH} "Sqlite database provider"
LangString DESC_prov_sqlite ${LANG_FRENCH} "Fournisseur pour les bases de données Sqlite"
LangString DESC_prov_web ${LANG_ENGLISH} "Provider for database accessed through a web server"
LangString DESC_prov_web ${LANG_FRENCH} "Fournisseur pour les bases de données via un serveur web"


; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} $(DESC_core)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} $(DESC_prov_mysql)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} $(DESC_prov_bdb)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC04} $(DESC_prov_mdb)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC05} $(DESC_prov_postgresql)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC06} $(DESC_prov_oracle)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC07} $(DESC_prov_sqlite)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC08} $(DESC_prov_web)
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
!insertmacro MUI_UNGETLANGUAGE
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

