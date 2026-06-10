# WiFi OTA App 下载与 MD5 校验示例

本项目是一个基于 GD32F30x MCU 的 WiFi OTA 升级示例中的 App 侧工程。App 负责通过 WiFi 模块连接 OTA 服务器，上报当前版本，检查是否有新版本，分片下载新固件到 Flash 下载区，完成 MD5 校验后，将升级信息写入 Flash，等待用户确认后复位进入 Bootloader。

配套的 Bootloader 工程负责读取升级信息，将下载区的新固件搬运到 App 区，再次校验 MD5，更新版本号，清除升级标志，并跳转运行新的 App。

## 功能特点

- 通过 UART AT 指令控制 WiFi 模块
- 基于 HTTP 的 OTA 版本检查和固件下载
- 使用 HTTP `Range` 按 512 字节分片下载固件
- 下载完成后进行 MD5 校验
- 使用内部 Flash 持久化保存升级信息
- 支持用户长按按键确认升级
- 通过 Flash 标志完成 App 与 Bootloader 之间的信息传递
- 支持保存当前软件版本号

## 工程结构

```text
22_wifi_ota_app_download_md5/
├── App/
│   ├── main.c          # App 入口、驱动初始化、WiFi 任务、按键确认升级
│   ├── wifi_app.c      # WiFi/HTTP OTA 状态机
│   ├── wifi_app.h
│   ├── store_app.c     # 版本号与 OTA 升级信息存储
│   └── store_app.h
├── Driver/
│   ├── wifi_drv.c      # WiFi 串口/DMA 驱动
│   ├── inflash_drv.c   # 内部 Flash 读写擦除驱动
│   ├── iap_drv.c       # 复位回 Bootloader
│   ├── md5.c           # MD5 算法
│   └── ...
├── GD32_lib/           # GD32 外设库
├── Arm_kernel/         # 启动文件
├── ARM.uvprojx         # Keil uVision 工程
└── Objects/ARM.sct     # 分散加载文件，指定 App 链接地址
```

## OTA 架构

本项目采用 App + Bootloader 双工程 OTA 架构：

```text
App：
  上报当前版本
  检查云端是否有新版本
  下载新固件到 Download 区
  校验固件 MD5
  写入 UpdateInfo，flag = 0
  等待用户确认
  将升级 flag 置为 1
  复位进入 Bootloader

Bootloader：
  读取 UpdateInfo
  检查 magic、CRC、flag 和固件大小
  将 Download 区固件搬运到 App 区
  再次校验 App 区固件 MD5
  更新保存的软件版本号
  清除 UpdateInfo
  跳转到 App
```

App 不会在运行时直接覆盖自己，而是先把新固件下载到独立的 Flash 下载区。真正替换 App 区的动作由 Bootloader 在复位后完成。

## Flash 分区

Flash 分区定义在 `Driver/inflash_drv.h`。

```text
0x08000000  Bootloader 区  28 KB
0x08007000  App 区         240 KB
0x08043000  Download 区    240 KB
0x0807F000  UpdateInfo 区  2 KB
0x0807F800  Parameter 区   2 KB
0x08080000  Flash 结束地址
```

关键说明：

- App 链接地址为 `0x08007000`。
- Bootloader 和 App 必须使用完全一致的 Flash 分区定义。
- `Download 区` 用于临时保存下载的新固件。
- `UpdateInfo 区` 保存固件大小、MD5、目标版本号和升级标志。
- `Parameter 区` 保存当前软件版本号等系统参数。

## 关键模块

### `wifi_app.c`

实现 WiFi OTA 主状态机，主要负责：

- 初始化 WiFi 模块
- 连接 WiFi 热点
- 连接 OTA 服务器
- 上报当前固件版本
- 检查是否存在新版本
- 解析云端返回的 OTA 信息
- 分片下载固件
- 将固件写入 Flash 下载区
- 校验下载固件的 MD5
- 下载成功后调用 `WriteUpdateInfo()` 写入升级信息

### `store_app.c`

提供 Flash 持久化存储接口，主要负责：

- `InitSysParam()`：启动时从 Flash 读取系统参数。
- `GetSoftwareVersionParam()`：获取当前软件版本号。
- `SetSoftwareVersionParam()`：升级成功后写入新的软件版本号。
- `WriteUpdateInfo()`：下载成功后写入 OTA 升级信息。
- `ReadUpdateInfo()`：读取并校验 OTA 升级信息。
- `SetUpdateFlag()`：用户确认升级后设置升级标志。
- `ClearUpdateInfo()`：升级完成后清除升级信息，避免重复升级。

### `main.c`

负责驱动初始化、运行 WiFi OTA 任务，并处理按键长按确认升级：

```c
WifiNetworkTask();

if (keyVal == KEY1_LONG_PRESS)
{
    if (SetUpdateFlag())
    {
        ResetToBoot();
    }
}
```

## 数据校验机制

本项目针对不同数据使用不同校验方式：

- `magicCode`：判断 Flash 区域中是否存在有效结构体数据。
- `CRC8`：校验 `SysParam_t`、`UpdateInfo_t` 等小结构体是否损坏。
- `MD5`：校验下载的新固件是否完整。

App 下载完成后会校验一次 MD5。Bootloader 将固件搬运到 App 区后，也应该再次校验 MD5，确保最终写入 App 区的固件完整可靠。

## 编译方式

本工程面向 Keil uVision / ARMCC。

1. 使用 Keil uVision 打开 `ARM.uvprojx`。
2. 确认目标 MCU 和 GD32 库路径配置正确。
3. 确认 `Objects/ARM.sct` 中 App 链接地址为 `0x08007000`。
4. 编译工程。
5. 将生成的 App 镜像烧录到 App 区。

Bootloader 工程需要单独编译，并烧录到 `0x08000000`。

## 需要修改的配置

实际使用前通常需要修改：

- `App/wifi_app.c` 中的 WiFi SSID 和密码
- OTA 服务器地址、产品 ID、设备名和鉴权字符串
- `Driver/inflash_drv.h` 中的 Flash 分区地址
- `Objects/ARM.sct` 中的 App 链接地址
- 当前软件版本号的保存和更新逻辑

## 当前限制

该工程更适合作为学习和实验用 OTA 示例。如果用于真实产品，建议继续增强：

- 升级过程断电保护
- 失败回滚或 A/B 分区机制
- 固件数字签名校验
- 更安全的字符串长度检查
- 更健壮的 HTTP 响应解析
- 断点续传
- 长时间 Flash 操作期间的看门狗处理
- WiFi 和云端密钥的安全保存

## 安全提醒

当前源码中包含演示用 WiFi 信息和云端鉴权字符串。上传到公开 GitHub 仓库前，请确认已经替换或删除真实的 WiFi 密码、云端 token、产品密钥和设备密钥，避免泄露敏感信息。

## 许可证

当前项目尚未指定许可证。如果希望他人可以使用、修改或分发该项目，建议在上传 GitHub 前添加明确的开源许可证文件。
