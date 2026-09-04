# Packaging.  Included from the top-level CMakeLists; sets up the CPack
# generators used by the recipes under distro/.
#
# Only the Linux package formats are driven from here.  The Windows installer
# needs a tree of bundled Qt DLLs assembled first and the macOS disk image needs
# tools that only exist on a Mac, so both are built by their own recipe scripts
# rather than by CPack.

set(CPACK_PACKAGE_NAME "vclock")
set(CPACK_PACKAGE_VENDOR "vclock")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A transparent analog desktop clock")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/wryan/vclock")
set(CPACK_PACKAGE_CONTACT "vclock <vclock@localhost>")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "vclock")
set(CPACK_STRIP_FILES ON)

# CPack otherwise packages the whole source tree for its source generators.
set(CPACK_SOURCE_GENERATOR "")

set(CPACK_PACKAGE_DESCRIPTION
"vclock draws an analog clock on the desktop with no window frame and a
transparent background, so only the clock face itself is visible.  Faces,
colours, hands and marks are configurable per clock, and any number of clocks
can be shown at once.")

# ------------------------------------------------------------------ Debian

# Worked out from what the binary actually links against, which is better than a
# hand-written list that goes stale the first time a Qt module is added.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  "${CMAKE_CURRENT_LIST_DIR}/deb/postinst;${CMAKE_CURRENT_LIST_DIR}/deb/postrm")

# ---------------------------------------------------------------------- RPM

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_LIST_DIR}/rpm/post")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_LIST_DIR}/rpm/postun")

# These belong to the filesystem and desktop-file packages; claiming ownership
# of them makes the package conflict with anything else that ships an icon.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
  /usr/share/applications
  /usr/share/icons
  /usr/share/icons/hicolor
  /usr/share/icons/hicolor/scalable
  /usr/share/icons/hicolor/scalable/apps)

include(CPack)
