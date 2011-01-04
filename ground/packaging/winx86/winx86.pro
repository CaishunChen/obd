include(../../openpilotgcs.pri)

TEMPLATE = subdirs
SUBDIRS =

# Some Windows packaging magic (for release build only)
equals(copydata, 1):win32:CONFIG(release, debug|release) {

    # Root of SVN working copy (to get revision info from)
    SVN_SOURCE_ROOT  = \"$${_PRO_FILE_PWD_}/../..\"

    # Add Qt version to the build info.
    #VERSION_OUT = version.mk
    #version.commands += @echo QT_VERSION = $${QT_VERSION} > $$VERSION_OUT $$addNewline()
    #version.commands += @echo QT_MAJOR_VERSION = $${QT_MAJOR_VERSION} >> $$VERSION_OUT $$addNewline()
    #version.commands += @echo QT_MINOR_VERSION = $${QT_MINOR_VERSION} >> $$VERSION_OUT $$addNewline()
    #version.commands += @echo QT_PATCH_VERSION = $${QT_PATCH_VERSION} >> $$VERSION_OUT $$addNewline()
    #version.target = $${VERSION_OUT}.dummy
    #QMAKE_EXTRA_TARGETS += version
    #force.depends += version

    # Check for SubWCRev.exe executable required to get some useful SVN repository info.
    # For example, currently checked out SVN revision (highest for the working copy).
    # SubWCRev is a part of TortoiseSVN client distribution:
    # http://tortoisesvn.net/
    # SubWCRev is also available separately:
    # http://sourceforge.net/projects/tortoisesvn/files/Tools/1.6.7/SubWCRev-1.6.7.18415.msi/download

    # Default location is TortoiseSVN bin folder.
    SUBWCREV_EXE = $$targetPath(\"$$(ProgramFiles)/TortoiseSVN/bin/SubWCRev.exe\")

    exists($$SUBWCREV_EXE) {
        message("SubWCRev found: $${SUBWCREV_EXE}")
        SVNINFO_TPL = svninfo.tpl
        SVNINFO_MK  = svninfo.mk

        # extract SVN info from local copy for general use.
        #svninfo.commands += -@$(DEL_FILE) $$SVNINFO_MK $$addNewline()
        svninfo.commands += $$SUBWCREV_EXE $$SVN_SOURCE_ROOT $$SVNINFO_TPL $$SVNINFO_MK $$addNewline()
        svninfo.target = $${SVNINFO_MK}.dummy
        QMAKE_EXTRA_TARGETS += svninfo
        force.depends += svninfo
    } else {
        message("SubWCRev not found, SVN info is not available")
        SUBWCREV_EXE =
    }

    # Generate NSIS installer version info based on SVN working copy.
    # Currently used X.Y.Z.R version format where
    # X.Y.Z is a Qt version, R is an SVN revision.
    exists($$SUBWCREV_EXE) {
        NSIS_TPL = $$targetPath(nsis/OpenPilotGCS.tpl)
        NSIS_NSH = $$targetPath(nsis/OpenPilotGCS.nsh)
        nsis.commands += \
            @echo $$LITERAL_HASH > $$NSIS_TPL $$addNewline() \
            @echo $$LITERAL_HASH This file contains defaults and will be rebuilt automatically >> $$NSIS_TPL $$addNewline() \
            @echo $$LITERAL_HASH >> $$NSIS_TPL $$addNewline() \
            @echo ; >> $$NSIS_TPL $$addNewline() \
            @echo ; Installer file name >> $$NSIS_TPL $$addNewline() \
            @echo OutFile \"OpenPilotGCS-\$\$WCREV\$\$-install.exe\" >> $$NSIS_TPL $$addNewline() \
            @echo ; >> $$NSIS_TPL $$addNewline() \
            @echo ; Installer version info >> $$NSIS_TPL $$addNewline() \
            @echo VIProductVersion \"$${QT_VERSION}.\$\$WCREV\$\$\" >> $$NSIS_TPL $$addNewline() \
            @echo VIAddVersionKey \"FileVersion\" \"$${QT_VERSION}.\$\$WCREV\$\$\" >> $$NSIS_TPL $$addNewline() \
            @echo VIAddVersionKey \"Comments\" \
                \"OpenPilot GCS Installer. Product revision \$\$WCREV\$\$ committed on \$\$WCDATE\$\$. \
                Built on \$\$WCNOW\$\$ from \$\$WCURL\$\$ using Qt $${QT_VERSION}.\" >> $$NSIS_TPL $$addNewline() \
            $$SUBWCREV_EXE $$SVN_SOURCE_ROOT $$NSIS_TPL $$NSIS_NSH $$addNewline() \
            -@$(DEL_FILE) $$NSIS_TPL $$addNewline()
        nsis.target = $${NSIS_NSH}.dummy
        QMAKE_EXTRA_TARGETS += nsis
        force.depends += nsis
    }

    # Redefine FORCE target to collect data every time
    force.target = FORCE
    QMAKE_EXTRA_TARGETS += force
}