@echo off
REM 视频水印处理示例脚本
REM 请确保 watermark_1.png 存在于当前目录

echo ================================
echo 视频水印处理工具 - 使用示例
echo ================================
echo.

REM 检查是否提供了输入视频
if "%~1"=="" (
    echo 错误: 请提供输入视频文件
    echo.
    echo 用法: example_usage.bat ^<输入视频^>
    echo.
    echo 示例:
    echo   example_usage.bat video.mp4
    echo.
    pause
    exit /b 1
)

set INPUT_VIDEO=%~1
set OUTPUT_DIR=%~dp1

echo 输入视频: %INPUT_VIDEO%
echo 输出目录: %OUTPUT_DIR%
echo.

REM 检查输入文件是否存在
if not exist "%INPUT_VIDEO%" (
    echo 错误: 输入视频文件不存在: %INPUT_VIDEO%
    pause
    exit /b 1
)

REM 检查水印文件是否存在
if not exist "watermark_1.png" (
    echo 警告: 水印文件 watermark_1.png 不存在于当前目录
    echo 请确保水印文件存在
    pause
    exit /b 1
)

echo ================================
echo 方法1: DirectX GPU加速 (推荐)
echo ================================
echo 正在使用DirectX方法处理...
echo.

DXWatermark.exe "%INPUT_VIDEO%" 0.3 dx

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo DirectX方法处理失败，尝试使用FFmpeg方法...
    echo.
    echo ================================
    echo 方法2: FFmpeg Filter
    echo ================================
    echo 正在使用FFmpeg方法处理...
    echo.
    
    DXWatermark.exe "%INPUT_VIDEO%" 0.3 ffmpeg
    
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo 两种方法都失败了，请检查错误信息
        pause
        exit /b 1
    )
)

echo.
echo ================================
echo 处理完成！
echo ================================
echo 输出文件已保存到输入视频的同一目录
echo.

pause

