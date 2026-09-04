cmake_minimum_required(VERSION 3.16)
project(vclock VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

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
  src/clockmanager.cpp
  src/manageclocksdialog.cpp
  src/settingsdialog.cpp
  src/clockwindow.cpp
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
  src/clockmanager.h
  src/manageclocksdialog.h
  src/settingsdialog.h
  src/clockwindow.h
  src/windowgroup.h
)

target_link_libraries(vclock PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Svg
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
endif()

set_target_properties(vclock PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "vclock"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.vclock.vclock"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# The program draws its own icon and so ships no icon file; distro/macos builds
# one from vclock.svg and points this at it.  Without it the bundle gets the
# generic application icon, which is only a cosmetic loss.
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
endif()

include(distro/packaging.cmake)
