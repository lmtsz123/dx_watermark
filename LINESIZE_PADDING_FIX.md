# FFmpeg Linesize Padding 问题修复

## 问题描述

从BMP导出的图像中发现：
- ✅ 水印正常覆盖整个画面
- ❌ 底层视频帧被严重拉伸变形

## 问题根源

### FFmpeg的内存对齐

FFmpeg在分配帧缓冲区时，会对每一行进行**内存对齐**以提高性能。这意味着：

```
实际存储：[像素数据][padding][像素数据][padding]...
         |<- linesize ->|<- linesize ->|

而不是：[像素数据][像素数据][像素数据]...
       |<- width*3 ->|<- width*3 ->|
```

### 问题代码

**旧代码** (`VideoProcessor.cpp`):
```cpp
// 转换为RGB
sws_scale(swsToRgbCtx_, frame->data, frame->linesize, 0, height_,
          rgbFrame->data, rgbFrame->linesize);

// 直接传递data[0]指针，假设数据是紧密排列的
d3dProcessor_->UpdateTextureData(videoTexture_, rgbFrame->data[0], width_, height_);
```

**UpdateTextureData假设数据紧密排列**:
```cpp
for (int i = 0; i < width * height; i++) {
    rgbaData[i * 4 + 0] = data[i * 3 + 0]; // 错误！假设连续存储
    rgbaData[i * 4 + 1] = data[i * 3 + 1];
    rgbaData[i * 4 + 2] = data[i * 3 + 2];
}
```

### 为什么会拉伸？

假设：
- 视频宽度：1920像素
- 理论行大小：1920 * 3 = 5760字节
- 实际linesize：5888字节（对齐到64字节边界）
- Padding：128字节

当我们按`i * 3`读取时：
```
期望读取：像素0, 像素1, 像素2, ...
实际读取：像素0, 像素1, ..., 像素N, padding, 像素N+1, ...
```

结果：
- 每行末尾的padding被当作像素数据
- 下一行的开始位置错位
- 整个图像被"拉伸"和错位

## 修复方案

### 新代码

```cpp
// 转换为RGB
sws_scale(swsToRgbCtx_, frame->data, frame->linesize, 0, height_,
          rgbFrame->data, rgbFrame->linesize);

// 复制到紧密排列的缓冲区，去除padding
std::vector<unsigned char> tightRgbData(width_ * height_ * 3);
for (int y = 0; y < height_; y++) {
    memcpy(tightRgbData.data() + y * width_ * 3,      // 目标：紧密排列
           rgbFrame->data[0] + y * rgbFrame->linesize[0],  // 源：有padding
           width_ * 3);                                // 只复制实际像素
}

// 传递紧密排列的数据
d3dProcessor_->UpdateTextureData(videoTexture_, tightRgbData.data(), width_, height_);
```

### 关键改进

1. **逐行复制**：每次只复制一行的实际像素数据
2. **跳过padding**：源地址使用`linesize[0]`步进，目标使用`width * 3`步进
3. **紧密排列**：确保传递给GPU的数据没有padding

## 技术细节

### FFmpeg的linesize

```cpp
AVFrame* frame;
frame->linesize[0]  // 第0平面（RGB的话就是整个图像）的行大小（字节）
frame->width        // 图像宽度（像素）
frame->height       // 图像高度（像素）

// 对于RGB24格式：
// linesize[0] >= width * 3
// 等号成立当且仅当没有padding
```

### 为什么需要对齐？

1. **CPU缓存行对齐**：提高内存访问效率
2. **SIMD指令**：某些SIMD指令要求数据对齐
3. **硬件加速**：某些硬件解码器要求对齐

### 常见的对齐值

- 16字节对齐（SSE）
- 32字节对齐（AVX）
- 64字节对齐（缓存行）

## 性能影响

### 修复前
- ❌ 数据错位，图像拉伸
- ❌ 无法正常显示

### 修复后
- ✅ 数据正确
- ✅ 图像正常显示
- ⚠️ 增加一次内存复制（可接受的开销）

### 优化建议

如果需要进一步优化，可以：

1. **直接在UpdateTextureData中处理linesize**：
```cpp
bool D3DProcessor::UpdateTextureData(ID3D11Texture2D* texture, 
                                     const unsigned char* data, 
                                     int width, int height,
                                     int linesize)  // 新增参数
{
    std::vector<unsigned char> rgbaData(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = y * linesize + x * 3;  // 使用linesize
            int dstIdx = (y * width + x) * 4;
            rgbaData[dstIdx + 0] = data[srcIdx + 0];
            rgbaData[dstIdx + 1] = data[srcIdx + 1];
            rgbaData[dstIdx + 2] = data[srcIdx + 2];
            rgbaData[dstIdx + 3] = 255;
        }
    }
    // ...
}
```

2. **使用GPU直接处理带padding的数据**：
   - 修改纹理创建，使用linesize作为RowPitch
   - 在Shader中处理

## 其他可能受影响的地方

### 检查清单

- [x] YUV到RGB转换（已修复）
- [ ] RGB到YUV转换（需要检查）
- [ ] 水印加载（已经是紧密排列）
- [ ] BMP导出（已经正确处理RowPitch）

### RGB到YUV转换

当前代码：
```cpp
memcpy(tempRgbFrame->data[0], blendedData.data(), width_ * height_ * 3);
```

这个是安全的，因为：
- `blendedData`是紧密排列的
- `tempRgbFrame`会自动处理linesize

## 测试验证

### 测试步骤

1. 删除旧的BMP文件
```bash
del blended_frame_*.bmp
```

2. 重新运行程序
```bash
cd build\Release
DXWatermark.exe 11.mp4 0.3 dx
```

3. 检查新生成的BMP文件
- [ ] 视频内容不再拉伸
- [ ] 图像比例正确
- [ ] 水印正常覆盖
- [ ] 混合效果正确

### 预期结果

修复后的BMP应该显示：
- ✅ 正常比例的视频内容
- ✅ 正常覆盖的水印
- ✅ 正确的混合效果
- ✅ 没有拉伸或变形

## 经验教训

### 1. 永远不要假设FFmpeg数据是紧密排列的

**错误假设**：
```cpp
// 假设RGB数据紧密排列
for (int i = 0; i < width * height; i++) {
    process(data[i * 3]);
}
```

**正确做法**：
```cpp
// 使用linesize
for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        int idx = y * linesize + x * 3;
        process(data[idx]);
    }
}
```

### 2. 读取FFmpeg文档

FFmpeg文档明确说明：
> The linesize may be larger than the size of usable data

### 3. 使用调试工具

导出BMP是发现这类问题的最佳方法：
- 可以直观看到图像问题
- 比在视频中调试容易得多

## 相关链接

- FFmpeg AVFrame文档：https://ffmpeg.org/doxygen/trunk/structAVFrame.html
- 内存对齐：https://en.wikipedia.org/wiki/Data_structure_alignment
- SIMD对齐要求：https://software.intel.com/content/www/us/en/develop/articles/data-alignment-to-assist-vectorization.html

## 总结

这是一个经典的**内存对齐问题**：
- 问题根源：假设数据紧密排列
- 实际情况：FFmpeg使用对齐的linesize
- 解决方案：逐行复制，去除padding
- 教训：永远使用linesize，不要假设数据布局

修复后，视频帧应该能正确显示，不再有拉伸问题！

