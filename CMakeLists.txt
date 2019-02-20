cmake_minimum_required(VERSION 3.13)
project(ydisk_commander)

set(CMAKE_CXX_STANDARD 14)

add_executable(tests tests.cpp wfxplugin.h json11.cpp)

add_library(ydisk_commander SHARED library.cpp library.h httplib.h json11.cpp restclient.h restclient.cpp plugin_utils.h dialogs.h dialogs.cpp dialogs.cpp)
target_link_libraries(ydisk_commander pthread ssl crypto)
set_target_properties(ydisk_commander PROPERTIES PREFIX "" SUFFIX ".wfx")
