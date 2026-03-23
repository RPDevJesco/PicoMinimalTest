# LAFVIN Pico 2 开发套件 — 交互式显示演示

基于 **LAFVIN Pico 2 开发套件** 的交互式硬件演示项目，通过 SPI 总线由 RP2350 微控制器驱动 3.5 英寸 ILI9488 480×320 TFT 显示屏。采用源自 [Tutorial-OS](https://github.com/nicktasios/tutorial-os) 的帧缓冲架构，将图形渲染至 SRAM 中的 RGB565 影子缓冲区，再以 RGB666 格式通过 SPI 推送至显示屏。

## 功能概述

六个交互式场景展示开发板的各个外设：

| 场景 | 内容 |
|------|------|
| **启动画面** | 启动界面，显示系统信息、彩色圆圈、触摸检测状态 |
| **色彩测试** | 点击触摸屏或操作摇杆，循环切换 8 种纯色填充及标签 |
| **图形绘制** | 矩形、圆形、三角形、圆角矩形、放射线条 |
| **渐变效果** | 水平渐变、垂直渐变、全彩虹渐变填充 |
| **文字显示** | 8×8 位图字体，5 种不同缩放比例，完整字符集 |
| **系统信息** | 实时显示触摸坐标、摇杆 ADC 数值、按键状态 |

### 操作方式

- **摇杆左/右** — 切换场景
- **触摸点击** — 下一个场景（在色彩测试场景中循环切换颜色）
- **K2 按键（GP14）** — 开启音频（上行和弦确认）
- **K1 按键（GP15）** — 关闭音频（立即静音）

仅在音频开启时播放导航提示音。每个场景底部的状态栏显示音频状态和当前场景编号。

https://github.com/user-attachments/assets/038a5f96-5b88-49f7-a4ca-1698bde3e856

## 硬件

**开发板：** LAFVIN Pico 2 开发套件，集成 3.5 英寸电容触摸 TFT 屏幕

**主控芯片：** RP2350A（双核 Cortex-M33，150 MHz，520 KB SRAM）

| 外设 | 接口 | 引脚 |
|------|------|------|
| ILI9488 显示屏 | SPI0，10 MHz | GP2（SCK）、GP3（MOSI）、GP4（MISO）、GP5（CS）、GP6（DC）、GP7（RST） |
| 电容触摸屏 | I2C0，400 kHz | GP8（SDA）、GP9（SCL）、GP10（RST）、GP11（INT） |
| 蜂鸣器 | PWM | GP13 |
| K1/K2 按键 | GPIO，低电平有效 | GP15（K1）、GP14（K2） |
| 模拟摇杆 | ADC0/ADC1 | GP26（X 轴）、GP27（Y 轴） |
| RGB LED（WS2812） | —（尚未使用） | GP12 |
| 板载 LED | GPIO | GP25 |

## 系统架构

```
  绘图 API（ARGB8888 颜色格式）
       │
       ▼
  framebuffer.c ──► RGB565 影子缓冲区（SRAM 中 307 KB）
                         │
                         ▼
  display_spi.c ──► RGB565→RGB666 逐行转换（960 字节静态缓冲区）
                         │
                         ▼
                    SPI0 @ 10 MHz ──► ILI9488
```

所有绘图函数接受标准 ARGB8888 颜色（`0xAARRGGBB`）。帧缓冲区以 RGB565 格式存储像素（每像素 2 字节），以适应 RP2350 的 520 KB SRAM 容量。调用 `display_spi_present()` 时，逐行将 RGB565 转换为 RGB666（每像素 3 字节——ILI9488 在 SPI 模式下唯一支持的格式），并通过总线推送。

### 内存布局

| 区域 | 大小 | 用途 |
|------|------|------|
| `.text` | ~27 KB | 代码（帧缓冲绘图、SPI 驱动、字体数据、演示逻辑） |
| `.bss` | ~310 KB | 影子缓冲区（307,200 字节）+ SPI 行缓冲区（960 字节）+ 状态数据 |
| **合计** | **~337 KB** | 520 KB SRAM 中剩余约 183 KB 可用 |

### 关键设计决策

**影子缓冲区，而非直接写入 SPI。** 最初的方案使用栈分配的行缓冲区直接通过 SPI 写入像素。这在嵌套绘图调用时导致栈溢出，并使多帧渲染变得不可靠。影子缓冲区消除了绘图路径中的所有栈分配——所有缓冲区均为静态分配。

**单缓冲，显式提交。** RP2350 没有扫描输出硬件——没有 GPU 自主读取帧缓冲区。CPU 本身就是扫描输出。`display_spi_present()` 是显式的"立即推送此帧"调用，而非缓冲区交换。双缓冲会浪费 520 KB 可用空间中的 307 KB。

**整个事务期间 CS 保持低电平。** 本开发板上的 ILI9488 要求在完整的 CASET→RASET→RAMWR→像素数据序列期间 CS 保持拉低。在命令之间释放 CS 会导致控制器丢失写入窗口——第一帧正常渲染，但后续帧静默失败。这是最难发现的 Bug。

## ILI9488 面板特定发现

以下设置通过在此 LAFVIN 开发板上的反复测试确定：

| 设置 | 值 | 原因 |
|------|----|----|
| `COLMOD` | `0x66`（18 位） | ILI9488 **在 SPI 模式下仅支持 RGB666**。设置 `0x55`（RGB565）会产生乱码。每个像素在总线上占 3 字节。 |
| `MADCTL` | `0x48`（MX + BGR） | 面板的子像素排列为物理 BGR 顺序。不设置 BGR 标志会导致红色和蓝色互换。 |
| `INVON` | `0x21` | 此面板的液晶单元取向需要开启显示反转。否则白色↔黑色互换，所有颜色按位取反。 |
| SPI 速度 | 10 MHz | 更高速度（40 MHz）会产生噪声——电路板走线无法承受。 |
| CS 协议 | 保持低电平 | 在整个命令+数据事务期间 CS 必须保持拉低。详见上文。 |

## 构建方法

### 前置条件

- Docker（推荐），**或者**
- `arm-none-eabi-gcc`、CMake 3.13+、Python 3，以及 [Pico SDK 2.1.1](https://github.com/raspberrypi/pico-sdk)

### 使用 Docker（任意操作系统）

```bash
./build.bat         # 仅构建
./build.sh          # 仅构建
./build.sh flash    # 构建 + 将 .uf2 复制到已挂载的 Pico
./build.sh clean    # 删除构建目录
```

或使用 Make：

```bash
make                # 构建
make flash          # 构建 + 烧录
make clean          # 清理
```

Docker 镜像（`Dockerfile`）包含 ARM 交叉编译器和 Pico SDK，首次构建后会被缓存。

### 不使用 Docker

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s ..
make -j$(nproc)
```

输出文件为 `build/src/pico2_ili9341.uf2`。

### 烧录固件

1. 按住 Pico 2 上的 **BOOTSEL** 按钮
2. 插入 USB 线
3. 将 `pico2_ili9341.uf2` 复制到已挂载的 `RP2350` 驱动器

macOS：`cp build/src/pico2_ili9341.uf2 /Volumes/RP2350/`

Linux：`cp build/src/pico2_ili9341.uf2 /media/$USER/RP2350/`

## 项目结构

```
├── CMakeLists.txt            根 CMake（SDK 初始化，C11/C++17）
├── pico_sdk_import.cmake     Pico SDK 导入辅助脚本
├── Dockerfile                Ubuntu 24.04 + ARM 交叉编译器 + SDK 2.1.1
├── Makefile                  Docker 构建封装
├── build.sh                  Shell 构建脚本
└── src/
    ├── CMakeLists.txt        构建目标、库、编译器选项
    │
    │── hal_types.h           HAL 适配层（内存屏障、volatile、像素格式枚举）
    │── fb_pixel.h            RGB565 ↔ ARGB8888 内联转换函数
    │── framebuffer.h         帧缓冲区结构体 + 完整绘图 API
    │── framebuffer.c         绘图基元（源自 Tutorial-OS）
    │
    │── pins.h                开发板引脚定义
    │── display_spi.h         ILI9488 SPI 后端 API
    │── display_spi.c         硬件初始化、影子缓冲区、SPI 提交
    │
    │── input.h / input.c     K1/K2 按键消抖（GPIO，低电平有效）
    │── buzzer.h / buzzer.c   PWM 音调生成（GP13）
    │── joystick.h / .c       模拟摇杆，ADC 采集（GP26/27，自动校准）
    │── touch.h / touch.c     电容触摸屏（FT6236/GT911 自动检测，I2C）
    │
    └── main.c                6 场景交互式演示
```

## 帧缓冲区 API

帧缓冲区 API 是 [Tutorial-OS](https://github.com/nicktasios/tutorial-os) 帧缓冲驱动的子集，适配为独立 Pico SDK 使用。所有函数接受 ARGB8888 颜色（`0xAARRGGBB`）。在 RP2350 上，通过 `fb_pixel.h` 辅助函数在写入时转换为 RGB565。

全屏提交在 10 MHz SPI 下大约需要 100ms（320×480×3 字节 = 460,800 字节 ÷ 10 Mbit/s）。

## USB 标准输入输出

USB stdio 在编译时已**禁用**（`pico_enable_stdio_usb 0`）。早期测试发现启用它会导致硬件异常——可能是因为 TinyUSB 完全初始化之前触发了未处理的 USB 中断。如需 UART 调试输出，请修改 CMakeLists.txt 中的设置并将串口适配器连接到 UART 引脚。

## 许可证

本项目引用了 Tutorial-OS 的帧缓冲代码。ILI9488 驱动、外设驱动和演示代码均为针对此开发板的原创作品。
