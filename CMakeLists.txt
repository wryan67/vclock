cmake_minimum_required(VERSION 3.16)
project(vclock VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Network)

add_executable(vclock WIN32 MACOSX_BUNDLE
  src/main.cpp
  src/config.cpp
  src/face.cpp
  src/render.cpp
  src/colorbutton.cpp
  src/colorpicker.cpp
  src/icons.cpp
  src/registry.cpp
  src/autostart.cpp
  src/jumplist.cpp
  src/trayicon.cpp
  src/singleinstance.cpp
  src/clockmanager.cpp
  src/manageclocksdialog.cpp
  src/settingsdialog.cpp
  src/clockwindow.cpp
  src/timetip.cpp
  src/windowgroup.cpp
  src/config.h
  src/embedded.h
  src/face.h
  src/render.h
  src/colorbutton.h
  src/colorpicker.h
  src/icons.h
  src/registry.h
  src/autostart.h
  src/jumplist.h
  src/trayicon.h
  src/singleinstance.h
  src/clockmanager.h
  src/manageclocksdialog.h
  src/settingsdialog.h
  src/clockwindow.h
  src/timetip.h
  src/windowgroup.h
)

target_link_libraries(vclock PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Svg
  Qt${QT_VERSION_MAJOR}::Network
)

# On X11 every window a process opens shares one window group, and window
# managers stack a group together: one clock set to stay on top would carry the
# others up with it.  Breaking that needs to reach past Qt to the WM_HINTS
# record, which is a few lines of xcb.  The library is already there underneath
# Qt's own X11 plugin; only its headers are an extra build dependency.
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
  target_link_libraries(vclock PRIVATE PkgConfig::XCB)
  target_compile_definitions(vclock PRIVATE VCLOCK_HAVE_XCB)
  # And DBus, for the one question the tray icon has to ask before it says
  # anything to Qt: whether this desktop has anywhere to put a tray icon.  Part
  # of Qt Base like the rest, so it is a component rather than a dependency of
  # its own.
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS DBus)
  target_link_libraries(vclock PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
endif()

# The program draws its own icon and so ships no icon file -- except for the
# two platforms that show an icon for the executable before it has run.  Both
# are built from vclock.svg by distro/make-icons.sh and committed, because
# neither can be produced from the SVG on the machine that builds them: a
# Windows box has no SVG rasteriser, and the Windows package is cross-compiled
# from a container that has no Windows resource compiler beyond windres.
if(WIN32)
  # project() above lists CXX only, so the resource compiler is not enabled and
  # a .rc would be passed to the linker as an unknown file.
  enable_language(RC)
  target_sources(vclock PRIVATE distro/windows/vclock.rc)
  # The taskbar menu is COM: the shell link and destination list objects live
  # in ole32 and shell32, and the property that carries an entry's text is
  # read and written through propsys.  shlwapi is there for SHStrDupW, which
  # the inline that packs a string into a PROPVARIANT calls.
  target_link_libraries(vclock PRIVATE ole32 shell32 propsys shlwapi)
endif()

set_target_properties(vclock PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "vclock"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.vclock.vclock"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# distro/macos/package.sh regenerates this with iconutil, which is the
# authoritative tool and always present on a Mac; the committed file is what a
# plain cmake build gets, so that building by hand does not quietly produce a
# bundle wearing the generic application icon.
if(APPLE AND NOT VCLOCK_MACOS_ICON)
  set(VCLOCK_MACOS_ICON ${CMAKE_CURRENT_SOURCE_DIR}/distro/macos/vclock.icns)
endif()

if(APPLE AND VCLOCK_MACOS_ICON)
  target_sources(vclock PRIVATE ${VCLOCK_MACOS_ICON})
  get_filename_component(_vclock_icon_name ${VCLOCK_MACOS_ICON} NAME)
  set_source_files_properties(${VCLOCK_MACOS_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(vclock PROPERTIES
    MACOSX_BUNDLE_ICON_FILE ${_vclock_icon_name})
endif()

include(GNUInstallDirs)

# Directories inside a package must not be group writable; without this they
# inherit the build user's umask, which on a developer machine is usually 002.
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE)

install(TARGETS vclock
  BUNDLE  DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# A packaged install gets a real icon file and a menu entry.  The program draws
# its own icon at runtime and so needs neither to work, but a package that puts
# nothing in the menu is one you can only start from a terminal, and the
# autostart entry has somewhere to point instead of writing a PNG of its own.
if(UNIX AND NOT APPLE)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/vclock.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
  )
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/distro/vclock.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
  )

  # A manual page, because /usr/bin is a place where "man vclock" is a
  # reasonable thing to try and getting nothing back is a poor answer.

  # Debian requires every package to state its licence in
  # /usr/share/doc/<package>/copyright and to ship a changelog beside it, and
  # counts a package without them as broken.  Both are installed for every
  # Unix package rather than for the Debian one alone: RPM states the licence
  # in its own header as well, but a copy on disk is no burden there and the
  # alternative is a rule that has to know which generator is running, which is
  # not something a CMake configure step can be told.
  #
  # The changelog and the manual page are both installed compressed, because
  # Debian requires it of both and RPM would compress them itself anyway.  gzip
  # is asked for the best compression it has and for no timestamp, so that the
  # same source always produces byte-identical output; a package that differs
  # from build to build cannot be checked against another copy of itself.
  #
  # CPack does not do this for us -- it copies what it is given -- so it is done
  # here, once, in a rule each file goes through.
  set(_vclock_compressed)
  foreach(pair
      "${CMAKE_CURRENT_SOURCE_DIR}/distro/deb/changelog|changelog.gz"
      "${CMAKE_CURRENT_SOURCE_DIR}/distro/vclock.1|vclock.1.gz")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 src)
    list(GET pair 1 gz)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${gz}
      COMMAND ${CMAKE_COMMAND} -E env gzip -9nc ${src} > ${CMAKE_CURRENT_BINARY_DIR}/${gz}
      DEPENDS ${src}
      COMMENT "Compressing ${gz}"
      VERBATIM
    )
    list(APPEND _vclock_compressed ${CMAKE_CURRENT_BINARY_DIR}/${gz})
  endforeach()
  add_custom_target(vclock-docs ALL DEPENDS ${_vclock_compressed})

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/distro/deb/copyright
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/changelog.gz
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vclock.1.gz
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
  )
endif()

include(distro/packaging.cmake)
