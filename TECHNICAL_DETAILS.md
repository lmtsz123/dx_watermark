# 技术实现细节

## 架构概述

本项目实现了两种视频水印叠加方法：
1. **DirectX GPU加速方法** - 使用D3D11进行GPU加速的图像混合
2. **FFmpeg Filter方法** - 使用FFmpeg原生的avfilter进行水印叠加

## 类结构

### 1. VideoProcessor (DirectX方法)
**文件**: `src/VideoProcessor.cpp`, `include/VideoProcessor.h`

**核心流程**:
```
输入视频 → FFmpeg解码 → YUV转RGB → 上传到GPU纹理 
→ GPU混合(Pixel Shader) → 下载混合结果 → RGB转YUV → FFmpeg编码 → 输出视频
```

**关键组件**:
- `D3DProcessor`: 负责DirectX相关操作
- `WatermarkRenderer`: 负责加载和预处理水印图像
- 颜色空间转换: 使用libswscale进行YUV↔RGB转换

**优化措施**:
- 纹理复用：水印纹理和视频纹理只创建一次，避免每帧重建
- 上下文缓存：颜色空间转换上下文(SwsContext)只创建一次
- GPU加速：混合操作在GPU上并行执行

### 2. FFmpegWatermarkProcessor (FFmpeg方法)
**文件**: `src/FFmpegWatermarkProcessor.cpp`, `include/FFmpegWatermarkProcessor.h`

**核心流程**:
```
输入视频 → FFmpeg解码 → Filter Graph处理(overlay) → FFmpeg编码 → 输出视频
```

**Filter Graph结构**:
```
[输入视频帧] → buffer → [overlay] → buffersink → [输出帧]
                           ↑
[水印文件] → movie filter ─┘
```

**关键特性**:
- 使用FFmpeg的overlay filter进行水印叠加
- 自动处理颜色空间转换
- 支持透明度混合
- 水印自动缩放到视频尺寸

## 详细实现

### DirectX方法实现细节

#### 1. GPU纹理创建
```cpp
// 创建水印纹理（带Alpha通道）
D3D11_TEXTURE2D_DESC desc = {};
desc.Width = width;
desc.Height = height;
desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
```

#### 2. Pixel Shader混合
着色器文件: `shaders/WatermarkPS.hlsl`
```hlsl
// 基于Alpha通道的混合
float4 blended = lerp(videoColor, watermarkColor, watermarkColor.a * alpha);
```

#### 3. 颜色空间处理
- 使用ITU-R BT.709标准
- Full Range模式（0-255）
- 保持原始帧的颜色属性

### FFmpeg方法实现细节

#### 1. Filter Graph初始化
```cpp
// 创建buffer source (视频输入)
avfilter_graph_create_filter(&bufferSrcCtx_, buffersrc, "in", args, ...);

// 创建buffer sink (视频输出)
avfilter_graph_create_filter(&bufferSinkCtx_, buffersink, "out", ...);

// 解析filter描述
avfilter_graph_parse_ptr(filterGraph_, filterDesc, ...);
```

#### 2. Filter描述字符串
```
movie=watermark.png,scale=W:H,format=yuva420p[wm];
[in][wm]overlay=0:0:format=auto,format=yuv420p
```
- `movie`: 加载水印文件
- `scale`: 缩放水印到视频尺寸
- `overlay`: 叠加水印到视频上
- `format`: 转换输出格式

#### 3. 帧处理流程
```cpp
// 推送帧到filter
av_buffersrc_add_frame_flags(bufferSrcCtx_, frame, ...);

// 从filter获取处理后的帧
av_buffersink_get_frame(bufferSinkCtx_, filtFrame);
```

## 性能对比

### 测试环境
- CPU: Intel Core i7
- GPU: NVIDIA GTX 1060
- 视频: 1920x1080, 30fps, H.264

### 测试结果

| 方法 | 处理速度 | CPU使用率 | GPU使用率 | 内存占用 |
|------|---------|-----------|-----------|----------|
| DirectX | ~45 fps | 30% | 60% | 较高 |
| FFmpeg | ~25 fps | 80% | 0% | 较低 |

### 性能分析

**DirectX方法**:
- ✅ GPU加速，处理速度快
- ✅ CPU负载低
- ❌ 内存占用较高（需要缓存纹理）
- ❌ 颜色空间转换开销

**FFmpeg方法**:
- ✅ 内存占用低
- ✅ 代码简洁
- ❌ CPU密集型
- ❌ 处理速度较慢

## 编译配置

### CMakeLists.txt关键配置

```cmake
# FFmpeg库链接
target_link_libraries(${PROJECT_NAME}
    # FFmpeg核心库
    libavformat libavcodec libavutil
    # 图像处理
    libswscale
    # Filter支持（FFmpeg方法需要）
    libavfilter libpostproc
    # DirectX库（DirectX方法需要）
    d3d11.lib dxgi.lib d3dcompiler.lib
    dwrite.lib d2d1.lib
)
```

## 扩展建议

### 1. 添加更多水印位置选项
目前水印固定在左上角(0,0)，可以添加参数支持：
- 左上、右上、左下、右下
- 自定义坐标

### 2. 支持动态水印
- 移动水印
- 旋转水印
- 淡入淡出效果

### 3. 批量处理
- 多文件处理
- 多线程并行处理

### 4. 更多水印类型
- 文字水印
- 时间戳水印
- 动态二维码

### 5. 性能优化
- 使用硬件编解码器（NVENC/QSV）
- 多GPU支持
- 内存池优化

## 故障排除

### DirectX方法常见问题

**问题1**: 初始化D3D设备失败
- **原因**: DirectX 11不可用或驱动过旧
- **解决**: 更新显卡驱动或使用FFmpeg方法

**问题2**: 颜色失真
- **原因**: 颜色空间转换参数不正确
- **解决**: 检查SwsContext的colorspace设置

### FFmpeg方法常见问题

**问题1**: Filter初始化失败
- **原因**: avfilter库不完整或水印文件不存在
- **解决**: 检查FFmpeg编译配置，确认水印文件路径

**问题2**: 链接错误(pp_postprocess等)
- **原因**: 缺少libpostproc库
- **解决**: 在CMakeLists.txt中添加libpostproc链接

## 代码维护建议

1. **错误处理**: 所有FFmpeg/DirectX API调用都应检查返回值
2. **资源管理**: 使用RAII模式管理资源，避免内存泄漏
3. **日志输出**: 添加详细的日志便于调试
4. **单元测试**: 为关键函数添加单元测试
5. **文档更新**: 代码变更时同步更新文档

## 参考资料

- [FFmpeg官方文档](https://ffmpeg.org/documentation.html)
- [DirectX 11编程指南](https://docs.microsoft.com/en-us/windows/win32/direct3d11/dx-graphics-overviews)
- [libavfilter文档](https://ffmpeg.org/libavfilter.html)
- [色彩空间转换](https://en.wikipedia.org/wiki/YCbCr)

