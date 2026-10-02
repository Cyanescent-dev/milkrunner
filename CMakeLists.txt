cmake_minimum_required(VERSION 3.24)

if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    endif()
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target macOS architectures")
    endif()
endif()

project(MilkRunnerVisualizer
    VERSION 0.1.0
    DESCRIPTION "A free MilkDrop-compatible visualizer frontend powered by a portable renderer abstraction"
    LANGUAGES CXX OBJCXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

option(MILK_RUNNER_ENABLE_PROJECTM "Build the optional libprojectM renderer when projectM4 is available" ON)
option(MILK_RUNNER_ENABLE_QT_MULTIMEDIA "Build live audio capture when Qt Multimedia is available" ON)

set(MILK_RUNNER_LOCAL_PROJECTM_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/external/projectm-install")
if(EXISTS "${MILK_RUNNER_LOCAL_PROJECTM_PREFIX}/lib/cmake/projectM4/projectM4Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${MILK_RUNNER_LOCAL_PROJECTM_PREFIX}")
endif()

if(CMAKE_PREFIX_PATH)
    list(APPEND QT_ADDITIONAL_PACKAGES_PREFIX_PATH ${CMAKE_PREFIX_PATH})
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    OpenGL
    Qml
    Quick
    QuickControls2
)

set(MILK_RUNNER_HAS_QT_MULTIMEDIA OFF)
if(MILK_RUNNER_ENABLE_QT_MULTIMEDIA)
    find_package(Qt6 6.5 QUIET COMPONENTS Multimedia)
    if(TARGET Qt6::Multimedia)
        set(MILK_RUNNER_HAS_QT_MULTIMEDIA ON)
        message(STATUS "Qt Multimedia found: enabling live audio input")
    else()
        message(STATUS "Qt Multimedia not found: live audio input disabled, demo audio remains available")
    endif()
endif()

set(MILK_RUNNER_HAS_PROJECTM OFF)
if(MILK_RUNNER_ENABLE_PROJECTM)
    find_package(projectM4 QUIET)
    if(projectM4_FOUND AND TARGET libprojectM::projectM)
        set(MILK_RUNNER_HAS_PROJECTM ON)
        message(STATUS "libprojectM found: enabling ProjectMRenderer")
    else()
        message(STATUS "libprojectM/projectM4 not found: building with DummyRenderer fallback")
    endif()
endif()

qt_standard_project_setup(REQUIRES 6.5)

set(MILK_RUNNER_APP_SOURCES
    src/main.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/SettingsStore.cpp
    src/app/SettingsStore.h
    src/audio/AudioInput.cpp
    src/audio/AudioInput.h
    src/audio/MacosSystemAudioInput.h
    src/audio/MacosSystemAudioInput.mm
    src/audio/DemoAudioInput.cpp
    src/audio/DemoAudioInput.h
    src/audio/QtAudioInput.cpp
    src/audio/QtAudioInput.h
    src/audio/ScionAudioInput.cpp
    src/audio/ScionAudioInput.h
    src/audio/rtmidi/RtMidi.cpp
    src/playlists/Playlist.cpp
    src/playlists/Playlist.h
    src/playlists/PlaylistManager.cpp
    src/playlists/PlaylistManager.h
    src/presets/PresetLibrary.cpp
    src/presets/PresetLibrary.h
    src/presets/PresetMetadata.h
    src/presets/PresetScanner.cpp
    src/presets/PresetScanner.h
    src/render/DummyRenderer.cpp
    src/render/DummyRenderer.h
    src/render/IVisualizerRenderer.h
    src/render/OpenGlVisualizerItem.cpp
    src/render/OpenGlVisualizerItem.h
    src/render/ProjectMRenderer.cpp
    src/render/ProjectMRenderer.h
)

qt_add_executable(MilkRunnerVisualizer
    MACOSX_BUNDLE
    ${MILK_RUNNER_APP_SOURCES}
)

qt_add_qml_module(MilkRunnerVisualizer
    URI MilkRunnerVisualizer
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/VisualizerView.qml
        src/ui/qml/PresetLibraryView.qml
        src/ui/qml/PlaylistEditor.qml
        src/ui/qml/SettingsView.qml
)

target_include_directories(MilkRunnerVisualizer
    PRIVATE
        src
        src/render
)

target_link_libraries(MilkRunnerVisualizer
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::OpenGL
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework ScreenCaptureKit"
)

target_compile_definitions(MilkRunnerVisualizer PRIVATE __MACOSX_CORE__)

if(MILK_RUNNER_HAS_QT_MULTIMEDIA)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE MILK_RUNNER_HAS_QT_MULTIMEDIA=1)
    target_link_libraries(MilkRunnerVisualizer PRIVATE Qt6::Multimedia)
endif()

if(MILK_RUNNER_HAS_PROJECTM)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE MILK_RUNNER_HAS_PROJECTM=1)
    target_link_libraries(MilkRunnerVisualizer PRIVATE libprojectM::projectM)
endif()

set_target_properties(MilkRunnerVisualizer PROPERTIES
    OUTPUT_NAME "Milk Runner Visualizer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.milkrunner.visualizer"
    MACOSX_BUNDLE_BUNDLE_NAME "Milk Runner Visualizer"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(milk_runner_core_tests
        tests/playlist_scanner_tests.cpp
        src/playlists/Playlist.cpp
        src/playlists/Playlist.h
        src/presets/PresetMetadata.h
        src/presets/PresetLibrary.cpp
        src/presets/PresetLibrary.h
        src/presets/PresetScanner.cpp
        src/presets/PresetScanner.h
    )
    target_include_directories(milk_runner_core_tests PRIVATE src)
    target_link_libraries(milk_runner_core_tests PRIVATE Qt6::Core)
    add_test(NAME milk_runner_core_tests COMMAND milk_runner_core_tests)
endif()
