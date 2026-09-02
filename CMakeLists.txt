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
  src/settingsdialog.cpp
  src/clockwindow.cpp
  src/config.h
  src/embedded.h
  src/face.h
  src/render.h
  src/colorbutton.h
  src/settingsdialog.h
  src/clockwindow.h
)

target_link_libraries(vclock PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Svg
)

set_target_properties(vclock PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "vclock"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.vclock.vclock"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

include(GNUInstallDirs)
install(TARGETS vclock
  BUNDLE  DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
