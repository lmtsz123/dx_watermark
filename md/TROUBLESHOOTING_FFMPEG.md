# FFmpeg方法故障排查指南

## 问题：生成的MP4文件只有48字节

这个问题通常表示视频编码或写入过程失败。以下是可能的原因和解决方案：

### 1. Filter初始化失败

**症状**：
- 程序输出 "无法解析filter图" 或 "无法配置filter图"
- 输出文件非常小（几十字节）

**可能原因**：
- 水印文件路径不正确
- 水印文件格式不支持
- Filter描述字符串有语法错误

**解决方案**：
```bash
# 1. 确保水印文件在正确位置
dir watermark_1.png

# 2. 使用绝对路径测试
DXWatermark.exe video.mp4 0.3 ffmpeg

# 3. 检查程序输出的Filter描述
# 应该看到类似：Filter描述: movie='watermark_1.png',scale=1920:1080[wm];[in][wm]overlay=0:0
```

### 2. 编码器问题

**症状**：
- 程序输出 "发送帧到编码器失败"
- 解码帧数正常，但编码帧数为0

**可能原因**：
- H.264编码器不可用
- 编码参数不兼容
- 像素格式转换失败

**解决方案**：
```bash
# 检查FFmpeg编码器
ffmpeg -encoders | findstr h264

# 如果没有h264编码器，需要重新编译FFmpeg或使用其他编码器
```

### 3. 水印文件问题

**症状**：
- 程序输出 "无法解析filter图"
- 错误信息提到 "movie" filter

**可能原因**：
- 水印文件不存在
- 水印文件损坏
- 文件格式不支持

**解决方案**：
```bash
# 1. 验证水印文件
ffmpeg -i watermark_1.png

# 2. 转换水印文件格式
ffmpeg -i watermark.jpg -pix_fmt rgba watermark_1.png

# 3. 确保水印文件有透明通道（如果需要）
ffmpeg -i watermark.png -pix_fmt rgba watermark_1.png
```

### 4. 路径问题（Windows）

**症状**：
- 使用绝对路径时失败
- 路径包含空格或特殊字符

**可能原因**：
- Windows路径反斜杠未正确转义
- 路径包含特殊字符

**解决方案**：
- 程序已自动将反斜杠转换为正斜杠
- 如果仍有问题，将水印文件复制到程序目录
- 使用不含空格和特殊字符的路径

### 5. 调试步骤

#### 步骤1：启用详细日志
程序已包含详细的错误信息输出。运行时注意查看：

```
Buffer source参数: video_size=1920x1080:pix_fmt=0:time_base=1/30000:pixel_aspect=1/1
Filter描述: movie='watermark_1.png',scale=1920:1080[wm];[in][wm]overlay=0:0
Filter初始化成功
开始处理视频帧...
已解码 30 帧, 已编码 30 帧
```

#### 步骤2：检查每个阶段
1. **输入打开**：应该看到 "输入视频: WxH, 格式: xxx"
2. **输出创建**：应该看到 "输出视频: WxH"
3. **Filter初始化**：应该看到 "Filter初始化成功"
4. **帧处理**：应该看到 "已解码 X 帧, 已编码 Y 帧"
5. **完成**：应该看到 "处理完成！解码 X 帧, 编码 Y 帧"

#### 步骤3：使用测试脚本
```bash
test_ffmpeg_method.bat your_video.mp4
```

测试脚本会：
- 检查输入文件
- 检查水印文件
- 运行程序
- 验证输出文件大小
- 报告可能的问题

### 6. 常见错误信息

#### "无法打开输入文件"
- 检查视频文件路径
- 确保文件格式被FFmpeg支持

#### "未找到H.264编码器"
- FFmpeg编译时未包含H.264支持
- 需要重新编译FFmpeg或使用预编译版本

#### "无法解析filter图"
- Filter描述字符串语法错误
- 水印文件路径问题
- 检查程序输出的Filter描述

#### "推送帧到filter失败"
- Filter配置与输入格式不匹配
- 检查像素格式和分辨率

#### "写入数据包失败"
- 输出文件路径无效
- 磁盘空间不足
- 文件权限问题

### 7. 与DirectX方法对比测试

如果FFmpeg方法失败，可以尝试DirectX方法验证基本功能：

```bash
# DirectX方法
DXWatermark.exe video.mp4 0.3 dx

# 如果DirectX方法成功，说明：
# - 输入视频正常
# - 水印图像正常
# - 问题在FFmpeg filter配置
```

### 8. 手动测试FFmpeg Filter

可以使用命令行FFmpeg测试filter：

```bash
# 测试overlay filter
ffmpeg -i video.mp4 -i watermark_1.png -filter_complex "[1:v]scale=1920:1080[wm];[0:v][wm]overlay=0:0" -c:v libx264 -crf 23 test_output.mp4

# 如果这个命令成功，说明filter本身没问题
# 如果失败，检查FFmpeg版本和filter支持
```

### 9. 检查FFmpeg库版本

```bash
# 检查avfilter版本
ffmpeg -version

# 确保包含以下库：
# - libavfilter
# - libpostproc
# - overlay filter支持
```

### 10. 最后的解决方案

如果所有方法都失败：

1. **使用DirectX方法**：
   ```bash
   DXWatermark.exe video.mp4 0.3 dx
   ```

2. **使用外部FFmpeg命令**：
   ```bash
   ffmpeg -i video.mp4 -i watermark_1.png -filter_complex "overlay=0:0" output.mp4
   ```

3. **更新FFmpeg库**：
   - 下载最新的FFmpeg开发库
   - 重新编译项目

## 获取帮助

如果问题仍未解决，请提供以下信息：

1. 完整的程序输出（包括所有错误信息）
2. 输入视频信息（使用 `ffmpeg -i video.mp4`）
3. 水印文件信息（使用 `ffmpeg -i watermark_1.png`）
4. FFmpeg版本（使用 `ffmpeg -version`）
5. 输出文件大小
6. 是否使用DirectX方法成功

## 快速检查清单

- [ ] 水印文件 watermark_1.png 存在
- [ ] 输入视频文件存在且可播放
- [ ] FFmpeg库完整（包含avfilter）
- [ ] 有足够的磁盘空间
- [ ] 输出目录有写入权限
- [ ] 查看程序输出的详细错误信息
- [ ] 尝试使用DirectX方法对比
- [ ] 使用测试脚本验证

