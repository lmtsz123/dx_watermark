# 视频水印处理工具 - 双方法支持

本工具支持两种水印处理方法，用户可以根据需求选择：

## 方法对比

### 1. DirectX GPU加速方法 (默认)
- **优势**：
  - 利用GPU硬件加速，处理速度快
  - 适合需要高性能处理的场景
  - 支持复杂的图像混合操作
  
- **劣势**：
  - 需要DirectX 11支持
  - 仅支持Windows平台
  - 需要加载完整水印图像到内存

### 2. FFmpeg Filter方法
- **优势**：
  - 跨平台支持（Windows/Linux/macOS）
  - 使用FFmpeg原生filter，兼容性好
  - 代码简洁，易于维护
  - 支持更多水印格式和效果
  
- **劣势**：
  - CPU处理，速度相对较慢
  - 依赖FFmpeg的filter库

## 使用方法

### 基本语法
```bash
DXWatermark.exe <输入视频> [透明度] [方法]
```

### 参数说明
- `输入视频`: 要处理的视频文件路径（必需）
- `透明度`: 水印透明度，范围0.0-1.0，默认0.3（可选）
- `方法`: 处理方法，可选值：
  - `dx` - 使用DirectX GPU加速（默认）
  - `ffmpeg` - 使用FFmpeg filter

### 使用示例

#### 1. 使用默认DirectX方法
```bash
# 使用默认透明度0.3
DXWatermark.exe input.mp4

# 指定透明度
DXWatermark.exe input.mp4 0.5

# 显式指定DirectX方法
DXWatermark.exe input.mp4 0.3 dx
```

#### 2. 使用FFmpeg方法
```bash
# 使用默认透明度0.3
DXWatermark.exe input.mp4 0.3 ffmpeg

# 指定透明度
DXWatermark.exe input.mp4 0.5 ffmpeg
```

## 输出文件
输出文件将自动生成在输入文件的同一目录下，文件名格式为：
```
<原文件名>_watermarked.<扩展名>
```

例如：
- 输入：`video.mp4`
- 输出：`video_watermarked.mp4`

## 水印文件
程序会在当前目录查找 `watermark_1.png` 作为水印图像。
请确保该文件存在于程序运行目录。

## 技术实现

### DirectX方法
1. 使用FFmpeg解码视频帧
2. 将帧转换为RGB格式
3. 使用DirectX 11创建GPU纹理
4. 通过Pixel Shader在GPU上混合水印
5. 转换回YUV格式
6. 使用FFmpeg编码输出

### FFmpeg方法
1. 使用FFmpeg解码视频帧
2. 使用avfilter的overlay filter叠加水印
3. 自动处理颜色空间转换
4. 使用FFmpeg编码输出

## 性能建议

- **高性能需求**：选择DirectX方法，充分利用GPU加速
- **跨平台需求**：选择FFmpeg方法，保证兼容性
- **批量处理**：建议使用DirectX方法，可显著减少处理时间
- **简单场景**：两种方法均可，FFmpeg方法更简单直接

## 故障排除

### DirectX方法失败
- 检查是否安装了DirectX 11运行时
- 检查显卡驱动是否最新
- 尝试使用FFmpeg方法作为替代

### FFmpeg方法失败
- 检查FFmpeg库是否完整安装
- 确认avfilter库可用
- 检查水印文件格式是否正确

## 依赖库
- FFmpeg (libavformat, libavcodec, libswscale, libavutil, libavfilter, libpostproc)
- DirectX 11 (仅DirectX方法需要)
- DirectWrite/Direct2D (仅DirectX方法需要)

