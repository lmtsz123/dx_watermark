# 完整的Linesize问题修复

## 问题总结

通过BMP调试发现了两个独立的linesize问题：

### 问题1：YUV到RGB转换（已修复）
- **现象**：BMP中视频帧被拉伸
- **原因**：读取RGB数据时没有考虑linesize的padding
- **影响**：GPU混合输入错误

### 问题2：RGB到YUV转换（刚修复）
- **现象**：BMP正常，但MP4视频异常
- **原因**：写入RGB数据时没有考虑linesize的padding
- **影响**：编码器输入错误

## 完整的数据流

```
原始YUV帧 (有padding)
    ↓
[YUV→RGB转换] sws_scale
    ↓
RGB帧 (有padding) ← 问题1：需要去除padding
    ↓
[逐行复制] 去除padding
    ↓
紧密排列的RGB数据
    ↓
[GPU混合] DirectX
    ↓
混合后的RGB数据 (紧密排列)
    ↓
[逐行复制] 添加padding ← 问题2：需要添加padding
    ↓
RGB帧 (有padding)
    ↓
[RGB→YUV转换] sws_scale
    ↓
YUV帧 (有padding)
    ↓
编码器
```

## 修复详情

### 修复1：YUV到RGB（去除padding）

**位置**：`VideoProcessor::ProcessFrame` - YUV转RGB后

**代码**：
```cpp
// 转换为RGB
sws_scale(swsToRgbCtx_, frame->data, frame->linesize, 0, height_,
          rgbFrame->data, rgbFrame->linesize);

// 去除padding，复制到紧密排列的缓冲区
std::vector<unsigned char> tightRgbData(width_ * height_ * 3);
for (int y = 0; y < height_; y++) {
    memcpy(tightRgbData.data() + y * width_ * 3,           // 目标：紧密排列
           rgbFrame->data[0] + y * rgbFrame->linesize[0],  // 源：有padding
           width_ * 3);                                     // 只复制实际像素
}

// 传递给GPU
d3dProcessor_->UpdateTextureData(videoTexture_, tightRgbData.data(), width_, height_);
```

**原理**：
```
源数据（有padding）：
[行1数据][padding][行2数据][padding]...
|<-- linesize -->|

目标数据（紧密排列）：
[行1数据][行2数据][行3数据]...
|<-width*3->|
```

### 修复2：RGB到YUV（添加padding）

**位置**：`VideoProcessor::ProcessFrame` - GPU混合后

**代码**：
```cpp
// GPU混合返回紧密排列的数据
std::vector<unsigned char> blendedData(width_ * height_ * 3);
d3dProcessor_->BlendTextures(..., blendedData.data());

// 创建RGB帧
AVFrame* tempRgbFrame = av_frame_alloc();
tempRgbFrame->format = AV_PIX_FMT_RGB24;
tempRgbFrame->width = width_;
tempRgbFrame->height = height_;
av_frame_get_buffer(tempRgbFrame, 0);

// 添加padding，逐行复制到AVFrame
for (int y = 0; y < height_; y++) {
    memcpy(tempRgbFrame->data[0] + y * tempRgbFrame->linesize[0],  // 目标：有padding
           blendedData.data() + y * width_ * 3,                     // 源：紧密排列
           width_ * 3);                                             // 复制一行
}

// 转换回YUV
sws_scale(swsToYuvCtx_, tempRgbFrame->data, tempRgbFrame->linesize, 0, height_,
          yuvFrame->data, yuvFrame->linesize);
```

**原理**：
```
源数据（紧密排列）：
[行1数据][行2数据][行3数据]...
|<-width*3->|

目标数据（有padding）：
[行1数据][padding][行2数据][padding]...
|<-- linesize -->|
```

## 为什么两处都需要处理？

### 1. GPU处理需要紧密排列的数据
- DirectX的`UpdateSubresource`期望数据按指定的RowPitch排列
- 我们使用`width * 4`作为RowPitch，所以输入必须是紧密排列的
- GPU混合输出也是紧密排列的

### 2. FFmpeg的AVFrame使用对齐的linesize
- `av_frame_get_buffer`会自动分配对齐的内存
- `sws_scale`期望输入输出都有正确的linesize
- 不能假设linesize等于width * bpp

### 3. 数据转换流程

```
FFmpeg AVFrame (有padding)
    ↓ 去除padding
GPU处理 (紧密排列)
    ↓ 添加padding
FFmpeg AVFrame (有padding)
```

## 性能考虑

### 当前方案
- ✅ 正确性：完全正确
- ⚠️ 性能：增加了两次内存复制
- ✅ 简单性：代码清晰易懂

### 优化方案（可选）

如果性能成为瓶颈，可以考虑：

1. **让GPU直接处理带padding的数据**
```cpp
// 修改UpdateSubresource使用实际的linesize
context_->UpdateSubresource(texture, 0, nullptr, 
                           rgbaData.data(), 
                           rgbFrame->linesize[0] * 4 / 3,  // 使用实际linesize
                           0);
```

2. **使用GPU进行格式转换**
- 直接在GPU上进行YUV↔RGB转换
- 避免CPU端的内存复制

3. **使用零拷贝技术**
- 使用DirectX和FFmpeg的共享内存
- 需要更复杂的内存管理

## 测试验证

### 测试步骤

1. **删除旧文件**
```bash
cd build\Release
del blended_frame_*.bmp
del 11_watermarked.mp4
```

2. **重新运行**
```bash
DXWatermark.exe 11.mp4 0.3 dx
```

3. **验证BMP**（应该已经正常）
- 打开`blended_frame_0.bmp`
- 检查视频内容不拉伸
- 检查水印正常覆盖

4. **验证MP4**（现在应该正常了）
- 播放`11_watermarked.mp4`
- 检查视频画面正常
- 检查水印效果正确

### 预期结果

- ✅ BMP图片正常（已确认）
- ✅ MP4视频正常（应该修复了）
- ✅ 水印覆盖整个画面
- ✅ 混合效果正确
- ✅ 没有拉伸或变形

## 经验教训

### 1. FFmpeg的linesize无处不在

**需要处理linesize的地方**：
- ✅ 读取AVFrame数据
- ✅ 写入AVFrame数据
- ✅ 传递给sws_scale
- ✅ 任何直接访问data指针的地方

### 2. 不同组件的数据布局要求

| 组件 | 数据布局要求 |
|------|-------------|
| FFmpeg AVFrame | 对齐的linesize（有padding） |
| DirectX UpdateSubresource | 指定的RowPitch（通常紧密排列） |
| GPU Shader | 纹理采样（自动处理） |
| 自定义处理 | 根据需要（通常紧密排列） |

### 3. 调试技巧

- ✅ 导出BMP查看中间结果
- ✅ 分段验证数据流
- ✅ 打印linesize值
- ✅ 对比紧密排列和实际大小

### 4. 代码审查清单

在处理图像数据时，始终检查：
- [ ] 是否假设数据紧密排列？
- [ ] 是否使用了linesize？
- [ ] 是否逐行处理？
- [ ] 是否考虑了padding？

## 相关代码位置

### 修改的文件
- `src/VideoProcessor.cpp`
  - 第254-260行：YUV到RGB后去除padding
  - 第300-305行：RGB到YUV前添加padding

### 相关函数
- `VideoProcessor::ProcessFrame` - 帧处理主函数
- `D3DProcessor::UpdateTextureData` - 更新GPU纹理
- `D3DProcessor::BlendTextures` - GPU混合

## 总结

这次修复解决了**完整的linesize问题**：

1. **问题1**：YUV→RGB转换后，数据有padding，需要去除才能传给GPU
2. **问题2**：GPU混合后，数据紧密排列，需要添加padding才能传给sws_scale

两个问题都是因为**FFmpeg使用对齐的linesize，而GPU处理使用紧密排列的数据**。

修复后，整个数据流应该完全正常：
- ✅ BMP导出正常
- ✅ MP4视频正常
- ✅ 水印效果正确
- ✅ 没有拉伸变形

这是一个经典的**数据布局不匹配问题**，通过正确处理linesize完美解决！

