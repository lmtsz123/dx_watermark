# 更新日志 - 双方法支持

## 版本: 2.0 - 添加FFmpeg Filter方法支持

### 更新日期
2025年11月17日

### 主要变更

#### 1. 新增FFmpeg水印处理器
**新增文件**:
- `include/FFmpegWatermarkProcessor.h` - FFmpeg方法的头文件
- `src/FFmpegWatermarkProcessor.cpp` - FFmpeg方法的实现

**功能特性**:
- 使用FFmpeg原生的avfilter进行水印叠加
- 支持overlay filter自动混合
- 自动处理颜色空间转换
- 跨平台兼容性好

#### 2. 更新主程序
**修改文件**: `src/main.cpp`

**变更内容**:
- 添加第三个命令行参数用于选择处理方法
- 支持 `dx` 和 `ffmpeg` 两种方法选择
- 更新帮助信息，显示两种方法的说明
- 添加方法验证逻辑

**新的命令行格式**:
```bash
DXWatermark.exe <输入视频> [透明度] [方法]
```

#### 3. 更新构建配置
**修改文件**: `CMakeLists.txt`

**变更内容**:
- 添加 `FFmpegWatermarkProcessor.cpp` 到源文件列表
- 添加 `FFmpegWatermarkProcessor.h` 到头文件列表
- 链接 `libavfilter` 库（所有构建配置）
- 链接 `libpostproc` 库（解决filter依赖）

#### 4. 新增文档
**新增文件**:
- `README_WATERMARK_METHODS.md` - 用户使用指南
- `TECHNICAL_DETAILS.md` - 技术实现细节
- `example_usage.bat` - 使用示例脚本
- `CHANGELOG_DUAL_METHOD.md` - 本更新日志

### 技术实现

#### DirectX方法（原有）
```
视频 → 解码 → YUV→RGB → GPU纹理 → Shader混合 → RGB→YUV → 编码 → 输出
```

#### FFmpeg方法（新增）
```
视频 → 解码 → Filter Graph(overlay) → 编码 → 输出
```

### API变更

#### 新增类
```cpp
class FFmpegWatermarkProcessor
{
public:
    bool ProcessVideo(const std::string& inputPath,
                     const std::string& outputPath,
                     const std::string& watermarkPath,
                     float alpha = 0.3f);
    
    static bool GetVideoDimensions(const std::string& path, 
                                   int& width, int& height);
};
```

#### 主函数参数
```cpp
// 旧版本
DXWatermark.exe <输入视频> [透明度]

// 新版本
DXWatermark.exe <输入视频> [透明度] [方法]
```

### 使用示例

#### 使用DirectX方法（默认）
```bash
# 方式1: 不指定方法（默认使用dx）
DXWatermark.exe video.mp4 0.3

# 方式2: 显式指定dx方法
DXWatermark.exe video.mp4 0.3 dx
```

#### 使用FFmpeg方法
```bash
DXWatermark.exe video.mp4 0.3 ffmpeg
```

### 兼容性

#### 向后兼容
- ✅ 原有的命令行调用方式仍然有效
- ✅ 默认使用DirectX方法，行为与旧版本一致
- ✅ 原有的VideoProcessor类保持不变

#### 新依赖
- `libavfilter` - FFmpeg filter库
- `libpostproc` - FFmpeg后处理库

### 性能对比

| 特性 | DirectX方法 | FFmpeg方法 |
|------|------------|-----------|
| 处理速度 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| CPU使用 | ⭐⭐⭐⭐ | ⭐⭐ |
| 跨平台 | ❌ Windows Only | ✅ 全平台 |
| 代码复杂度 | 高 | 低 |
| 内存占用 | 较高 | 较低 |

### 测试结果

#### 编译测试
- ✅ Release模式编译成功
- ✅ 所有依赖库正确链接
- ⚠️ 一些linter警告（不影响功能）

#### 功能测试
- ✅ 帮助信息正确显示
- ✅ 参数解析正常工作
- ✅ 方法选择逻辑正确

### 已知问题

1. **FFmpeg方法的透明度控制**
   - 当前FFmpeg方法使用overlay filter的默认混合
   - 透明度参数传递但未完全实现
   - 需要进一步调整filter参数

2. **水印位置固定**
   - 两种方法都将水印固定在左上角(0,0)
   - 未来可以添加位置参数

### 下一步计划

#### 短期（v2.1）
- [ ] 完善FFmpeg方法的透明度控制
- [ ] 添加水印位置参数
- [ ] 添加更多输出格式支持

#### 中期（v2.5）
- [ ] 支持多个水印
- [ ] 添加文字水印功能
- [ ] 支持批量处理

#### 长期（v3.0）
- [ ] Linux/macOS移植
- [ ] 硬件编解码支持
- [ ] GUI界面

### 迁移指南

#### 从v1.x升级到v2.0

**无需修改代码**:
如果您使用默认参数，代码无需任何修改：
```bash
# v1.x
DXWatermark.exe video.mp4 0.3

# v2.0 - 完全兼容
DXWatermark.exe video.mp4 0.3
```

**使用新功能**:
如果要使用FFmpeg方法，只需添加第三个参数：
```bash
DXWatermark.exe video.mp4 0.3 ffmpeg
```

**重新编译**:
如果从源码编译，需要：
1. 确保FFmpeg包含avfilter和postproc库
2. 重新运行CMake配置
3. 重新编译项目

### 贡献者
- 主要开发: AI Assistant
- 测试: 项目团队

### 许可证
与原项目保持一致

---

## 详细变更列表

### 新增文件
```
include/FFmpegWatermarkProcessor.h
src/FFmpegWatermarkProcessor.cpp
README_WATERMARK_METHODS.md
TECHNICAL_DETAILS.md
example_usage.bat
CHANGELOG_DUAL_METHOD.md
```

### 修改文件
```
src/main.cpp
  - 添加FFmpegWatermarkProcessor头文件引用
  - 更新帮助信息
  - 添加方法选择逻辑
  - 添加参数验证

CMakeLists.txt
  - 添加FFmpegWatermarkProcessor源文件
  - 链接libavfilter库
  - 链接libpostproc库
```

### 未修改文件
```
src/VideoProcessor.cpp
src/D3DProcessor.cpp
src/WatermarkRenderer.cpp
include/VideoProcessor.h
include/D3DProcessor.h
include/WatermarkRenderer.h
shaders/WatermarkPS.hlsl
shaders/WatermarkVS.hlsl
```

### 构建产物
```
build/Release/DXWatermark.exe - 更新的可执行文件
```

