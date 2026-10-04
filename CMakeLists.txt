cmake_minimum_required(VERSION 3.20)
project(JkaExport LANGUAGES CXX)

# ------------------------------------------------------------
#  JKA Export: Ghoul2 (GLM/GLA) direkt aus 3ds Max.
#    g2export  (C++20, ohne SDK)  g2c-Schreiber + eigene Schicht g2export.cpp
#    JkaExport.dle (C++17, SDK)   Max-Schicht und Fenster
# ------------------------------------------------------------
set(MAX_VERSION "0" CACHE STRING "3ds Max Jahrgang, z.B. 2027")
set(3DSMAX_SDK_DIR "" CACHE PATH "3ds Max SDK (maxsdk-Ordner)")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "32-Bit-Bau. Es braucht x64:  cmake -S . -B build -A x64")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
if(MSVC)
    add_compile_options(/guard:cf)
    add_link_options(/guard:cf)
    add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
endif()

add_library(g2export STATIC
    src/g2export.h src/g2export.cpp
    vendor/g2c/src/mdxa.cpp vendor/g2c/src/mdxm.cpp
    vendor/g2c/src/compress.cpp vendor/g2c/src/parallel_stub.cpp)
target_include_directories(g2export PUBLIC src PRIVATE vendor/g2c/include)
set_target_properties(g2export PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(g2export PRIVATE _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(g2export PRIVATE /EHsc /utf-8 /W3)
endif()

if(NOT EXISTS "${3DSMAX_SDK_DIR}/include/max.h")
    message(FATAL_ERROR "3ds Max SDK nicht gefunden unter: ${3DSMAX_SDK_DIR}")
endif()
add_library(JkaExport SHARED
    src/jkaexport.h src/jkaexport_dll.cpp src/jkaexport_max.cpp
    src/jkaexport_fenster.h src/jkaexport_fenster.cpp src/jkaexport_ui.h
    src/jkaexport_res.h src/jkaexport.rc jkaexport.def)
set_target_properties(JkaExport PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
    SUFFIX ".dle" PREFIX "" OUTPUT_NAME "JkaExport"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
target_include_directories(JkaExport PRIVATE src)
target_include_directories(JkaExport SYSTEM PRIVATE "${3DSMAX_SDK_DIR}/include")
target_link_directories(JkaExport PRIVATE "${3DSMAX_SDK_DIR}/lib/x64/Release")
target_link_libraries(JkaExport PRIVATE g2export
    core.lib maxutil.lib bmm.lib Geom.lib mesh.lib Maxscrpt.lib MNMath.lib Paramblk2.lib
    comctl32.lib ole32.lib uuid.lib shell32.lib dwmapi.lib uxtheme.lib)
target_compile_definitions(JkaExport PRIVATE _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    # /Zp8 ist beim Max-SDK Pflicht.
    target_compile_options(JkaExport PRIVATE /EHsc /Zp8 /bigobj /utf-8 /Zc:__cplusplus /external:W0 /W4 /wd4100 /wd4458)
    target_link_options(JkaExport PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()
message(STATUS "JkaExport: Max ${MAX_VERSION}")
