@echo off
REM FFmpeg方法测试脚本
echo ================================
echo FFmpeg水印方法测试
echo ================================
echo.

REM 检查是否提供了输入视频
if "%~1"=="" (
    echo 错误: 请提供输入视频文件
    echo.
    echo 用法: test_ffmpeg_method.bat ^<输入视频^>
    echo.
    pause
    exit /b 1
)

set INPUT_VIDEO=%~1

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

echo 输入视频: %INPUT_VIDEO%
echo 水印文件: watermark_1.png
echo 处理方法: FFmpeg Filter
echo 透明度: 0.3
echo.
echo 开始处理...
echo ================================
echo.

REM 运行程序
DXWatermark.exe "%INPUT_VIDEO%" 0.3 ffmpeg

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ================================
    echo 处理成功！
    echo ================================
    
    REM 获取输出文件名
    for %%F in ("%INPUT_VIDEO%") do (
        set "FILENAME=%%~nF"
        set "FILEPATH=%%~dpF"
        set "FILEEXT=%%~xF"
    )
    
    set "OUTPUT_FILE=%FILEPATH%%FILENAME%_watermarked%FILEEXT%"
    
    echo 输出文件: %OUTPUT_FILE%
    
    REM 检查输出文件大小
    if exist "%OUTPUT_FILE%" (
        for %%A in ("%OUTPUT_FILE%") do (
            echo 文件大小: %%~zA 字节
            
            REM 检查文件是否太小（可能有问题）
            if %%~zA LSS 1000 (
                echo.
                echo 警告: 输出文件太小 (%%~zA 字节^)，可能处理失败
                echo 请检查上面的错误信息
            ) else (
                echo.
                echo 文件看起来正常，可以尝试播放
            )
        )
    ) else (
        echo.
        echo 警告: 未找到输出文件
    )
) else (
    echo.
    echo ================================
    echo 处理失败！
    echo ================================
    echo 错误代码: %ERRORLEVEL%
    echo 请检查上面的错误信息
)

echo.
pause

