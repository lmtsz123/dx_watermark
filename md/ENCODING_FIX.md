# 编码和透明度问题修复

## 修复的问题

### 问题1：中文显示为 "????"
**原因**：使用了CP_UTF8编码，但Windows命令行传入的参数是GBK/ANSI编码

**修复**：
- 改用CP_ACP（系统默认编码，通常是GBK）
- 添加编码转换错误检查
- 添加调试输出显示转换后的文字

### 问题2：水印不可见
**原因**：画刷的透明度设置为0.3，导致文字本身就很透明

**修复**：
- 将画刷透明度改为1.0（完全不透明）
- 最终透明度由用户的alpha参数控制（在GPU混合时）

## 修改详情

### 1. WatermarkRenderer.cpp

**旧代码**：
```cpp
D2D1::ColorF(D2D1::ColorF::White, 0.3f),  // 画刷透明度0.3
```

**新代码**：
```cpp
D2D1::ColorF(D2D1::ColorF::White, 1.0f),  // 画刷不透明
```

### 2. main.cpp

**旧代码**：
```cpp
// 使用UTF-8编码
int wideLen = MultiByteToWideChar(CP_UTF8, 0, textWatermark.c_str(), -1, nullptr, 0);
```

**新代码**：
```cpp
// 使用系统默认编码（GBK）
int wideLen = MultiByteToWideChar(CP_ACP, 0, textWatermark.c_str(), -1, nullptr, 0);
if (wideLen == 0) {
    std::cerr << "文字编码转换失败" << std::endl;
    return 1;
}
// 添加调试输出
std::wcout << L"转换后的文字: \"" << wideText << L"\"" << std::endl;
```

## 使用方法

### 正确的命令行用法

```bash
# 方法1：直接输入中文（推荐）
DXWatermark.exe video.mp4 0.3 dx "机密文件"

# 方法2：如果方法1不行，先设置代码页
chcp 936
DXWatermark.exe video.mp4 0.3 dx "机密文件"
```

### 预期输出

```
生成文字水印: "机密文件"
转换后的文字: "机密文件"
文字水印生成成功（45度倾斜平铺）
```

如果看到：
- ✅ `转换后的文字: "机密文件"` - 编码正确
- ❌ `转换后的文字: "????"` - 编码错误

## 透明度说明

### 两层透明度控制

1. **画刷透明度**（WatermarkRenderer中）
   - 现在固定为1.0（不透明）
   - 控制文字本身的不透明度

2. **混合透明度**（用户参数）
   - 由命令行参数控制（如0.3）
   - 在GPU混合时应用
   - 控制水印与视频的混合程度

### 效果对比

**修复前**：
```
画刷透明度：0.3
混合透明度：0.3
最终效果：0.3 × 0.3 = 0.09（非常淡，几乎看不见）
```

**修复后**：
```
画刷透明度：1.0
混合透明度：0.3
最终效果：1.0 × 0.3 = 0.3（正常可见）
```

## 测试方法

### 1. 测试编码
```bash
DXWatermark.exe test.mp4 0.3 dx "测试"
```

检查输出：
- 应该看到 `转换后的文字: "测试"`
- 不应该看到 `????`

### 2. 测试可见性
```bash
# 使用较高的透明度测试
DXWatermark.exe test.mp4 0.5 dx "测试水印"
```

播放生成的视频，应该能清楚看到水印。

### 3. 测试不同透明度
```bash
# 淡水印
DXWatermark.exe test.mp4 0.2 dx "测试"

# 中等水印（推荐）
DXWatermark.exe test.mp4 0.3 dx "测试"

# 明显水印
DXWatermark.exe test.mp4 0.5 dx "测试"
```

## 常见问题

### Q1: 还是显示 "????"
**A**: 尝试以下方法：
```bash
# 方法1：设置代码页为GBK
chcp 936
DXWatermark.exe video.mp4 0.3 dx "机密文件"

# 方法2：使用英文测试
DXWatermark.exe video.mp4 0.3 dx "CONFIDENTIAL"
```

### Q2: 水印太淡看不清
**A**: 增加透明度参数：
```bash
# 从0.3增加到0.5或更高
DXWatermark.exe video.mp4 0.5 dx "机密文件"
```

### Q3: 水印太明显影响观看
**A**: 降低透明度参数：
```bash
# 从0.3降低到0.2或更低
DXWatermark.exe video.mp4 0.2 dx "机密文件"
```

### Q4: 英文正常但中文显示乱码
**A**: 这是编码问题，确保：
1. 命令行窗口支持中文
2. 使用 `chcp 936` 设置代码页
3. 或者使用PowerShell而不是CMD

## 推荐设置

### 一般用途
```bash
DXWatermark.exe video.mp4 0.3 dx "机密文件"
```

### 防录屏（明显水印）
```bash
DXWatermark.exe video.mp4 0.5 dx "严格保密"
```

### 版权标记（淡水印）
```bash
DXWatermark.exe video.mp4 0.2 dx "版权所有"
```

## 技术细节

### 编码转换流程
```
命令行输入（GBK）
    ↓
argv[4]（char*，GBK编码）
    ↓
MultiByteToWideChar(CP_ACP, ...)
    ↓
wstring（UTF-16）
    ↓
DirectWrite渲染
    ↓
RGBA位图
```

### 透明度应用流程
```
文字渲染（alpha=1.0）
    ↓
生成RGBA水印（alpha通道=255）
    ↓
GPU混合（使用用户的alpha参数）
    ↓
最终视频（水印透明度=用户参数）
```

## 总结

修复后：
- ✅ 中文正确显示
- ✅ 水印清晰可见
- ✅ 透明度控制正常
- ✅ 支持中英文混合

现在可以正常使用文字水印功能了！

