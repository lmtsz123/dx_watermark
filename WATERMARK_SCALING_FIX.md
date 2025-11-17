# 水印缩放问题修复说明

## 问题描述

之前的版本中，使用DirectX方法时，水印只出现在视频的中间部分，而不是覆盖整个画面。

## 原因分析

在 `WatermarkRenderer.cpp` 的 `LoadWatermarkFromPNG` 函数中，水印的处理方式是：

### 旧版本行为（问题版本）
1. 加载PNG水印图像
2. **保持宽高比**缩放水印
3. 将缩放后的水印**居中放置**在一个与视频尺寸相同的透明画布上
4. 周围区域保持透明

**结果**：水印只出现在视频中间，周围是原始视频内容（因为透明区域不混合）

示例：
```
视频尺寸: 1920x1080
水印原始尺寸: 500x500
缩放后（保持宽高比）: 500x500
放置位置: 居中 (710, 290) 到 (1210, 790)
其他区域: 透明（显示原始视频）
```

## 修复方案

### 新版本行为（修复后）
1. 加载PNG水印图像
2. **拉伸水印到整个视频尺寸**（不保持宽高比）
3. 水印覆盖整个画面

**结果**：水印覆盖整个视频画面

示例：
```
视频尺寸: 1920x1080
水印原始尺寸: 500x500
拉伸后: 1920x1080（填满整个画面）
放置位置: (0, 0) 到 (1920, 1080)
覆盖范围: 100%
```

## 代码变更

### 修改文件
`src/WatermarkRenderer.cpp` (第207-250行)

### 关键变更

**旧代码**：
```cpp
// 计算缩放比例（保持宽高比）
double scaleX = static_cast<double>(targetWidth) / originalWidth;
double scaleY = static_cast<double>(targetHeight) / originalHeight;
double scale = (std::min)(scaleX, scaleY);  // 使用较小的缩放比例

UINT scaledWidth = static_cast<UINT>(originalWidth * scale);
UINT scaledHeight = static_cast<UINT>(originalHeight * scale);

// ... 缩放 ...

// 计算居中偏移
int offsetX = (targetWidth - scaledWidth) / 2;
int offsetY = (targetHeight - scaledHeight) / 2;

// 将缩放后的水印复制到目标缓冲区（居中）
for (UINT y = 0; y < scaledHeight; y++) {
    for (UINT x = 0; x < scaledWidth; x++) {
        int targetX = x + offsetX;  // 居中放置
        int targetY = y + offsetY;
        // ... 复制像素 ...
    }
}
```

**新代码**：
```cpp
// 直接拉伸到目标尺寸（不保持宽高比，填满整个画面）
UINT scaledWidth = targetWidth;
UINT scaledHeight = targetHeight;

// ... 缩放 ...

// 直接复制整个水印（已经是目标尺寸）
for (UINT y = 0; y < scaledHeight; y++) {
    for (UINT x = 0; x < scaledWidth; x++) {
        int srcIdx = (y * scaledWidth + x) * 4;
        int dstIdx = (y * targetWidth + x) * 4;  // 直接对应，无偏移
        // ... 复制像素 ...
    }
}
```

## 效果对比

### 修复前
```
+----------------------------------+
|                                  |
|        +------------+            |
|        |            |            |
|        |  水印区域  |            |
|        |            |            |
|        +------------+            |
|                                  |
+----------------------------------+
```

### 修复后
```
+----------------------------------+
|                                  |
|                                  |
|         水印覆盖整个画面         |
|                                  |
|                                  |
+----------------------------------+
```

## 使用说明

修复后的版本会自动将水印拉伸到整个视频画面。

### 使用DirectX方法
```bash
# 水印会覆盖整个视频
DXWatermark.exe video.mp4 0.3 dx
```

### 透明度控制
通过调整透明度参数，可以控制水印的可见程度：

```bash
# 淡水印（透明度0.1）
DXWatermark.exe video.mp4 0.1 dx

# 中等水印（透明度0.3，默认）
DXWatermark.exe video.mp4 0.3 dx

# 明显水印（透明度0.5）
DXWatermark.exe video.mp4 0.5 dx

# 强水印（透明度0.8）
DXWatermark.exe video.mp4 0.8 dx
```

## 注意事项

### 1. 水印图像变形
由于水印被拉伸到视频尺寸，如果水印和视频的宽高比不同，水印会被拉伸变形。

**解决方案**：
- 使用与视频相同宽高比的水印图像
- 或者在水印图像中预留透明边距

### 2. 水印设计建议
为了获得最佳效果，建议：

- **使用半透明水印**：让底层视频内容可见
- **使用简单图案**：复杂图案拉伸后可能失真
- **使用重复图案**：适合平铺效果
- **预先调整尺寸**：将水印图像制作成与目标视频相同的宽高比

### 3. 性能影响
拉伸到全屏不会影响性能，因为：
- GPU处理整个画面的速度与处理部分画面相同
- 纹理大小保持不变
- 混合操作在shader中并行执行

## 其他缩放模式（未来扩展）

如果需要其他缩放模式，可以考虑添加：

1. **FIT_CENTER**（旧版本行为）：保持宽高比，居中放置
2. **TILE**：平铺水印到整个画面
3. **CORNER**：在四个角落放置水印
4. **CUSTOM**：自定义位置和大小

这些功能可以通过添加命令行参数实现。

## 测试建议

### 测试步骤
1. 准备一个测试视频（如 1920x1080）
2. 准备一个水印图像（任意尺寸）
3. 运行命令：
   ```bash
   DXWatermark.exe test_video.mp4 0.3 dx
   ```
4. 检查输出视频，确认水印覆盖整个画面

### 验证要点
- [ ] 水印覆盖整个视频画面
- [ ] 水印透明度正确
- [ ] 视频内容在水印下方可见
- [ ] 没有黑边或透明区域
- [ ] 输出视频可以正常播放

## 回滚方法

如果需要恢复到旧版本的居中放置模式，可以：

1. 使用Git恢复旧版本的 `WatermarkRenderer.cpp`
2. 或者手动修改代码，恢复保持宽高比和居中逻辑

## 相关文件

- `src/WatermarkRenderer.cpp` - 水印加载和缩放逻辑
- `shaders/WatermarkPS.hlsl` - 水印混合shader
- `src/D3DProcessor.cpp` - DirectX处理器
- `src/VideoProcessor.cpp` - 视频处理主流程

