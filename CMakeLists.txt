cmake_minimum_required(VERSION 3.10)
project(DXWatermark)

set(CMAKE_CXX_STANDARD 17)

# FFmpeg路径 - 根据编译模式自动选择
set(FFMPEG_DIR "D:/Learn/MultiMedia/shiftmediaproject/msvc")
include_directories(${FFMPEG_DIR}/include)

# 根据编译配置选择对应的库目录
# Debug模式使用 lib/x64 目录，Release模式也使用 lib/x64 目录
# 如果有单独的debug库目录，可以修改为：
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     link_directories(${FFMPEG_DIR}/lib/x64/debug)
# else()
#     link_directories(${FFMPEG_DIR}/lib/x64)
# endif()
link_directories(${FFMPEG_DIR}/lib/x64)

# DirectX已包含在Windows SDK中
if(WIN32)
    include_directories($ENV{WindowsSdkDir}Include/$ENV{WindowsSDKVersion}um)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/D3DProcessor.cpp
    src/WatermarkRenderer.cpp
    src/VideoProcessor.cpp
    src/FFmpegWatermarkProcessor.cpp
    src/main.cpp
)

set(HEADERS
    include/D3DProcessor.h
    include/WatermarkRenderer.h
    include/VideoProcessor.h
    include/FFmpegWatermarkProcessor.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库 - 使用生成器表达式根据配置选择Debug或Release版本的FFmpeg库
# ShiftMediaProject的Debug库带'd'后缀（如 libavformatd.lib）
target_link_libraries(${PROJECT_NAME}
    # FFmpeg - 使用生成器表达式自动选择Debug/Release版本
    $<$<CONFIG:Debug>:libavformatd>
    $<$<CONFIG:Debug>:libavcodecd>
    $<$<CONFIG:Debug>:libswscaled>
    $<$<CONFIG:Debug>:libavutild>
    $<$<CONFIG:Debug>:libswresampled>
    $<$<CONFIG:Debug>:libavfilterd>
    $<$<CONFIG:Debug>:libpostprocd>
    $<$<CONFIG:Release>:libavformat>
    $<$<CONFIG:Release>:libavcodec>
    $<$<CONFIG:Release>:libswscale>
    $<$<CONFIG:Release>:libavutil>
    $<$<CONFIG:Release>:libswresample>
    $<$<CONFIG:Release>:libavfilter>
    $<$<CONFIG:Release>:libpostproc>
    $<$<CONFIG:RelWithDebInfo>:libavformat>
    $<$<CONFIG:RelWithDebInfo>:libavcodec>
    $<$<CONFIG:RelWithDebInfo>:libswscale>
    $<$<CONFIG:RelWithDebInfo>:libavutil>
    $<$<CONFIG:RelWithDebInfo>:libswresample>
    $<$<CONFIG:RelWithDebInfo>:libavfilter>
    $<$<CONFIG:RelWithDebInfo>:libpostproc>
    $<$<CONFIG:MinSizeRel>:libavformat>
    $<$<CONFIG:MinSizeRel>:libavcodec>
    $<$<CONFIG:MinSizeRel>:libswscale>
    $<$<CONFIG:MinSizeRel>:libavutil>
    $<$<CONFIG:MinSizeRel>:libswresample>
    $<$<CONFIG:MinSizeRel>:libavfilter>
    $<$<CONFIG:MinSizeRel>:libpostproc>
    # DirectX
    d3d11.lib dxgi.lib d3dcompiler.lib
    # DirectWrite for text
    dwrite.lib d2d1.lib
)

# 复制着色器文件
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)