cmake_minimum_required(VERSION 3.19)
project(demuxing_decoding_4 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

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
)

target_link_libraries(demuxing_decoding_4
    PRIVATE
        Qt::Core
        Qt::Widgets
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

# 1. 指定 FFmpeg 根目录
set(FFMPEG_DIR "D:/Codes/cppCode/FFmpegCode/ffmpeg-master-latest-win64-gpl-shared")
# 2. 添加 FFmpeg 头文件路径
# include_directories(${FFMPEG_DIR}/include)
target_include_directories(demuxing_decoding_4
    PRIVATE
        ${FFMPEG_DIR}/include
)
# 3. 添加 FFmpeg 库文件路径
# link_directories(${FFMPEG_DIR}/lib)
target_link_directories(demuxing_decoding_4
    PRIVATE
        ${FFMPEG_DIR}/lib
)
# 5. 链接 FFmpeg 库（按需添加）
target_link_libraries(demuxing_decoding_4
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
# add_custom_command(TARGET demuxing_decoding_4 POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_directory
#         "${FFMPEG_DIR}/bin/"
#         $<TARGET_FILE_DIR:demuxing_decoding_4>
# )

#include <SDL.h>
set(SDL_DIR "D:/Codes/cppCode/FFmpegCode/014/ffmpeg-simple-player/thirdparty/SDL2-2.0.22")
target_include_directories(demuxing_decoding_4
    PRIVATE
        ${SDL_DIR}/include
)
target_link_directories(demuxing_decoding_4
    PRIVATE
        ${SDL_DIR}/lib/x64
)
# 4. 添加编译定义，避免 SDL 重定义 main
target_compile_definitions(demuxing_decoding_4
    PRIVATE
        SDL_MAIN_HANDLED
)
target_link_libraries(demuxing_decoding_4
    PRIVATE
        SDL2 # 只链接 SDL2，不链接 SDL2main
)
# # 6. 自动复制 DLL 到输出目录（可选）
# # 添加到“高级系统设置”可注释掉：D:\Codes\cppCode\FFmpegCode\ffmpeg-master-latest-win64-gpl-shared\bin
# # 但是要注意只能有一个ffmpeg路径，否则cmd输入ffmpeg就会显示拒绝访问
add_custom_command(TARGET demuxing_decoding_4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SDL_DIR}/lib/x64"
        $<TARGET_FILE_DIR:demuxing_decoding_4>
)

