# 文字水印功能使用指南

## 功能说明

程序现在支持两种水印模式：
1. **图片水印**：使用PNG图片文件
2. **文字水印**：自动生成45度倾斜平铺的文字水印（新功能）

## 使用方法

### 基本语法

```bash
DXWatermark.exe <输入视频> [透明度] [方法] [文字水印]
```

### 参数说明

| 参数 | 说明 | 默认值 | 示例 |
|------|------|--------|------|
| 输入视频 | 要处理的视频文件路径 | 必需 | `video.mp4` |
| 透明度 | 水印透明度 (0.0-1.0) | 0.3 | `0.3` |
| 方法 | `dx` 或 `ffmpeg` | `dx` | `dx` |
| 文字水印 | 要显示的文字（可选） | 无 | `"机密文件"` |

## 使用示例

### 1. 使用图片水印（原有功能）

```bash
# 使用默认设置
DXWatermark.exe video.mp4

# 指定透明度
DXWatermark.exe video.mp4 0.5 dx

# 使用FFmpeg方法
DXWatermark.exe video.mp4 0.3 ffmpeg
```

**要求**：程序目录下必须有 `watermark_1.png` 文件

### 2. 使用文字水印（新功能）

```bash
# 基本用法
DXWatermark.exe video.mp4 0.3 dx "机密文件"

# 不同透明度
DXWatermark.exe video.mp4 0.2 dx "内部资料"
DXWatermark.exe video.mp4 0.5 dx "CONFIDENTIAL"

# 中文水印
DXWatermark.exe video.mp4 0.3 dx "版权所有 © 2025"

# 英文水印
DXWatermark.exe video.mp4 0.3 dx "CONFIDENTIAL"

# 混合文字
DXWatermark.exe video.mp4 0.3 dx "机密 CONFIDENTIAL"
```

**注意**：文字水印需要用引号括起来（如果包含空格）

## 文字水印特性

### 1. 45度倾斜
文字自动旋转45度（顺时针），与图片示例中的效果一致

### 2. 平铺重复
文字会自动平铺填满整个视频画面，间距根据文字长度自动调整

### 3. 自适应尺寸
水印会自动适应视频尺寸，无论视频是720p、1080p还是4K

### 4. 透明度控制
通过透明度参数控制水印的可见程度：
- `0.1-0.2`：淡水印，不影响观看
- `0.3-0.4`：适中水印（推荐）
- `0.5-0.7`：明显水印
- `0.8-1.0`：强水印

## 效果对比

### 图片水印
```bash
DXWatermark.exe video.mp4 0.3 dx
```
- 使用预先准备的PNG图片
- 图片内容固定
- 需要提前制作水印图片

### 文字水印
```bash
DXWatermark.exe video.mp4 0.3 dx "机密文件"
```
- 实时生成水印
- 内容灵活可变
- 无需准备图片文件
- 自动45度倾斜平铺

## 技术细节

### 文字渲染
- 使用DirectWrite进行高质量文字渲染
- 字体：Arial
- 大小：35pt
- 颜色：白色
- 透明度：可调节

### 平铺算法
```
水平间距 = 文字宽度 + 150像素
垂直间距 = 文字高度 + 80像素
旋转角度 = -45度（顺时针45度）
```

### 渲染流程
```
1. 创建与视频尺寸相同的透明画布
2. 计算文字尺寸
3. 根据文字大小计算平铺间距
4. 在扩展区域平铺绘制文字（45度旋转）
5. 裁剪到视频尺寸
6. 转换为RGBA格式
7. 传递给GPU进行混合
```

## 常见用途

### 1. 版权保护
```bash
DXWatermark.exe video.mp4 0.3 dx "版权所有 © 公司名称"
```

### 2. 机密标记
```bash
DXWatermark.exe video.mp4 0.4 dx "机密文件 CONFIDENTIAL"
```

### 3. 内部资料标记
```bash
DXWatermark.exe video.mp4 0.3 dx "内部资料 仅供参考"
```

### 4. 防泄露追踪
```bash
DXWatermark.exe video.mp4 0.2 dx "工号:12345 2025-01-17"
```

### 5. 会议录制标记
```bash
DXWatermark.exe meeting.mp4 0.3 dx "内部会议 2025-01-17"
```

## 性能说明

### 文字水印 vs 图片水印

| 特性 | 文字水印 | 图片水印 |
|------|---------|---------|
| 生成速度 | 快（实时生成） | 快（直接加载） |
| 处理速度 | 相同 | 相同 |
| 内存占用 | 相同 | 相同 |
| 灵活性 | 高（随时修改） | 低（需要重新制作） |
| 质量 | 高（矢量渲染） | 取决于图片 |

**结论**：两种方式处理速度相同，文字水印更灵活方便。

## 故障排查

### 问题1：文字显示乱码
**原因**：控制台编码问题
**解决**：
```bash
# 确保使用UTF-8编码
chcp 65001
DXWatermark.exe video.mp4 0.3 dx "中文水印"
```

### 问题2：文字水印不可见
**原因**：透明度太低
**解决**：增加透明度参数
```bash
DXWatermark.exe video.mp4 0.5 dx "测试水印"
```

### 问题3：文字太小或太大
**原因**：字体大小固定为35pt
**解决**：当前版本字体大小固定，如需调整请修改源码中的字体大小参数

### 问题4：文字间距不合适
**原因**：间距根据文字长度自动计算
**解决**：
- 短文字：会自动增加间距
- 长文字：会自动减少间距
- 如需手动调整，可修改源码中的间距参数

## 高级用法

### 1. 批处理多个视频
创建批处理脚本 `batch_watermark.bat`：

```batch
@echo off
for %%f in (*.mp4) do (
    echo 处理: %%f
    DXWatermark.exe "%%f" 0.3 dx "机密文件"
)
echo 批处理完成！
pause
```

### 2. 动态水印（包含日期时间）
创建脚本 `watermark_with_date.bat`：

```batch
@echo off
set VIDEO=%1
set TEXT=%2

REM 获取当前日期时间
for /f "tokens=1-3 delims=/ " %%a in ('date /t') do set MYDATE=%%a-%%b-%%c
for /f "tokens=1-2 delims=: " %%a in ('time /t') do set MYTIME=%%a:%%b

REM 添加水印
DXWatermark.exe "%VIDEO%" 0.3 dx "%TEXT% %MYDATE% %MYTIME%"
```

使用：
```bash
watermark_with_date.bat video.mp4 "内部资料"
```

### 3. 不同级别的水印
```bash
# 淡水印（不影响观看）
DXWatermark.exe video.mp4 0.15 dx "草稿"

# 标准水印（推荐）
DXWatermark.exe video.mp4 0.3 dx "内部资料"

# 强水印（防止录屏）
DXWatermark.exe video.mp4 0.6 dx "严格保密"
```

## 输出文件

输出文件自动生成在输入文件的同一目录：
```
输入：video.mp4
输出：video_watermarked.mp4
```

## 总结

文字水印功能提供了：
- ✅ 灵活的文字内容
- ✅ 45度倾斜效果
- ✅ 自动平铺填充
- ✅ 高质量渲染
- ✅ 简单易用

适合需要快速添加文字水印的场景，无需提前制作水印图片！

