; Windows installer for keen4revamped. Built by release.yml with NSIS (makensis
; runs on Linux too):
;
;   makensis -DVERSION=1.2.0 -DSRCDIR=path/to/package -DOUTFILE=setup.exe keen4revamped.nsi
;
; SRCDIR is the unpacked windows-x64 release folder (keen4.exe, keen4-extract.exe,
; README.md, docs/, tools/). Game data is never part of it: players point the game
; at their own copy of Keen 4, so the installer neither asks for nor writes a data path.

Unicode true
!include MUI2.nsh
!include x64.nsh

!ifndef VERSION
  !error "Pass -DVERSION=x.y.z"
!endif
!ifndef SRCDIR
  !error "Pass -DSRCDIR=path to the windows-x64 package folder"
!endif
!ifndef OUTFILE
  !define OUTFILE "keen4revamped-${VERSION}-windows-x64-setup.exe"
!endif

!define APPNAME "keen4revamped"
!define DISPLAYNAME "Commander Keen 4 (keen4revamped)"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
!define SHCNE_CREATE 0x00000002
!define SHCNE_ASSOCCHANGED 0x08000000
!define SHCNF_PATHW 0x0005

Name "${DISPLAYNAME}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "FileDescription" "${DISPLAYNAME} installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "keen4revamped contributors"

!define MUI_ICON "..\icon\keen4revamped.ico"
!define MUI_UNICON "..\icon\keen4revamped.ico"
!define MUI_ABORTWARNING

!define MUI_FINISHPAGE_TEXT "$(FinishText)"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.md"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(OpenReadme)"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "PortugueseBR"

LangString FinishText ${LANG_ENGLISH} "The game needs your own copy of Commander Keen 4 v1.4 EGA (KEEN4E.EXE and the .CK4 files, for example from the Steam release). It is not included.$\r$\n$\r$\nREADME.md explains where keen4 looks for those files and how to point it at them with --data."
LangString FinishText ${LANG_PORTUGUESEBR} "O jogo precisa da sua própria cópia de Commander Keen 4 v1.4 EGA (KEEN4E.EXE e os arquivos .CK4, por exemplo da versão da Steam). Ela não vem incluída.$\r$\n$\r$\nO README.md explica onde o keen4 procura esses arquivos e como indicá-los com --data."
LangString OpenReadme ${LANG_ENGLISH} "Open README.md"
LangString OpenReadme ${LANG_PORTUGUESEBR} "Abrir o README.md"
LangString SecDesktopName ${LANG_ENGLISH} "Desktop shortcut"
LangString SecDesktopName ${LANG_PORTUGUESEBR} "Atalho na área de trabalho"
LangString DescMain ${LANG_ENGLISH} "The game, keen4-extract, the art tools and the docs."
LangString DescMain ${LANG_PORTUGUESEBR} "O jogo, o keen4-extract, as ferramentas de arte e a documentação."
LangString DescDesktop ${LANG_ENGLISH} "Put a shortcut to the game on the desktop."
LangString DescDesktop ${LANG_PORTUGUESEBR} "Coloca um atalho para o jogo na área de trabalho."

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "keen4revamped needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "keen4revamped" SecMain
  SectionIn RO
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File /r "${SRCDIR}\*.*"
  File "/oname=keen4revamped.ico" "..\icon\keen4revamped.ico"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Shortcuts start in the install folder, like running keen4.exe from there.
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${DISPLAYNAME}.lnk" "$INSTDIR\keen4.exe" "" "$INSTDIR\keen4.exe" 0
  CreateShortcut "$SMPROGRAMS\${APPNAME}\README.lnk" "$INSTDIR\README.md"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall keen4revamped.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${DISPLAYNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "keen4revamped"
  WriteRegStr HKLM "${UNINSTKEY}" "URLInfoAbout" "https://github.com/talkedu/keen4revamped"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\keen4.exe,0"
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  SectionGetSize ${SecMain} $0
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" $0
SectionEnd

Section "$(SecDesktopName)" SecDesktop
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  CreateShortcut "$DESKTOP\${DISPLAYNAME}.lnk" "$INSTDIR\keen4.exe" "" "$INSTDIR\keen4.exe" 0
  ; Explorer does not always notice new files on the Public desktop until the
  ; desktop is refreshed (F5), so tell it about the shortcut.
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_CREATE}, i ${SHCNF_PATHW}, w "$DESKTOP\${DISPLAYNAME}.lnk", p 0)'
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, p 0, p 0)'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} $(DescMain)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DescDesktop)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Removes only what the installer put there. Anything else in the folder (for
; example game files or art a player copied in) stays, and so do the saved
; games and configuration in %APPDATA%\keen4revamped.
Section "Uninstall"
  SetShellVarContext all

  Delete "$DESKTOP\${DISPLAYNAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APPNAME}"

  Delete "$INSTDIR\keen4.exe"
  Delete "$INSTDIR\keen4-extract.exe"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\keen4revamped.ico"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\tools"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "Software\${APPNAME}"
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, p 0, p 0)'
SectionEnd
