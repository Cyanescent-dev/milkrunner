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
    LANGUAGES CXX
)

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MILK_RUNNER_ENABLE_PROJECTM "Build the optional libprojectM renderer when projectM4 is available" ON)
option(MILK_RUNNER_ENABLE_QT_MULTIMEDIA "Build live audio capture when Qt Multimedia is available" ON)

if(APPLE)
    set(AUDIO_DEPENDENCY_DEFAULT OFF)
else()
    set(AUDIO_DEPENDENCY_DEFAULT ON)
endif()
option(MILK_RUNNER_ENABLE_FILE_AUDIO "FFmpeg file audio decoding" ${AUDIO_DEPENDENCY_DEFAULT})
option(MILK_RUNNER_ENABLE_LINUX_SYSTEM_AUDIO "PulseAudio-compatible output capture" ${AUDIO_DEPENDENCY_DEFAULT})
if(MILK_RUNNER_ENABLE_FILE_AUDIO OR (UNIX AND NOT APPLE AND MILK_RUNNER_ENABLE_LINUX_SYSTEM_AUDIO))
    find_package(PkgConfig QUIET)
endif()
if(MILK_RUNNER_ENABLE_FILE_AUDIO)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat>=59 libavcodec libavutil libswresample)
    elseif(APPLE)
        # Homebrew libraries also work without adding pkg-config to a Mac build host.
        find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h HINTS /opt/homebrew/opt/ffmpeg/include /usr/local/opt/ffmpeg/include REQUIRED)
        add_library(PkgConfig::FFMPEG INTERFACE IMPORTED)
        target_include_directories(PkgConfig::FFMPEG INTERFACE "${FFMPEG_INCLUDE_DIR}")
        foreach(component avformat avcodec avutil swresample)
            find_library(FFMPEG_${component}_LIBRARY NAMES ${component} HINTS /opt/homebrew/opt/ffmpeg/lib /usr/local/opt/ffmpeg/lib REQUIRED)
            target_link_libraries(PkgConfig::FFMPEG INTERFACE "${FFMPEG_${component}_LIBRARY}")
        endforeach()
    else()
        message(FATAL_ERROR "File audio requires pkg-config and FFmpeg development packages")
    endif()
endif()
if(UNIX AND NOT APPLE AND MILK_RUNNER_ENABLE_LINUX_SYSTEM_AUDIO)
    if(NOT PkgConfig_FOUND)
        message(FATAL_ERROR "Linux system audio requires pkg-config and libpulse-dev")
    endif()
    pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
endif()

set(MILK_RUNNER_LOCAL_PROJECTM_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/external/projectm-install")
if(EXISTS "${MILK_RUNNER_LOCAL_PROJECTM_PREFIX}/lib/cmake/projectM4/projectM4Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${MILK_RUNNER_LOCAL_PROJECTM_PREFIX}")
endif()

if(CMAKE_PREFIX_PATH)
    list(APPEND QT_ADDITIONAL_PACKAGES_PREFIX_PATH ${CMAKE_PREFIX_PATH})
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    OpenGL
    Qml
    Quick
    QuickControls2
)

set(MILK_RUNNER_HAS_QT_MULTIMEDIA OFF)
if(MILK_RUNNER_ENABLE_QT_MULTIMEDIA)
    find_package(Qt6 6.4 QUIET COMPONENTS Multimedia)
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

qt_standard_project_setup(REQUIRES 6.4)

set(MILK_RUNNER_APP_SOURCES
    src/main.cpp
    src/app/AutomationOptions.cpp
    src/app/AutomationOptions.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/SettingsStore.cpp
    src/app/SettingsStore.h
    src/audio/BufferedAudioInput.cpp
    src/audio/BufferedAudioInput.h
    src/audio/FileAudioInput.cpp
    src/audio/FileAudioInput.h
    src/audio/LinuxSystemAudioInput.cpp
    src/audio/LinuxSystemAudioInput.h
    src/audio/AudioInput.cpp
    src/audio/AudioInput.h
    src/audio/MacosSystemAudioInput.h
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

if(APPLE)
    list(APPEND MILK_RUNNER_APP_SOURCES src/audio/MacosSystemAudioInput.mm)
else()
    list(APPEND MILK_RUNNER_APP_SOURCES src/audio/MacosSystemAudioInput_unsupported.cpp)
endif()

qt_add_executable(MilkRunnerVisualizer
    MACOSX_BUNDLE
    ${MILK_RUNNER_APP_SOURCES}
)

qt_add_qml_module(MilkRunnerVisualizer
    URI MilkRunnerVisualizer
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
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
)

if(MILK_RUNNER_ENABLE_FILE_AUDIO)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE MILK_RUNNER_HAS_FFMPEG=1)
    target_link_libraries(MilkRunnerVisualizer PRIVATE PkgConfig::FFMPEG)
endif()
if(TARGET PkgConfig::PULSE)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE MILK_RUNNER_HAS_PULSE=1)
    target_link_libraries(MilkRunnerVisualizer PRIVATE PkgConfig::PULSE)
endif()

if(APPLE)
    target_link_libraries(MilkRunnerVisualizer PRIVATE
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework ScreenCaptureKit"
    )
    target_compile_definitions(MilkRunnerVisualizer PRIVATE __MACOSX_CORE__)
elseif(UNIX)
    find_package(ALSA REQUIRED)
    target_link_libraries(MilkRunnerVisualizer PRIVATE ALSA::ALSA pthread)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE __LINUX_ALSA__)
endif()

if(MILK_RUNNER_HAS_QT_MULTIMEDIA)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE MILK_RUNNER_HAS_QT_MULTIMEDIA=1)
    target_link_libraries(MilkRunnerVisualizer PRIVATE Qt6::Multimedia)
endif()

if(MILK_RUNNER_HAS_PROJECTM)
    target_compile_definitions(MilkRunnerVisualizer PRIVATE MILK_RUNNER_HAS_PROJECTM=1)
    target_link_libraries(MilkRunnerVisualizer PRIVATE libprojectM::projectM)
endif()

set_target_properties(MilkRunnerVisualizer PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.milkrunner.visualizer"
    MACOSX_BUNDLE_BUNDLE_NAME "Milk Runner Visualizer"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

if(APPLE)
    set_target_properties(MilkRunnerVisualizer PROPERTIES OUTPUT_NAME "Milk Runner Visualizer")
else()
    # The QML module creates a "MilkRunnerVisualizer/" directory in the build tree, so the binary needs another name.
    set_target_properties(MilkRunnerVisualizer PROPERTIES OUTPUT_NAME "milk-runner-visualizer")
endif()

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

if(BUILD_TESTING)
    qt_add_executable(milk_runner_automation_tests
        tests/automation_tests.cpp src/app/AutomationOptions.cpp src/presets/PresetScanner.cpp)
    target_include_directories(milk_runner_automation_tests PRIVATE src)
    target_link_libraries(milk_runner_automation_tests PRIVATE Qt6::Core)
    add_test(NAME milk_runner_automation_tests COMMAND milk_runner_automation_tests)
endif()

if(BUILD_TESTING AND MILK_RUNNER_ENABLE_FILE_AUDIO)
    qt_add_executable(milk_runner_audio_tests tests/audio_tests.cpp
        src/audio/AudioInput.cpp src/audio/AudioInput.h
        src/audio/BufferedAudioInput.cpp src/audio/BufferedAudioInput.h
        src/audio/FileAudioInput.cpp src/audio/FileAudioInput.h
        src/audio/LinuxSystemAudioInput.cpp src/audio/LinuxSystemAudioInput.h)
    target_include_directories(milk_runner_audio_tests PRIVATE src)
    target_compile_definitions(milk_runner_audio_tests PRIVATE MILK_RUNNER_HAS_FFMPEG=1)
    target_link_libraries(milk_runner_audio_tests PRIVATE Qt6::Core PkgConfig::FFMPEG)
    if(TARGET PkgConfig::PULSE)
        target_compile_definitions(milk_runner_audio_tests PRIVATE MILK_RUNNER_HAS_PULSE=1)
        target_link_libraries(milk_runner_audio_tests PRIVATE PkgConfig::PULSE)
    endif()
    add_test(NAME milk_runner_audio_tests COMMAND milk_runner_audio_tests)
    set_tests_properties(milk_runner_audio_tests PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME milk_runner_system_audio_tests COMMAND milk_runner_audio_tests --system)
    set_tests_properties(milk_runner_system_audio_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
if(BUILD_TESTING AND TARGET PkgConfig::PULSE)
    qt_add_executable(milk_runner_audio_recovery_tests tests/audio_recovery_tests.cpp
        src/audio/AudioInput.cpp src/audio/AudioInput.h
        src/audio/BufferedAudioInput.cpp src/audio/BufferedAudioInput.h
        src/audio/LinuxSystemAudioInput.cpp src/audio/LinuxSystemAudioInput.h)
    target_include_directories(milk_runner_audio_recovery_tests PRIVATE src)
    target_compile_definitions(milk_runner_audio_recovery_tests PRIVATE MILK_RUNNER_HAS_PULSE=1)
    target_link_libraries(milk_runner_audio_recovery_tests PRIVATE Qt6::Core PkgConfig::PULSE)
    add_test(NAME milk_runner_audio_recovery_tests COMMAND milk_runner_audio_recovery_tests)
    set_tests_properties(milk_runner_audio_recovery_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()
