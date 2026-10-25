cmake_minimum_required(VERSION 3.21)

project(ScreenCast VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_compile_options(/utf-8 /W3 /EHsc)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network OpenGLWidgets)

# ---- FFmpeg ----
set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}" CACHE PATH "FFmpeg install root")
if(NOT FFMPEG_ROOT)
    set(FFMPEG_ROOT "E:/deepseek/tools/ffmpeg-9.0.2-full_build-shared")
endif()

find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h HINTS "${FFMPEG_ROOT}/include")
find_library(AVCODEC_LIB NAMES avcodec HINTS "${FFMPEG_ROOT}/lib")
find_library(AVUTIL_LIB NAMES avutil HINTS "${FFMPEG_ROOT}/lib")
find_library(SWSCALE_LIB NAMES swscale HINTS "${FFMPEG_ROOT}/lib")

if(NOT FFMPEG_INCLUDE_DIR OR NOT AVCODEC_LIB OR NOT AVUTIL_LIB OR NOT SWSCALE_LIB)
    message(FATAL_ERROR "FFmpeg not found. Pass -DFFMPEG_ROOT=<path containing include/ and lib/>")
endif()
message(STATUS "FFmpeg: ${FFMPEG_ROOT}")

# ---- sources ----
set(SOURCES
    src/main.cpp
    src/core/Config.cpp
    src/net/Protocol.cpp
    src/net/StreamServer.cpp
    src/net/StreamClient.cpp
    src/capture/GdiCapturer.cpp
    src/capture/DxgiCapturer.cpp
    src/codec/VideoEncoder.cpp
    src/codec/VideoDecoder.cpp
    src/render/VideoWidget.cpp
    src/pipeline/CaptureWorker.cpp
    src/app/MainWindow.cpp
    src/app/SelfTest.cpp
)

add_executable(ScreenCast ${SOURCES})

target_include_directories(ScreenCast PRIVATE src ${FFMPEG_INCLUDE_DIR})

target_link_libraries(ScreenCast PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGLWidgets
    ${AVCODEC_LIB}
    ${AVUTIL_LIB}
    ${SWSCALE_LIB}
)

if(WIN32)
    target_link_libraries(ScreenCast PRIVATE d3d11 dxgi)
endif()

# ---- 运行期依赖：Qt DLL + FFmpeg DLL 拷到输出目录 ----
if(WIN32)
    file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    add_custom_command(TARGET ScreenCast POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFMPEG_DLLS} "$<TARGET_FILE_DIR:ScreenCast>"
        COMMENT "Copy FFmpeg runtime DLLs"
    )

    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    if(EXISTS "${QT_BIN_DIR}/windeployqt.exe")
        add_custom_command(TARGET ScreenCast POST_BUILD
            COMMAND "${QT_BIN_DIR}/windeployqt.exe" --no-translations
                    --no-system-d3d-compiler "$<TARGET_FILE:ScreenCast>"
            COMMENT "Deploy Qt runtime"
        )
    endif()
endif()
