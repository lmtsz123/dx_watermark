# BMP调试导出功能说明

## 功能说明

为了诊断水印混合问题，我添加了自动导出混合后帧的功能。程序会自动保存前5帧的混合结果为BMP文件。

## 使用方法

### 1. 运行程序
```bash
cd build\Release
DXWatermark.exe 11.mp4 0.3 dx
```

### 2. 查看输出
程序运行时会显示：
```
已保存混合后的帧: blended_frame_0.bmp
已保存混合后的帧: blended_frame_1.bmp
已保存混合后的帧: blended_frame_2.bmp
已保存混合后的帧: blended_frame_3.bmp
已保存混合后的帧: blended_frame_4.bmp
```

### 3. 检查BMP文件
在 `build\Release` 目录下会生成以下文件：
- `blended_frame_0.bmp` - 第1帧混合结果
- `blended_frame_1.bmp` - 第2帧混合结果
- `blended_frame_2.bmp` - 第3帧混合结果
- `blended_frame_3.bmp` - 第4帧混合结果
- `blended_frame_4.bmp` - 第5帧混合结果

## 检查要点

### 1. 水印是否覆盖整个画面
打开BMP文件，检查：
- [ ] 水印是否覆盖整个图像
- [ ] 是否只有中间一小块区域有水印
- [ ] 周围区域是否是纯视频内容

### 2. 混合效果是否正确
检查：
- [ ] 水印和视频是否正确混合
- [ ] 透明度是否符合预期
- [ ] 颜色是否正确（没有偏色）
- [ ] 是否有黑边或异常区域

### 3. 水印内容
检查：
- [ ] 水印图案是否清晰
- [ ] 水印是否被拉伸变形
- [ ] 水印透明区域是否正确处理

## 可能的问题和诊断

### 问题1：水印只在中间一小块
**现象**：BMP图像中，水印只出现在中间，周围是纯视频内容

**原因**：
- 水印纹理没有填满整个画面
- 水印加载时保持了宽高比并居中放置
- Shader采样到了透明区域

**解决方案**：
- 检查 `WatermarkRenderer::LoadWatermarkFromPNG` 的缩放逻辑
- 确认水印被拉伸到整个视频尺寸

### 问题2：水印完全不可见
**现象**：BMP图像看起来和原视频一样，没有水印

**原因**：
- 水印的alpha通道全为0（完全透明）
- alpha参数太小
- Shader混合逻辑错误

**解决方案**：
- 检查水印PNG文件的alpha通道
- 增加alpha参数（如0.8）测试
- 检查Shader代码

### 问题3：颜色不对
**现象**：BMP图像颜色偏红/偏蓝/偏绿

**原因**：
- RGB/BGR通道顺序错误
- 颜色空间转换问题

**解决方案**：
- 检查纹理创建时的格式
- 检查BMP保存时的通道顺序

### 问题4：水印太亮或太暗
**现象**：水印覆盖了视频内容，或者几乎看不见

**原因**：
- alpha参数设置不当
- 混合公式错误

**解决方案**：
- 调整alpha参数
- 检查Shader中的lerp公式

## 技术细节

### BMP导出位置
代码位于 `src/D3DProcessor.cpp` 的 `BlendTextures` 函数中：

```cpp
// 保存前5帧的混合结果为BMP（用于调试）
static int frameCount = 0;
if (frameCount < 5) {
    std::vector<unsigned char> bmpData(width_ * height_ * 4);
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int srcIdx = y * mapped.RowPitch + x * 4;
            int dstIdx = (y * width_ + x) * 4;
            bmpData[dstIdx + 0] = src[srcIdx + 0];  // R
            bmpData[dstIdx + 1] = src[srcIdx + 1];  // G
            bmpData[dstIdx + 2] = src[srcIdx + 2];  // B
            bmpData[dstIdx + 3] = src[srcIdx + 3];  // A
        }
    }
    
    std::string filename = "blended_frame_" + std::to_string(frameCount) + ".bmp";
    SaveBMP(filename, bmpData.data(), width_, height_);
    frameCount++;
}
```

### 为什么只保存5帧？
- 避免生成过多文件
- 前5帧足以诊断大部分问题
- 减少对性能的影响

### 如何修改保存帧数？
修改 `D3DProcessor.cpp` 中的条件：
```cpp
if (frameCount < 5) {  // 改为需要的帧数，如10
```

### 如何禁用BMP导出？
注释掉整个if块，或者将条件改为：
```cpp
if (false) {  // 禁用导出
```

## 对比测试

### 同时导出水印纹理
如果需要查看加载的水印，可以在 `WatermarkRenderer::LoadWatermarkFromPNG` 末尾添加：

```cpp
// 保存加载的水印
SaveBMP("loaded_watermark.bmp", outData.data(), targetWidth, targetHeight);
```

### 对比检查
1. 打开 `loaded_watermark.bmp` - 查看加载的水印
2. 打开 `blended_frame_0.bmp` - 查看混合结果
3. 对比两者，确认混合是否正确

## 清理BMP文件

测试完成后，可以删除生成的BMP文件：
```bash
del blended_frame_*.bmp
del loaded_watermark.bmp
```

## 下一步诊断

根据BMP文件的内容，可以确定问题所在：

1. **如果水印只在中间**
   - 修改 `WatermarkRenderer.cpp` 的缩放逻辑
   - 确保水印拉伸到整个画面

2. **如果水印不可见**
   - 检查水印PNG的alpha通道
   - 增加alpha参数
   - 检查Shader混合逻辑

3. **如果颜色不对**
   - 检查RGB/BGR通道顺序
   - 检查纹理格式

4. **如果混合效果不对**
   - 检查Shader中的lerp公式
   - 检查finalAlpha的计算

请运行程序并查看生成的BMP文件，然后告诉我看到了什么，我可以根据实际情况进一步诊断问题。

