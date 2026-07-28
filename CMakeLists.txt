cmake_minimum_required(VERSION 3.19)
project(demuxing_decoding_4 LANGUAGES C CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts OpenGL OpenGLWidgets)

qt_standard_project_setup()

qt_add_executable(demuxing_decoding_4
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    MyPacketQueue.h
    MyDemuxThread.h MyDemuxThread.cpp
    MyVideoDecodeThread.h MyVideoDecodeThread.cpp
    MyDisplayImgWidget.h
    MyAudioDecodeThread.h MyAudioDecodeThread.cpp
    MyAudioBufQueue.h
    MyYUV420POpenGLWidget.h
)

target_link_libraries(demuxing_decoding_4
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::OpenGL
        Qt::OpenGLWidgets
        Qt::Charts
)

include(GNUInstallDirs)

install(TARGETS demuxing_decoding_4
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET demuxing_decoding_4
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

set(MY_TARGET_NAME demuxing_decoding_4)
message(STATUS ">>> Linking FFmpeg dev lib")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    message(STATUS ">>> Building on Windows host")
    if(MSVC OR MINGW)
        message(STATUS ">>> The compiler is MSVC OR MINGW")
        # 1. 指定 FFmpeg 根目录
        set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ffmpeg-n8.0.1-34-gbfa334de42-win64-gpl-shared-8.0")
        message(STATUS ">>> FFMPEG_DIR = ${FFMPEG_DIR}")
        # 2. 添加 FFmpeg 头文件路径
        target_include_directories(${MY_TARGET_NAME}
            PRIVATE
                ${FFMPEG_DIR}/include
        )
        # 3. 添加 FFmpeg 库文件路径
        target_link_directories(${MY_TARGET_NAME}
            PRIVATE
                ${FFMPEG_DIR}/lib
        )
        # 5. 链接 FFmpeg 库（按需添加）
        target_link_libraries(${MY_TARGET_NAME}
            PRIVATE
                avformat
                avcodec
                avutil
                avfilter
                swscale
                swresample
                ws2_32  # Windows 系统库
        )
        # # 6. 自动复制 DLL 到输出目录（可选）
        # # 添加到“高级系统设置”可注释掉：D:\Codes\cppCode\FFmpegCode\ffmpeg-master-latest-win64-gpl-shared\bin
        # # 但是要注意只能有一个ffmpeg路径，否则cmd输入ffmpeg就会显示拒绝访问
        # add_custom_command(TARGET ${MY_TARGET_NAME} POST_BUILD
        #         COMMAND ${CMAKE_COMMAND} -E copy_directory
        #         "${FFMPEG_DIR}/bin/"
        #         $<TARGET_FILE_DIR:${MY_TARGET_NAME}>
        # )
    # elseif(MINGW)
    #     message(STATUS ">>> The compiler is GNU ABI on Windows (MinGW)")
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    message(STATUS ">>> Building on Linux host")
    # 1. 指定 FFmpeg 根目录
    set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ffmpeg-n8.0.1-34-gbfa334de42-linux64-gpl-shared-8.0")
    message(STATUS ">>> FFMPEG_DIR = ${FFMPEG_DIR}")
    # 2. 添加 FFmpeg 头文件路径
    target_include_directories(${MY_TARGET_NAME}
        PRIVATE
            ${FFMPEG_DIR}/include
    )
    # 3. 添加 FFmpeg 库文件路径
    target_link_directories(${MY_TARGET_NAME}
        PRIVATE
            ${FFMPEG_DIR}/lib
    )
    # 5. 链接 FFmpeg 库（按需添加）
    target_link_libraries(${MY_TARGET_NAME}
        PRIVATE
            avformat
            avcodec
            avutil
            avfilter
            swscale
            swresample
    )
endif()

#include <SDL3/SDL.h>
set(MY_TARGET_NAME demuxing_decoding_4)
message(STATUS ">>> Linking SDL3 dev lib >>> https://wiki.libsdl.org/SDL3/README-cmake")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    message(STATUS ">>> Building on Windows host")
    if(MSVC)
        message(STATUS ">>> The compiler is MSVC")
        # 开关(ON/OFF)，是否使用 SDL3 CMake Config 模式
        option(USE_SDL3_CONFIG_PACKAGE "Use SDL3 find_package CONFIG mode（SDL3Config.cmake）" ON)
        if(USE_SDL3_CONFIG_PACKAGE)
            message(STATUS ">>> Using SDL3 CMake package config")
            # 通过 find_package()+SDL3Config.cmake 配置SDL3
            # CMake Config Package 机制：【XxXxConfig.cmake与XxXx_DIR成对出现，大小写敏感】cmake依赖发现机制
            # 1. 配置SDL3_DIR：XxXxConfig.cmake的absolutePath()
            # 2. 固定代码段find_package()+target_link_libraries()，参考自https://wiki.libsdl.org/SDL3/README-cmake
            # 3. project(... CXX)改为project(x LANGUAGES C CXX)
            # 4. 不需要也不会自动复制 DLL 到输出目录（这样就不会每次调试背后都拷贝文件，也不需要添加到“高级系统设置”）
            set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL3-devel-3.4.2-VC/SDL3-3.4.2/cmake")
            message(STATUS ">>> SDL3_DIR = ${SDL3_DIR}")
            find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
            target_link_libraries(${MY_TARGET_NAME} PRIVATE SDL3::SDL3)
        else()
            message(STATUS ">>> Using manual SDL3 include/lib setup")
            # 通过手动的 include/lib 配置SDL3
            set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL3-devel-3.4.2-VC/SDL3-3.4.2")
            message(STATUS ">>> SDL_DIR = ${SDL_DIR}")
            target_include_directories(${MY_TARGET_NAME}
                PRIVATE
                    ${SDL_DIR}/include
            )
            target_link_directories(${MY_TARGET_NAME}
                PRIVATE
                    ${SDL_DIR}/lib/x64
            )
            # 4. 添加编译定义，避免 SDL 重定义 main，告诉 SDL：不要替我处理 main 入口，我自己管理程序入口。
            target_compile_definitions(${MY_TARGET_NAME}
                PRIVATE
                    SDL_MAIN_HANDLED
            )
            target_link_libraries(${MY_TARGET_NAME}
                PRIVATE
                    SDL3 # 只链接 SDL3，不链接 SDL3main
            )
            # 6. 自动复制 DLL 到输出目录（可选）
            # 添加到“高级系统设置”可注释掉：XXX\third_party\SDL3-devel-3.4.2-VC\SDL3-3.4.2\lib\x64
            add_custom_command(TARGET ${MY_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${SDL_DIR}/lib/x64"
                    $<TARGET_FILE_DIR:${MY_TARGET_NAME}>
            )
        endif()
    elseif(MINGW)
        message(STATUS ">>> The compiler is GNU ABI on Windows (MinGW)")
        # 开关(ON/OFF)，是否使用 SDL3 CMake Config 模式
        option(USE_SDL3_CONFIG_PACKAGE "Use SDL3 find_package CONFIG mode（SDL3Config.cmake）" ON)
        if(USE_SDL3_CONFIG_PACKAGE)
            message(STATUS ">>> Using SDL3 CMake package config")
            # 通过 find_package()+SDL3Config.cmake 配置SDL3
            # CMake Config Package 机制：【XxXxConfig.cmake与XxXx_DIR成对出现，大小写敏感】cmake依赖发现机制
            # 1. 配置SDL3_DIR：XxXxConfig.cmake的absolutePath()
            # 2. 固定代码段find_package()+target_link_libraries()，参考自https://wiki.libsdl.org/SDL3/README-cmake
            # 3. project(... CXX)改为project(x LANGUAGES C CXX)
            # 4. 不需要也不会自动复制 DLL 到输出目录（这样就不会每次调试背后都拷贝文件，也不需要添加到“高级系统设置”）
            set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL3-devel-3.4.2-mingw/SDL3-3.4.2/cmake")
            message(STATUS ">>> SDL3_DIR = ${SDL3_DIR}")
            find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
            target_link_libraries(${MY_TARGET_NAME} PRIVATE SDL3::SDL3)
        else()
            message(STATUS ">>> Using manual SDL3 include/lib setup")
            # 通过手动的 include/lib 配置SDL3
            set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL3-devel-3.4.2-mingw/SDL3-3.4.2/x86_64-w64-mingw32")
            message(STATUS ">>> SDL_DIR = ${SDL_DIR}")
            target_include_directories(${MY_TARGET_NAME}
                PRIVATE
                    ${SDL_DIR}/include
            )
            target_link_directories(${MY_TARGET_NAME}
                PRIVATE
                    ${SDL_DIR}/lib
            )
            # 4. 添加编译定义，避免 SDL 重定义 main，告诉 SDL：不要替我处理 main 入口，我自己管理程序入口。
            target_compile_definitions(${MY_TARGET_NAME}
                PRIVATE
                    SDL_MAIN_HANDLED
            )
            target_link_libraries(${MY_TARGET_NAME}
                PRIVATE
                    SDL3 # 只链接 SDL3，不链接 SDL3main
            )
            # 6. 自动复制 DLL 到输出目录（mingw似乎并不需要）
            # 添加到“高级系统设置”可注释掉：XXX\third_party\SDL3-devel-3.4.2-VC\SDL3-3.4.2\lib\x64
            # add_custom_command(TARGET ${MY_TARGET_NAME} POST_BUILD
            #         COMMAND ${CMAKE_COMMAND} -E copy_directory
            #         "${SDL_DIR}/lib/x64"
            #         $<TARGET_FILE_DIR:${MY_TARGET_NAME}>
            # )
        endif()
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    message(STATUS ">>> Building on Linux host")
    # 1. sudo apt install libsdl3-dev
    # 2. https://wiki.libsdl.org/SDL3/README-cmake
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
    target_link_libraries(${MY_TARGET_NAME} PRIVATE SDL3::SDL3)
endif()


