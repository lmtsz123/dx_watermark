@echo off
chcp 65001 >nul
echo ================================
echo 文字水印功能测试
echo ================================
echo.

REM 检查是否提供了输入视频
if "%~1"=="" (
    echo 错误: 请提供输入视频文件
    echo.
    echo 用法: test_text_watermark.bat ^<输入视频^>
    echo.
    echo 示例:
    echo   test_text_watermark.bat video.mp4
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

echo 输入视频: %INPUT_VIDEO%
echo.

echo ================================
echo 测试1: 中文文字水印
echo ================================
echo 文字: "机密文件"
echo 透明度: 0.3
echo.

DXWatermark.exe "%INPUT_VIDEO%" 0.3 dx "机密文件"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo 测试失败！
    pause
    exit /b 1
)

echo.
echo ================================
echo 测试成功！
echo ================================
echo.
echo 输出文件已生成，请查看效果：
for %%F in ("%INPUT_VIDEO%") do (
    set "FILENAME=%%~nF"
    set "FILEPATH=%%~dpF"
    set "FILEEXT=%%~xF"
)
set "OUTPUT_FILE=%FILEPATH%%FILENAME%_watermarked%FILEEXT%"
echo %OUTPUT_FILE%
echo.
echo 可以尝试其他文字：
echo   DXWatermark.exe "%INPUT_VIDEO%" 0.3 dx "内部资料"
echo   DXWatermark.exe "%INPUT_VIDEO%" 0.3 dx "CONFIDENTIAL"
echo   DXWatermark.exe "%INPUT_VIDEO%" 0.3 dx "版权所有 © 2025"
echo.

pause

