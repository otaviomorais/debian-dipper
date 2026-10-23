> v0.1.3: áreas de toque ampliadas, contatos independentes e vibração opcional no Mi 8.
> Console atualizado / Português: [teclado, terminais, Wi-Fi e copiar/colar](docs/console-pt-BR.md).
> Imagem com tudo integrado: `make image` ou a ação manual **Build factory console image**.
> Para atualizar um Mi 8 já instalado: `sudo bash scripts/install-console.sh --autostart`.

# 小米 8（xiaomi-dipper）Debian 13 刷机包 —— 无 GUI 控制台版

面向小米 8 的 **Debian GNU/Linux 13 (trixie) aarch64** 成品镜像：开机直接进入
Linux framebuffer 终端，屏幕底部常驻一个全尺寸虚拟键盘（fbkeyboard），触摸即可打字。

本版本定位为**追求稳定的英文控制台系统**：界面语言英文 `en_US.UTF-8`，**无任何图形界面**
（不含 phosh / phoc / greetd / display-manager），**内核与设备树层面未包含摄像头**
（本版本按设计不含摄像头驱动；若日后需要，可参考
<https://github.com/2114460639/pmos-polaris-fixes> 里 polaris 的启用方式），
并把设备适配（网络、音频、ADB、固件等）固化进镜像，开机即用，无需首启配置。

本镜像的根文件系统基于 polaris（小米 MIX 2S）rootfs 构建，再叠加 **dipper 专属覆盖层**
（`etc/hostname`、`etc/issue(.net)`、`usr/sbin/polaris-usb-gadget`、
`usr/local/sbin/polaris-modem-start`、`usr/share/alsa/ucm2/…` 等，见下方「目录结构」）。
因此镜像里 systemd 单元名仍沿用 `polaris-*`（这是既定事实，未改名），含义对应本机。

- 发行版：Debian GNU/Linux 13 (trixie)，aarch64
- 设备：小米 8（dipper，Qualcomm SDM845，内存约 5.5 GiB 可见）
- 内核：postmarketOS 的 `linux-postmarketos-qcom-sdm845` **7.1\_rc1-r83**
  （版本串 `7.1.0-rc1-sdm845`，`#84`）；在上游基础上打了 dipper 设备树、触控与面板三处补丁
  （`dipper.patch`、`dipper-stmfts5-scan-mode.patch`、`dipper-panel.patch`），
  见 [kernel/README.md](kernel/README.md)
- 构建方式：**官方 Debian 源 + debootstrap**（非 Mobian），第三方预编译件仅为
  pmOS 内核/固件、静态 adbd、fbkeyboard 与 `polaris-keys`

> 刷机会**清空 userdata 分区**，手机内所有数据（照片、文档等）都会丢失，请先备份。

## 功能支持一览

图例：**Y** = 正常可用，**P** = 部分可用，**N** = 不可用。标「本次」的是本轮镜像更新新增/改动的能力。

| 功能                         |  状态 | 注释                                                                                                                                                                                                                                                                                                                              |
| -------------------------- | :-: | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Screen 屏幕 / Touch 触控       |  Y  | **本次**新增 `samsung,ea8074` DSI 面板驱动（DRM + 背光），修复 `panel_vci_vreg`（3.0 V / tlmm35）未上电导致 DRM 接管后全黑的问题；`/sys/class/backlight/ae94000.dsi.0` 可控制背光，`card0-DSI-1 connected`，登录界面正常显示；触控 `stmfts`（i2c-14 0x49）正常注册 |
| 3D GPU                     |  Y  | **本次**把 `qcom/a630_sqe.fw`、`qcom/a630_gmu.bin`、`qcom/sdm845/Xiaomi/dipper/a630_zap.mbn` 预置进 initramfs（msm DRM 在 t≈0.56s 即加载固件，早于 rootfs 挂载），freedreno/turnip 正常绑定 Adreno 630，`/dev/dri/renderD128` 出现；`vulkaninfo` 识别 `Turnip Adreno (TM) 630`，`vkCmdFillBuffer` 4 MiB 实测 6 ms、回读零偏差 |
| Wifi Wi‑Fi                 |  Y  | `wlan0` UP（WCN3990 / ath10k_snoc）。**本次**实测吞吐（iperf3，对端 `192.168.1.31`，5 GHz）：反向（对端→手机）671 / 669 Mbit/s、正向（手机→对端）680 / 678 Mbit/s，全程 `Retr 0`、Cwnd 稳定 8 MB，接近 2×2 / 80 MHz 的实际上限                                                                                                              |
| Bluetooth 蓝牙               |  Y  | `hciconfig -a` 为 **UP RUNNING**，`bluetooth.service` 运行中                                                                                                                                                                                                                                                                           |
| Modem 移动数据 4G              |  Y  | **本次**修复：`&ipa` 节点补回 `qcom,gsi-loader = "self"` 与 `memory-region = <&ipa_fw_mem>`。此前缺 `memory-region` 使 `ipa_firmware_load()` 以 `-ENODEV` 失败、GSI 不启动，`rmnet_ipa0` 不出现，ModemManager 拒绝建 modem；现在 IPA 正常 setup，`qmapmux0.0` 拿到 IP，`mmcli -m 0` 显示 `state: connected` / `access tech: lte` / `CHN-CT` 已注册，4G 实 ping 223.5.5.5 4/4 通、RTT ~29 ms |
| Audio 音频                   |  Y  | UCM `Dipper-HiFi`；扬声器（TAS2557，QUAT_MI2S_RX / MultiMedia3）与内置麦 **AMIC3 → MIC BIAS1** 均已验证；**本次**修复麦克风偏置路由（灵敏度恢复约 +35 dB），增益设为 ADC3 Volume 8 / DEC0 Volume 104（+28 dB）                                                                                                                                                              |
| Swap 内存交换 zram             |  Y  | `/dev/zram0` **4 GiB** zstd，priority 100                                                                                                                                                                                                                                                                                          |
| Power 电源守护（低电量安全关机 / 充电限流） |  Y  | **本次**新增 `polaris-power-guard.service`：低电量时主动干净关机（避免 UFS 非正常断电），插电时把 `pmi8998-charger` 的输入限流从驱动默认 **500 mA** 抬到 1.5 A；阈值见 `/etc/default/polaris-power-guard`                                                                                                                                          |
| USB Net USB 网络             |  Y  | `usb0` UP，设备侧 172.16.42.1                                                                                                                                                                                                                                                                                                       |
| USB OTG USB 主机 / Type-C PD |  Y  | **本次**实测通过：插 OTG 转接即由 tcpm 自动切 host（无需手写 role），`xhci-hcd` 枚举出 U 盘并正常挂载读写（读 26.3 MB/s、写 21.4 MB/s，3.4 GB `squashfs` 的 md5 与盘上 `md5sum.txt` 逐位一致）；插 PD 充电器协商出 **PD 3.0 / PPS 9V 2A（18 W）**，`port0` 转 `[sink]`、电池实际充入约 636 mA。仅验到 USB 2.0 High Speed，PD 未测 20V 档                                                                                                                                                   |
| ADB 直连                     |  Y  | `polaris-adbd.service` 运行中，`adb devices` 显示 `dipper`                                                                                                                                                                                                                                                                             |
| Keyboard 虚拟键盘              |  Y  | `fbkeyboard.service` 运行中。**本次**改为「抬起释放」（按下只高亮、抬手才发键，与 MIX 2S 一致），并修复「按下去不释放」：抬手判定不再要求触发抬手的 slot 恰好等于 `primary_slot`，并新增 `SYN_DROPPED` 处理（内核输入缓冲区溢出丢掉抬手时只清高亮、**不补发按键**），每轮把已排队帧一次排空以消除积压。长按连发仅 `Bcksp`/方向键，抬起后不补发 |
| Keys 电源 / 音量键              |  Y  | **本次**修复电源键并实测通过：原脚本硬编码 `/sys/class/backlight/backlight`（polaris 的名字），dipper 是 `ae94000.dsi.0`，每次按电源键都 `FileNotFoundError`、服务被 systemd 反复重启；改为运行期自动发现背光。连按 8 次 → 日志 8 条、亮度严格按 40%→80%→熄 循环（`409/818/0`）。音量键实测也全对：短按注入方向键（`kbd event5`，`tap -> arrow 103/108`）、长按调 PulseAudio 音量（0.4s 触发后每 0.2s ±5%，50%→130%→45% 与日志档数吻合） |
| Polkit 普通用户免密管理网络          |  Y  | 沿用 polaris 的 `/etc/polkit-1/rules.d/49-polaris-network.rules`                                                                                                                                                                                                                                                                     |
| Suspend 挂起 / 休眠            |  N  | 按设计**永久禁用**，**本次实测确认**：`sleep` / `suspend` / `hibernate` / `hybrid-sleep` / `suspend-then-hibernate` 五个目标 unit 全部 `masked`，logind 空闲与各按键动作全部 `ignore`。空闲 10 分钟由内核 `consoleblank=600` **完全熄灭屏幕**（DPMS 下电，非仅关背光）                                                                                                                            |
| Camera 摄像头                 |  N  | 按设计不含（内核与设备树层面未包含）。需要时可参考 <https://github.com/2114460639/pmos-polaris-fixes> 里 polaris 的启用方式                                                                                                                                                                                                            |

## 目录结构

```
debian-dipper-flash-console/
├── README.md                    本文档：刷机 + 全部适配方法
├── flash.sh                     Linux 一键刷机脚本
├── flash.bat                    Windows 一键刷机脚本
├── images/
│   ├── boot.img                 内核 + 设备树 + initramfs（刷到 boot 分区）
│   ├── xiaomi-dipper.img        系统镜像，刷到 userdata 分区
│   │                            （**内容是 Android sparse 格式**，不是 raw ext4，
│   │                            不要拿去 loop 挂载 / e2fsck）
│   └── *.md5                    镜像校验值
├── kernel/                      内核方法：dipper 设备树 / 触控补丁、APKBUILD、内核 config
│   ├── dipper.patch             dipper DTS + 麦克风偏置修复 + wcn3990-pmu 移除
│   ├── dipper-stmfts5-scan-mode.patch  触控 stmfts 扫描模式修正
│   ├── fbcon-scrollback.patch   fbcon 回滚（虚拟键盘翻日志用）
│   ├── APKBUILD / config-postmarketos-qcom-sdm845.aarch64
│   └── README.md                构建步骤与改动说明
├── rootfs/                      镜像内定制的配置文件（保持原始路径）
│   ├── etc/systemd/system/      fbkeyboard / polaris-* / qc-* 单元、睡眠 mask
│   ├── etc/systemd/network/     usb0 固定 172.16.42.1
│   ├── etc/NetworkManager/      WiFi MAC 固定、usb0 不交给 NM、4G 连接 Mobile4G
│   ├── etc/default/             polaris-power-guard 阈值（低电量关机 / 充电限流）
│   ├── etc/polkit-1/rules.d/    普通用户免密管理 NetworkManager
│   ├── etc/issue               登录界面来源标注（Build by …/debian-dipper）
│   ├── usr/local/sbin/polaris-modem-start  MPSS 启动脚本（dipper 覆盖层版本）
│   ├── usr/sbin/polaris-usb-gadget         USB gadget 配置（dipper 覆盖层版本）
│   ├── usr/sbin/polaris-power-guard        低电量安全关机 + 充电功率控制
│   └── usr/share/alsa/ucm2/     ALSA UCM：Dipper-HiFi.conf + conf.d/sdm845/Xiaomi Mi 8.conf
├── scripts/
│   ├── mk_sparse_fill.py        raw → Android sparse（RAW+FILL，无空洞）
│   ├── repack_boot.py           只换 boot.img 的 kernel 段
│   └── apply_local_overlay.sh   把 local/rootfs/ 注入 raw ext4 镜像（本机私有配置）
├── fbkeyboard/                  虚拟键盘源码（fbkeyboard.c + Makefile）
├── tools/
│   ├── linux/    adb、fastboot、lib64、51-android.rules（udev 规则）
│   └── windows/  adb.exe、fastboot.exe 及所需 DLL
└── drivers/
    └── windows/usb_driver/      Google USB 驱动（Windows 识别 fastboot 设备用）
```

> `.gitignore` 忽略 `*.img`、`*.apk`、`tools/`、`drivers/`、`local/`：镜像与第三方二进制不进 git，
> 本机私有 overlay（WiFi 密码等）也不入库。

> 原始 raw ext4 镜像不随刷机包携带，放在编译产物目录
> `/home/wxs/debian-dipper/out/xiaomi-dipper-2g.img`（2254856192 字节 ≈ 2150 MiB），
> 需要 loop 挂载 / e2fsck 检查时用那份。

## 镜像从哪来

仓库里**只放方法**（脚本、文档、补丁、配置），镜像和第三方二进制统一发在
[Releases](https://github.com/2114460639/debian-dipper/releases)。

全部刷机所需已打进一个 `debian-dipper-flash-console.7z`（压缩后大小与 sha256 见
[Releases](https://github.com/2114460639/debian-dipper/releases)）：

- `images/boot.img`（25 MiB）、`images/xiaomi-dipper.img`（≈1.64 GiB，**Android sparse 格式**）、`images/*.md5`
- `flash.sh` / `flash.bat`
- `tools/`（adb、fastboot）、`drivers/`（Windows USB 驱动）
- `kernel/` `rootfs/` `scripts/` `fbkeyboard/`（与仓库同源的适配方法）

```bash
sha256sum -c debian-dipper-flash-console.7z.sha256     # 校验值随 Release 提供
7z x debian-dipper-flash-console.7z
cd debian-dipper-flash-console && ./flash.sh           # 输入 yes
```

> 为什么压成 7z：GitHub 单文件上限 100 MB，1.6 GiB 的镜像没法直接放仓库；
> 打成一个 7z 既绕开限制，又大幅缩小体积，慢速网络也传得动。
> 解压后的 `images/*.md5` 可以直接 `md5sum -c` 再校验一遍。

`tools/`、`drivers/` 是第三方二进制（**二进制 + 第三方许可证**），不进 git。
如果你是 `git clone` 拿的仓库（没下载 Release），需要自行准备：

- Linux：`sudo apt install android-tools-adb android-tools-fastboot`
- Windows：下载 [platform-tools](https://developer.android.com/tools/releases/platform-tools)
  解压到 `tools/windows/`，驱动用 `drivers/windows/usb_driver/`

`flash.sh` 在随包工具不存在时会自动回退到系统 `PATH` 里的 fastboot / adb。

## 刷机步骤

1. 用数据线连电脑，直接运行脚本即可：
   - Linux：`./flash.sh`
   - Windows：双击 `flash.bat`（首次需先装 `drivers/windows/usb_driver` 里的驱动）
2. 脚本按三级顺序自动找设备，**不需要你手动进 fastboot**：
   1. 先看有没有 fastboot 设备，有就直接开始刷；
   2. 没有就看有没有 adb 设备，有就 `adb reboot bootloader`，然后每秒探测一次、
      最多等 60 秒等它进入 fastboot；
   3. adb 也没有、或 60 秒后仍未进 fastboot，才提示你手动进入
      （音量下 + 电源）。
3. 输入 `yes` 确认后脚本会依次：刷 `boot` → **`fastboot erase userdata`** → 刷系统镜像
   → 用 `fastboot reboot` 引导系统（失败自动退回 `fastboot continue`）。`erase` 是必须的
   （原因见下方「手动刷机」的说明），只要约 6 秒。**引导阶段可能较慢**：设备要把刚刷入的
   数据落盘到 UFS，可能持续几分钟。**首次开机**会自动把根文件系统扩容到整块 `userdata`
   （并生成 SSH 主机密钥），约 1\~2 分钟。

### Windows 驱动安装（仅首次）

设备管理器里找到带感叹号的 `Android` 设备 → 右键「更新驱动程序」→「浏览我的电脑」→
选择 `drivers/windows/usb_driver` 目录。

### Linux 权限

若提示 `no permissions`，安装随包的 udev 规则：

```bash
sudo cp tools/linux/51-android.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 手动刷机（不用脚本）

```bash
fastboot flash boot     images/boot.img
fastboot erase  userdata      # 必须先清空分区，见下方说明
fastboot flash userdata images/xiaomi-dipper.img
fastboot reboot               # 若卡住/失败，再执行 fastboot continue
```

> **刷 userdata 之前必须先** **`fastboot erase userdata`。**
> 本机 bootloader **从不写入全零数据**：FILL chunk 整个跳过，连 RAW chunk 里的全零块也
> 一样跳过（用 `cmp` 比对刷写前后的设备数据验证过，与 chunk 大小、类型都无关）。
> erase 走 discard、只要约 6 秒，清完之后被 bootloader 跳过的零区本来就是零，文件系统
> 才与镜像一致。不 erase 直接覆盖刷写的话，旧 Android 数据会留在「镜像要求为 0」的位图区，
> 每次开机 e2fsck 都报
> `ext2fs_check_desc: Corrupt group descriptor: bad block for block bitmap`
> 并强制约 3.5 分钟的全盘检查。
>
> **`images/xiaomi-dipper.img`** **名字叫** **`.img`，内容其实是 Android sparse 镜像**
> （按官方 postmarketOS 刷机包格式预先做好：只有 RAW/FILL chunk、没有 don't-care 空洞）。
> fastboot 会原样发给 bootloader 原生解析，可正常启动。别拿它当 raw 镜像去 loop 挂载 /
> e2fsck；需要 raw 镜像（loop 挂载、e2fsck 检查）时去编译产物目录取
> `/home/wxs/debian-dipper/out/xiaomi-dipper-2g.img`。
>
> 千万别换成 raw ext4 镜像让 fastboot 现场转换：现场转换会把全零块当成 don't-care 空洞，
> 实际写进 userdata 的内容会与镜像不一致。
>
> `boot.img` 的 cmdline 带 `fsck.repair=yes`：轻微不一致会自动 `fsck -y` 修复后继续
> 启动。
>
> **刷完后用** **`fastboot reboot`** **引导系统**（脚本里也用 `timeout 60` 兜底，超时或失败则
> 自动退回 `fastboot continue`）。本机 bootloader 偶发「接受了 `fastboot reboot` 命令、
> 复位后却**回落进 fastboot、进不了系统**」的情况，宿主侧 fastboot 进程也可能一直卡住
> 不返回（实测 15 分钟仍不返回），所以脚本对 `reboot` 加了 60 秒超时；`fastboot continue`
> 则是让 ABL 直接引导刚刷入的镜像，作为兜底。引导阶段设备要把刚刷入的数据落盘到 UFS，
> **可能持续几分钟**（刷机传输被背压时更明显），屏幕可能先黑后亮，属正常现象。若手机仍停在
> fastboot，**长按电源键 12\~15 秒**物理复位即可（数据已经写完了，不会丢）。

## 系统功能介绍

- **开机即终端**：启动后屏幕就是 Linux framebuffer 控制台（`simple-framebuffer`，
  1080×2248，显存取自 `cont_splash`），最后停在 `dipper login:` 提示符（tty1）。
  目前无 DSI/DRM 面板驱动（EA8074 面板驱动待做），`/sys/class/backlight` 为空、
  **没有背光控制**。
- **登录界面标注来源**：登录提示符上方显示一行 `Build by https://github.com/2114460639/debian-dipper`，
  由 `/etc/issue`（本地 console）与 `/etc/issue.net`（远程）提供，说明镜像来源。
- **登录账号**：
  - 普通用户 `user`，密码 `password`（uid 1000，可登录）；
  - `root` 密码已锁定，请用 `sudo`；`user` 已在 sudo 白名单中，**任意命令免密**
    （由 `/etc/sudoers.d/00-user-nopasswd` 提供，随镜像固化）。
  - 登录后主机名是 `dipper`；会话由 logind 正常管理（镜像含 `libpam-systemd`）。
- **常驻虚拟全键盘（fbkeyboard）**：在屏幕下半部分绘制 QWERTY 全键位键盘，通过 uinput
  注入按键，开机自启（`fbkeyboard.service` 已 enable），触摸/指针均可点击。
  - **输入方式为「抬起释放」**（与 MIX 2S 一致）：按下只做高亮、**不发键**，手指抬起时
    才把「抬起位置所在的键」发出去；手指按住后滑到别的键，抬起时发的是新键、不会补发
    原来的键；滑出键盘区域抬起则什么也不发。
  - 键盘**上方**那条区域是隐藏的 3×3 导航格（无视觉提示，直接点即可）：
    上排 `Home / ↑ / 日志上翻`、中排 `← / Enter / →`、下排 `End / ↓ / 日志下翻`；
    注意正中间是 `Enter`，容易误触。
  - 隐藏格**右上 / 右下**两键是**滚动控制台日志**：发送 `Shift+PageUp` / `Shift+PageDown`，
    点一下向上/向下翻一屏，可回看之前的输出。
    注意：主线 fbcon **没有实现** `con_scrolldelta`（`struct consw fb_con` 缺这一项），
    所以原生内核下这两个键按下去毫无反应。本包内核打了自研补丁
    `fbcon-scrollback.patch`：内核里加一条环形历史缓冲（4096 行），`fbcon_scroll()`
    在整屏上滚时把被顶出的行存进去，再注册 `fb_con.con_scrolldelta` —— 收到回滚请求
    就按「历史行 + 实时内容」整屏重绘（按属性分段 `putcs`，与 `fbcon_redraw()` 同款）。
    新的控制台输出会自动落回实时画面，切 VT 也会归零；历史缓冲只在
    `fbcon_init`/`fbcon_resize` 这类可睡眠上下文里分配，滚动路径零分配。
    内核版本 `7.1.0-rc1-sdm845 #84-postmarketos-qcom-sdm845`（pkgrel 83）。
  - 另外还有 `Esc / Tab / F10` 与 `Shift / Ctrl / Alt` 等功能键行。
  - **长按连发**：`Bcksp` 与四个方向键（`↑ ↓ ← →`）按住不放会持续生效——按住约 0.4 秒
    后开始连发（约每 80 毫秒一次），松手即停；连发过的键抬起时**不再补发**（否则会多出
    一个字符）。其余按键只在抬起时触发一次。
  - **修复「按下去不释放」**（**本次**）：基础镜像自带的 fbkeyboard 有两个缺陷会把手感
    永久卡在「按下」状态——高亮不消失、连发键无限重复，终端里也就跟着刷出一长串重复字符：
    1. 抬手判定要求触发抬手的 slot 恰好等于正在跟踪的 `primary_slot`，一旦上报顺序里
       `ABS_MT_SLOT` 与 `ABS_MT_TRACKING_ID` 交错（多指或驱动上报次序变化）就会漏掉抬手；
    2. 完全不处理 `EV_SYN/SYN_DROPPED`：主循环每轮只读一帧、重绘又要 10~25 ms，触摸密集
       时事件在内核输入缓冲区积压溢出，`SYN_DROPPED` 之后抬手事件（`ABS_MT_TRACKING_ID=-1`）
       被丢掉，于是永久停在按下状态。现在收到 `SYN_DROPPED` 就整体复位触点、只清高亮
       **绝不补发按键**（否则会凭空多出字符），并且每轮把已排队的帧一次排空（上限 32 帧），
       既消除了卡键也顺手解决了高亮/输入滞后于手指的问题。
- **大号控制台字体**：内核内置字体 8x16 在 1080×2248 屏上字非常小；本镜像改用
  Terminus **16x32**（正好 2 倍），字符尺寸翻倍、列/行数相应减半。开机自动生效：
  - 主机制：`/etc/default/console-setup` 设为 `FONTFACE="Terminus" FONTSIZE="16x32"`，
    CHARMAP=UTF-8 → CODESET=Uni2，故开机由 udev 在 framebuffer 控制台注册阶段执行
    `/etc/console-setup/cached_setup_font.sh` 加载
    `/usr/share/consolefonts/Uni2-Terminus32x16.psf.gz`（文件名是「高x宽」= 32 高 × 16 宽）；
  - 保险：`fbkeyboard.service` 的 drop-in 在 `console-setup.service` 之后再对 `/dev/tty1`
    显式 `setfont`，确保登录用的那个控制台一定是大字体。
- **电源键 / 音量键**（`polaris-keys.service`）：**本次**修复了电源键。
  原脚本（继承自 polaris）把背光目录硬编码为 `/sys/class/backlight/backlight`
  （MIX 2S 的名字），而 dipper 是 `/sys/class/backlight/ae94000.dsi.0`，于是每次按
  电源键都抛 `FileNotFoundError`、服务被 systemd 反复重启（`NRestarts` 不断增长）。
  现在改为运行期用 `find_backlight()` 自动发现背光设备，并对背光读写全程做容错：
  按电源键一次 = 40% → 80% → 熄 → 40% 循环（开机时若 systemd-backlight 恢复出
  极暗的 ~1% 亮度，`ensure_min()` 会先抬到 40%）。
  音量键沿用 polaris 配置，**本次一并实测通过**：短按注入方向键（`tap -> arrow 103/108`，
  uinput 设备 `polaris-keys` 带 `kbd` handler，箭头确实进控制台），长按调 PulseAudio 音量
  （`runuser -u user -- env XDG_RUNTIME_DIR=/run/user/1000 pactl set-sink-volume`；0.4s 触发，
  之后每 0.2s ±5%，实测 50%→130%→45% 与日志档数吻合）。硬件无可调 mixer，只能用软件音量。
  另外把该 unit 从 `Restart=on-failure` 改成 **`Restart=always`**（`RestartSec=1` 已有）：
  基础镜像继承的 on-failure 把 SIGTERM / 正常 `exit 0` 视为成功，进程一旦退出就永久停掉，
  无头机上电源键和音量键会一起失效、只能重启恢复；实测发 SIGTERM 后 systemd 自动拉起
  （`MainPID` 变化、`NRestarts=1`）。该 unit 原先不在 `apply-dipper-overlay.sh` 的注入清单里
  （来自 polaris 基础镜像），现已补上注入项。
- **永不挂起 / 十分钟完全熄屏**（**本次实测确认**）：
  - `logind` 已设 `IdleAction=ignore`、`IdleActionSec=0`，盖子/挂起/休眠键全部忽略，
    `suspend` / `hibernate` / `hybrid-sleep` / `suspend-then-hibernate` / `sleep.target`
    均已 mask 到 `/dev/null`，系统**永不挂起或休眠**。实测 `systemctl list-unit-files`
    这 5 个 unit 全部为 `masked`，`/etc/systemd/logind.conf.d/99-polaris.conf` 里
    `HandlePowerKey( LongPress )`、`HandleSuspendKey( LongPress )`、
    `HandleHibernateKey( LongPress )`、`HandleLidSwitch*`、`IdleAction` 全部 `ignore`；
  - 内核命令行加 `consoleblank=600`：空闲 10 分钟后**完全熄灭屏幕**——VT blanking 会让 fbcon
    走 `VESA_POWERDOWN` 把显示整体下电，实测背光设备 `bl_power=4`（`FB_BLANK_POWERDOWN`，
    背光被强制关断）、`/sys/class/graphics/fb0/blank=1`，`brightness` 保持原值不丢；按任意键
    唤醒后恢复 `bl_power=0`、`brightness=818`（40% 档）。这是**整屏 DPMS 下电**，不是"只关背光"，
    面板供电轨（`panel_vci_vreg` / `panel_vddio_vreg`，`use_count` 1/2）不会撤除，属正常行为。
    `HandlePowerKey=ignore` 把电源键让给 `polaris-keys` 循环亮度，不会因误按关机。
- **联网**：
  - **USB 网络**：设备侧固定 `172.16.42.1/24`，并自带 DHCP 服务，电脑插上线一般会自动
    拿到 `172.16.42.2`；由 systemd-networkd 管理 `usb0`（NetworkManager 已通过
    `unmanaged-devices` 忽略 `usb0`，两边不打架）。`usb0` 已确认 UP。
  - **Wi‑Fi**：NetworkManager 已启用，自带 `nmtui`、`nmcli`、`iw`、`rfkill` 等；
    `wpa_supplicant`、`bluetooth` 也已启用。WCN3990（`ath10k_snoc`）驱动绑定后
    `wlan0` 呈现为 **UP**。WCN3990 固件按内核实际查找的标准路径放置
    （`ath10k/WCN3990/hw1.0/…`、`qca/…`、`regulatory.db` 等）。
    > 4G 已修好（见下），但 polaris 上那条「WiFi 依赖 MPSS/WLFW」的完整链路在
    > dipper 上的对应关系**未逐一验证**；本机以「`wlan0` 已 UP」为准。
  - **普通用户可直接改网络（无需 sudo）**：`user` 已在 `netdev` / `sudo` 组，镜像另加
    `/etc/polkit-1/rules.d/49-polaris-network.rules`，把 `org.freedesktop.NetworkManager.*`
    下的**全部动作**授予这两个组。因此不必 `sudo`，`user` 即可 `nmtui` / `nmcli` 连接 Wi‑Fi、
    开关 radio、连接/断开设备、扫描、增删改连接等。
    验证：以 `user` 身份执行 `nmcli general permissions`，应全部为 `yes`。
    背景：NetworkManager 自带规则只放开 `settings.modify.system`，且限定
    `subject.local && subject.active`（本地会话）；其余动作默认 `auth_admin`，
    而本系统没有图形 polkit agent，一旦落到 `auth_admin` 就直接失败。
  - **WiFi/蓝牙 MAC 固定**（bootmac）：`wlan0`/`hci0` 出现时由 udev 规则
    `90-bootmac-{wifi,bluetooth}.rules` 拉起 `bootmac@.service`，脚本 `/usr/bin/bootmac`
    从内核 cmdline 的 `androidboot.serialno` 派生确定性 MAC（本地管理地址，前缀 `02:00:`），
    不再每次开机随机（机制沿用 polaris；dipper 的具体 MAC 未记录）。
    验证：`cat /sys/class/net/wlan0/address` 开机两次应一致。
  - **移动数据（4G / SIM）**：**本次已修好**。根因是设备树的 `&ipa` 节点缺
    `memory-region`：`ipa_firmware_load()` 拿不到预留内存就返回 `-ENODEV`，GSI 不启动，
    `rmnet_ipa0` 不出现，ModemManager 的 `qcom-soc` 插件因找不到 net port 而拒绝建
    modem（`Failed to find a net port in the QMI modem`）。补上
    `qcom,gsi-loader = "self"` + `memory-region = <&ipa_fw_mem>`（与 polaris 上游一致）后：
    IPA `setup completed successfully`，`rmnet_ipa0` + `qmapmux0.0` UP，
    `mmcli -m 0` 显示 `state: connected` / `access tech: lte` / `CHN-CT` 已注册、
    信号 92%，`qmapmux0.0` 拿到 `10.62.69.152/28`，`ping -I qmapmux0.0 223.5.5.5`
    4/4 通、RTT ≈29 ms。这条 IPA 通路也是 `polaris-modem-uim.service`（QMI 建
    provisioning session）能正常工作的前提。
  - **SSH**：默认开启，`ssh user@172.16.42.1`，密码 `password`。主机密钥在首启自动生成
    （镜像内不含任何密钥，machine-id 亦为空），由自建的 `polaris-ssh-keygen.service` 负责：
    Debian 自带的 `sshd-keygen.service` 依赖 `ConditionFirstBoot=yes`，而 systemd 启动时会
    先把空的 `/etc/machine-id` 填上，导致该条件恒为假、主机密钥永不生成，本镜像已绕过。
  - **ADB 直连**：内置静态 `adbd`（`polaris-adbd.service` 开机自启，复用 pmOS 的
    functionfs 接线脚本），USB 线插电脑即可 `adb devices`（可看到设备名 `dipper`）/
    `adb shell` / `adb push` / `adb pull`，与 NCM 网络共存、互不影响。
    `adb shell` 可直接得到 root shell 提示符并默认位于 `/root`、方向键/Ctrl-C 行编辑正常。
- **蓝牙**：BlueZ 开机自启；WCN3990 固件（`qca/crbtfw21.tlv` + 设备专属
  `qca/dipper/crnv21.bin`）加载后控制器正常启动，`hciconfig -a` 为 **UP RUNNING**、
  `bluetooth.service` 运行中，`bluetoothctl` 可扫描/配对，MAC 由 `bootmac@bluetooth` 固定；
  蓝牙音频（A2DP 耳机/音箱）由内置的 `pulseaudio-module-bluetooth` 提供。
  > **注意**：本机（dipper）蓝牙**已实测可用**。这与 polaris 老笔记里
  > 「SOC 蓝牙不可用」的旧结论**不同**，请勿照抄那条错误结论。
- **音频**：ALSA UCM 层（`/usr/share/alsa/ucm2/Qualcomm/sdm845/Dipper-HiFi.conf`，
  经 `conf.d/sdm845/Xiaomi Mi 8.conf` 按声卡名自动加载），PulseAudio 开机接管。
  - **扬声器**：TAS2557 智能功放，播放前端 `QUAT_MI2S_RX` / `MultiMedia3`（hw:0,2），
    已验证。
  - **内置麦**：AMIC3 → ADC3 → DEC0 → AIF1_CAP → SLIMBUS_0_TX → MultiMedia2（hw:0,1）。
    **本轮修复**麦克风偏置路由：内置麦胶囊由 **MIC BIAS1** 供电（不是上游从 beryllium
    照抄的 MIC BIAS3），设备树已改为 `AMIC3 → MIC BIAS1`，灵敏度**恢复约 +35 dB**、
    不再被噪声淹没；增益设为 `ADC3 Volume 8` / `DEC0 Volume 104`（合计 +28 dB，
    正常语音在 3–5 cm 处约 -26 dBFS，留有充足裕量）。已验证。
  - `pactl` 可用（`polaris-keys` 的长按调音量依赖它，但该按键行为未在 dipper 上验证）。
- **界面语言**：英文 `en_US.UTF-8`（未装中文字体）。
- **稳定性加固**：systemd 硬件看门狗 `RuntimeWatchdogSec=30` / `RebootWatchdogSec=30`；
  内核 `panic=120`、`panic_on_oops=1`、`panic_on_rcu_stall=1`（卡死自动重启）；
  `fs.protected_regular=0`。
- **内存与交换（zram）**：开机自动启用一块 **4 GiB、zstd 压缩**的 zram 交换设备
  （`/dev/zram0`，priority 100），由开机自启的 `polaris-zram-swap.service`
  （`WantedBy=swap.target`）调用 `/usr/sbin/polaris-zramswap` 创建。
  验证：`zramctl`、`cat /proc/swaps`、`free -h`（Swap 一栏显示 4.0 GiB）。
- **常用工具**：`nano`、`less`、`iw`、`rfkill`、`e2fsprogs`、`usbutils`、`iproute2`、
  `alsa-utils`、`python3` 等；**已预装** **`fastfetch`、`iperf3`**，并沿用 polaris 的
  `libvulkan1` + `mesa-vulkan-drivers`（turnip / freedreno ICD）——**本次**已验证
  Adreno 630 在 turnip 下可正常渲染（`vulkaninfo` / `vkCmdFillBuffer`）。系统用官方
  Debian 源，需要别的软件直接 `sudo apt update && sudo apt install <包名>` 即可
  （`apt` 可用，免密 sudo）。

## 分区与文件系统

| 镜像                    | 刷入分区       | 内容                                                                                                                                            | 大小                                                              |
| --------------------- | ---------- | --------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| `boot.img`            | `boot`     | 内核 `7.1.0-rc1-sdm845`（#84，含 `dipper.patch` / `dipper-stmfts5-scan-mode.patch` / `dipper-panel.patch`）+ 追加 `sdm845-xiaomi-dipper.dtb` + initramfs（含 A630 GPU 固件）                     | 25,858,048 B                                                    |
| `xiaomi-dipper.img`   | `userdata` | Debian 根文件系统（ext4，4096 字节块，**首启自动扩容到整块 userdata**，卷标 `dipper-root`），**Android sparse 格式**                                          | 1,762,014,360 B (≈1680 MiB / 1.64 GiB，声明覆盖 550502 个 4K 块 ≈ 2150 MiB) |

> 同一份根文件系统的 raw ext4 版为
> `/home/wxs/debian-dipper/out/xiaomi-dipper-2g.img`（2254856192 字节 ≈ 2150 MiB）。
> 不随包携带，需要 loop 挂载 / e2fsck 检查时用那份。

- 根分区：`/dev/sda21`，卷标 **dipper-root**。
- 内核 cmdline：
  `root=UUID=cac37d97-8a41-48ca-b85c-4350669188b4 rootfstype=ext4 rootwait rw loglevel=4 console=tty0 consoleblank=600 fsck.repair=yes`
- **开机自愈**：cmdline 里的 `fsck.repair=yes` 让 initramfs 对根分区执行 `fsck -y`
  （自动修复），而不是默认的 `fsck -a`（只修"安全"问题、其它一律报错退出）。
  万一刷写过程出现任何不一致，开机时会被自动修复并继续启动，**不会**像以前那样
  停在 `(initramfs)` 紧急 shell。若文件系统本来就是干净的，这一步是无操作。
- **小镜像 + 首启自动扩容**：镜像里的根文件系统只做到约 **2.1 GiB**
  （550502 个 4K 块），`/etc/fstab` 里带 `x-systemd.growfs`。这样刷机时 bootloader
  需要写入的**声明覆盖面积小**（fastboot 用 total\_blks 而不是文件大小估算刷写量，
  覆盖越小刷得越快）；设备首次开机时由 systemd 的 `systemd-growfs-root.service`
  依据该选项把根文件系统**自动扩到整块 `userdata`**（**实测已扩到 48G**，实际尺寸随
  userdata 分区而定）。若个别情况下首启未自动扩容，手动执行
  `sudo resize2fs /dev/sda21` 即可（瞬时完成）。
- initramfs 由 Debian `initramfs-tools` 生成（`MODULES=list`：ext4/ufs/显示等驱动已编入
  内核，故无需带模块），内含自建的 `polaris-usb-gadget` 所需组件，以及 A630 GPU 固件
  （`qcom/a630_sqe.fw` / `qcom/a630_gmu.bin` / `qcom/sdm845/Xiaomi/dipper/a630_zap.mbn`，
  由 `rootfs/etc/initramfs-tools/hooks/a630-gpu-firmware` 注入）：msm DRM 在切根前
  就加载 GPU 固件，因此这些固件必须随 initramfs 携带。

## 已知限制

- **刷 userdata 前必须先 `fastboot erase userdata`**：本机 bootloader 不写全零数据，
  不 erase 直接覆盖会让旧数据残留在「镜像要求为 0」的位图区，导致每次开机 e2fsck 全盘检查
  （见「刷机步骤」说明）。
- **稀疏镜像体积上限约 2.1 GiB**：`images/xiaomi-dipper.img` 是 Android sparse 格式
  （只有 RAW+FILL、无 don't-care 空洞），声明覆盖约 2.1 GiB；它不是 raw ext4，
  不能 loop 挂载 / e2fsck。
- **刷完可能停在 fastboot**：本机 bootloader 偶发回落进 fastboot，此时**长按电源键
  12\~15 秒**物理复位即可（数据已写完，不会丢）；引导阶段设备把数据落盘到 UFS 可能持续几分钟。
- **UFS 异常断电恢复**：根文件系统在 UFS 上，非正常断电（电量耗尽）可能导致 UFS 重新上电后
  HS-G2 链路训练失败、开机卡死。镜像**本次已烘入** `polaris-power-guard`
  （低电量 `<= 10%` 警告、`<= 7%` 且未充电连续 3 次确认后宽限 30 秒干净关机，
  期间恢复充电即取消；脚本已按 dipper 改写，阈值可用 `/etc/default/polaris-power-guard` 覆盖）。
  该守护依赖 `qcom-battery` + `pmi8998-charger` 两个 sysfs 节点（设备上已确认存在），
  但**低电量关机路径未实机走过**（没有把电池放到 7% 去验证）。若根文件系统出现不一致，
  cmdline 的 `fsck.repair=yes` 会开机自动 `fsck -y` 修复；仍失败则按「刷机步骤」重刷（先 `erase userdata`）。
- **蓝牙已实测可用**：本机（dipper）蓝牙正常（`hciconfig -a` UP RUNNING、
  `bluetooth.service` 运行中）。请**不要**照抄 polaris 老笔记里「SOC 蓝牙不可用」的旧结论。
- **USB OTG / Type-C PD 已实测可用**（本轮验证，`r79` 内核 + `#80`）：
  插 OTG 转接后 `tcpm` 自动把 `a600000.usb-role-switch` 切到 `host`（无需手工写 role），
  `xhci-hcd` 枚举出 USB 存储设备并成功挂载读写 —— 读 26.3 MB/s、写 21.4 MB/s，
  且 3.4 GB 的 `live/filesystem.squashfs` 读出 md5 与盘上官方 `md5sum.txt` 逐位一致；
  插 PD 充电器后 `port0` 转 `[sink]`、`power_operation_mode=usb_power_delivery`，
  协商出 **PD 3.0 / PPS 9 V 2 A（18 W）**（对端能力 5–20 V、最高 3 A），
  `pmi8998-charger` `online=1`、`status=Charging`，电池净充入约 636 mA。
  注意：只验证到 **USB 2.0 High Speed**（手头 U 盘是 USB2 设备），
  `usb2` 的 SuperSpeed 总线未插 USB3 外设验证；PD 也只跑到了 9 V 档。
- **本次未验证 / 待做的功能**（均**非**"可用"，请勿引用为已验证）：
  - 摄像头（见下）。
  - 音量键的**箭头注入**只验到"事件已进控制台键盘层"（`kbd` handler + `tap -> arrow 103/108`），
    没在 readline 里实际确认上/下翻历史；长按调音量是实测数值确认的。
- 无图形界面、无 GPU 桌面（本版本刻意如此，控制台不受影响）。
- 摄像头在内核与设备树层面未包含，不可用（需要时可参考
  <https://github.com/2114460639/pmos-polaris-fixes> 里 polaris 的启用方式）。
- 挂起/休眠已永久禁用（`sleep.target` 等 5 个目标 unit 全部 `masked`、logind 全部 `ignore`）；
  空闲 10 分钟由 `consoleblank=600` **完全熄灭屏幕**（背光 `bl_power=4` = `FB_BLANK_POWERDOWN`，
  面板供电轨不撤除，按键唤醒后按原亮度恢复）。
- 首次开机需生成 SSH 密钥并扩容根分区，比后续开机慢。

## 校验

```bash
cd images
md5sum -c boot.img.md5 xiaomi-dipper.img.md5
```

预期结果：`boot.img` = `ce37b635…`、`xiaomi-dipper.img` = `1cffb323…`。
