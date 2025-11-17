# FFmpeg视频处理逻辑详解

## 为什么有3个while循环？

这是FFmpeg编解码的标准模式，理解它需要了解FFmpeg的**异步编解码机制**。

## 核心概念

### 1. FFmpeg的编解码器缓冲区

FFmpeg的编解码器内部有**缓冲区**：
- **解码器缓冲区**：存储待解码的压缩数据
- **编码器缓冲区**：存储待编码的原始帧

这些缓冲区的存在是为了：
- 支持B帧（双向预测帧）
- 提高编解码效率
- 处理帧重排序

### 2. Send/Receive模式

FFmpeg 4.0+使用新的API模式：
```
发送输入 (send) → 处理 → 接收输出 (receive)
```

**关键特点**：
- `send`和`receive`是**异步**的
- 发送1个packet可能产生0个、1个或多个frame
- 发送1个frame可能产生0个、1个或多个packet

## 完整的处理流程

```
┌─────────────────────────────────────────────────────────────┐
│                     主循环（第1个while）                      │
│                  读取文件，直到文件结束                        │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  while (av_read_frame(...) >= 0)  // 读取压缩的packet       │
│      ↓                                                       │
│  avcodec_send_packet(decoder, packet)  // 发送到解码器      │
│      ↓                                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  while (avcodec_receive_frame(decoder, frame) >= 0) │   │
│  │      ↓                                               │   │
│  │  ProcessFrame(frame) → processedFrame               │   │
│  │      ↓                                               │   │
│  │  avcodec_send_frame(encoder, processedFrame)        │   │
│  │      ↓                                               │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ while (avcodec_receive_packet(encoder, ...) │   │   │
│  │  │     ↓                                        │   │   │
│  │  │ 写入输出文件                                 │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                 刷新解码器（第2个while）                      │
│              取出解码器缓冲区中剩余的帧                        │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  avcodec_send_packet(decoder, NULL)  // 发送EOF信号         │
│      ↓                                                       │
│  while (avcodec_receive_frame(decoder, frame) >= 0)         │
│      ↓                                                       │
│  ProcessFrame(frame) → processedFrame                       │
│      ↓                                                       │
│  avcodec_send_frame(encoder, processedFrame)                │
│      ↓                                                       │
│  while (avcodec_receive_packet(encoder, ...))               │
│      ↓                                                       │
│  写入输出文件                                                 │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                 刷新编码器（第3个while）                      │
│              取出编码器缓冲区中剩余的packet                    │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  avcodec_send_frame(encoder, NULL)  // 发送EOF信号          │
│      ↓                                                       │
│  while (avcodec_receive_packet(encoder, packet) >= 0)       │
│      ↓                                                       │
│  写入输出文件                                                 │
└─────────────────────────────────────────────────────────────┘
```

## 详细解释

### 第1个while循环：主处理循环

```cpp
while (av_read_frame(inputFormatCtx_, packet) >= 0) {
    // 读取一个压缩的packet（可能包含1帧或多帧数据）
    
    avcodec_send_packet(decoderCtx_, packet);  // 发送到解码器
    
    while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
        // 从解码器获取解码后的帧
        // 注意：1个packet可能产生多个frame！
        
        ProcessFrame(frame);  // 处理帧（添加水印）
        
        avcodec_send_frame(encoderCtx_, processedFrame);  // 发送到编码器
        
        while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
            // 从编码器获取编码后的packet
            // 注意：1个frame可能产生多个packet！
            
            av_interleaved_write_frame(...);  // 写入文件
        }
    }
}
```

**为什么需要内层的while循环？**

因为编解码器的输入输出**不是1:1的关系**：

#### 解码器：1个packet → 0~N个frame
```
情况1：普通情况
packet → frame (1:1)

情况2：B帧重排序
packet1 → (缓冲)
packet2 → (缓冲)
packet3 → frame1, frame2, frame3 (1:3)

情况3：需要更多数据
packet → (缓冲，返回EAGAIN) (1:0)
```

#### 编码器：1个frame → 0~N个packet
```
情况1：普通情况
frame → packet (1:1)

情况2：B帧编码
frame1 → (缓冲)
frame2 → (缓冲)
frame3 → packet1, packet2, packet3 (1:3)

情况3：GOP未完成
frame → (缓冲，返回EAGAIN) (1:0)
```

### 第2个while循环：刷新解码器

```cpp
avcodec_send_packet(decoderCtx_, nullptr);  // 发送EOF信号

while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
    // 取出解码器缓冲区中剩余的帧
    ProcessFrame(frame);
    // ... 编码和写入
}
```

**为什么需要？**

因为解码器内部可能还有**缓冲的帧**：
```
文件读取完毕，但解码器缓冲区还有数据：
[已处理的帧] ... [文件结束] [缓冲的帧1] [缓冲的帧2] [缓冲的帧3]
                              ↑
                         需要刷新取出
```

例如：
- B帧需要等待后续的P帧才能解码
- 解码器内部的重排序缓冲区
- 多线程解码的延迟

### 第3个while循环：刷新编码器

```cpp
avcodec_send_frame(encoderCtx_, nullptr);  // 发送EOF信号

while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
    // 取出编码器缓冲区中剩余的packet
    av_interleaved_write_frame(...);
}
```

**为什么需要？**

因为编码器内部可能还有**缓冲的packet**：
```
所有帧处理完毕，但编码器缓冲区还有数据：
[已编码的packet] ... [最后一帧] [缓冲的packet1] [缓冲的packet2]
                                  ↑
                            需要刷新取出
```

例如：
- B帧编码需要等待GOP完成
- 编码器的前向预测缓冲区
- 码率控制的延迟输出

## 为什么ProcessFrame被调用2次？

实际上**不是2次**，而是在**2个地方**调用：

### 调用位置1：主循环中（第409-446行）
```cpp
while (av_read_frame(...) >= 0) {
    while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
        ProcessFrame(frame);  // 处理大部分帧
    }
}
```
**处理**：文件中的大部分帧

### 调用位置2：刷新解码器时（第450-468行）
```cpp
avcodec_send_packet(decoderCtx_, nullptr);
while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
    ProcessFrame(frame);  // 处理解码器缓冲区中的剩余帧
}
```
**处理**：解码器缓冲区中的剩余帧

## 实际执行流程示例

假设有一个5帧的视频：

```
帧序列：I P B B P
```

### 执行过程：

```
步骤1：主循环
├─ 读取packet1 (I帧)
│  └─ 解码 → frame1 (I帧)
│     └─ ProcessFrame(frame1) ✓
│        └─ 编码 → 写入
│
├─ 读取packet2 (P帧)
│  └─ 解码 → frame2 (P帧)
│     └─ ProcessFrame(frame2) ✓
│        └─ 编码 → 写入
│
├─ 读取packet3 (B帧)
│  └─ 解码 → (缓冲，等待P帧)
│
├─ 读取packet4 (B帧)
│  └─ 解码 → (缓冲，等待P帧)
│
├─ 读取packet5 (P帧)
│  └─ 解码 → frame5 (P帧), frame3 (B帧), frame4 (B帧)
│     ├─ ProcessFrame(frame3) ✓
│     ├─ ProcessFrame(frame4) ✓
│     └─ ProcessFrame(frame5) ✓
│
└─ 文件读取完毕

步骤2：刷新解码器
└─ 发送EOF
   └─ (没有剩余帧)

步骤3：刷新编码器
└─ 发送EOF
   └─ 输出缓冲的packet → 写入

总共调用ProcessFrame：5次（每帧1次）
```

## 常见误解

### ❌ 误解1：每个packet对应一个frame
**真相**：1个packet可能产生0个、1个或多个frame

### ❌ 误解2：每个frame对应一个packet
**真相**：1个frame可能产生0个、1个或多个packet

### ❌ 误解3：刷新是多余的
**真相**：不刷新会丢失缓冲区中的数据

### ❌ 误解4：ProcessFrame被重复调用
**真相**：每帧只被处理一次，只是在不同位置调用

## 简化理解

可以把这个过程想象成**流水线**：

```
输入文件 → [解码器缓冲] → 解码 → [处理] → 编码 → [编码器缓冲] → 输出文件
```

3个while循环的作用：
1. **主循环**：持续向流水线输入数据
2. **刷新解码器**：清空解码器缓冲区
3. **刷新编码器**：清空编码器缓冲区

## 代码模板

这是FFmpeg处理视频的**标准模板**：

```cpp
// 1. 主循环
while (av_read_frame(inputCtx, packet) >= 0) {
    avcodec_send_packet(decoder, packet);
    while (avcodec_receive_frame(decoder, frame) >= 0) {
        // 处理frame
        avcodec_send_frame(encoder, processedFrame);
        while (avcodec_receive_packet(encoder, outPacket) >= 0) {
            // 写入outPacket
        }
    }
}

// 2. 刷新解码器
avcodec_send_packet(decoder, NULL);
while (avcodec_receive_frame(decoder, frame) >= 0) {
    // 处理frame
    avcodec_send_frame(encoder, processedFrame);
    while (avcodec_receive_packet(encoder, outPacket) >= 0) {
        // 写入outPacket
    }
}

// 3. 刷新编码器
avcodec_send_frame(encoder, NULL);
while (avcodec_receive_packet(encoder, outPacket) >= 0) {
    // 写入outPacket
}
```

## 总结

- **3个while循环**是FFmpeg视频处理的标准模式
- **第1个**：处理文件中的数据
- **第2个**：刷新解码器缓冲区
- **第3个**：刷新编码器缓冲区
- **ProcessFrame**：每帧只调用1次，但在2个地方（主循环和刷新解码器）
- 这个模式确保**所有帧都被正确处理**，不会丢失数据

理解这个模式后，你会发现几乎所有FFmpeg视频处理程序都遵循这个结构！

