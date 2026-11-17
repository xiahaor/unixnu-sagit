# 小米 6（sagit / MSM8998）上的 XNU 移植

在 **小米 6**（`sagit`，高通 MSM8998 / 骁龙 835）上，从 UEFI 一路启动**源码构建的苹果 XNU 内核**，
并跑到自写的 arm64 用户态（无 libc）。不依赖 Apple 原版 kernelcache，也不需要 iOS 系统盘密钥。

> 仓库：<https://github.com/xiahaor/unixnu-sagit>
> 许可证：GPL-3.0（沿用上游 `ilertnost/unixnu-sagit`）

## 一、启动链

```
XBL → ABL → EDK2/Mu-Silicium（UEFI 固件）→ UniXNU（引导管理器）
    → DarwinBoot（XNU loader）→ XNU 8796.141.3（源码构建）
    → RamBlock 发布 8 MiB HFS+ RAM 盘 → hfs: mounted sagitroot
    → /sbin/launchd（自写 freestanding arm64 PID1）
```

## 二、已验证能力（全部真机 + 屏幕日志为证）

| 类别 | 能力 |
|---|---|
| 启动 | EDK2 → UniXNU → DarwinBoot → XNU 内核 → HFS+ 根挂载 |
| 进程 | `posix_spawn` 服务列表、退出码解析、`wait4` 回收、崩溃监督重启（3 次上限） |
| 信号 | `kill` SIGTERM/SIGKILL，`wait` status 解析（`sig=15/9`） |
| 用户态线程 | 原版 `com.apple.kec.pthread` kext；`bsdthread_create` 的线程执行用户代码并返回 |
| 定时器/睡眠 | GICv3 非安全态引导 + 虚拟定时器（INTID 19/27），真实 `poll/tsleep` 睡眠 |
| VFS | `open/read/write/mkdir/rename/unlink/rmdir/getdirentries64`、`mount(MNT_UPDATE)` 重挂可写 |
| IPC | 父子进程经 HFS+ 文件交换数据 |
| VM | `mmap/munmap` 匿名页 |
| 图形 | 内核 `kern.sagitfb`（写矩形到 framebuffer）、`kern.sagitfbinfo`（读几何：1080×1920/32bpp） |
| 控制台 | devfs + `/dev/console` |

完整事件记录（含每一步的屏幕证据）见 [构建记录.md](docs/构建记录.md)，
技术总结见 [移植报告](docs/移植报告-20261115.md) 与 [发布说明](docs/发布说明.md)。

## 三、目录结构

```
unixnu-sagit/
├── README.md               本文件
├── RELEASE.md              发布说明（成果、镜像哈希、刷写方式）
├── docs/                   工程说明 / 构建记录 / 移植报告 / 发布说明
├── rootfs/                 自写 freestanding 用户态源码（launchd、测试服务等）
├── patches/                设备内核补丁（含 series 列表）
├── drivers/ firmware/      驱动与固件包（RamBlock、NativeUfs 等）
├── tools/                  构建脚本（内核、payload、boot 镜像、ADT）
├── profiles/ soc/ dts/     板级/SoC 配置与设备树
├── configs/                设备 defconfig
└── scripts/                刷写与维护脚本（auto-flash.sh 等）
```

## 四、快速开始（简述）

前提：

- 小米 6（sagit）已解锁 bootloader，能进入 recovery（OrangeFox）；
- **先备份 stock boot 分区**：`dd if=/dev/block/sde40 of=/sdcard/Download/boot-stock.img`；
- 主机：Linux x86_64，LLVM 交叉工具链（`toolchains/unixnu-linux`，体积大，不入库）；
- iOS 16.7.16 (20H392) kernelcache 与 device tree（放 `ipsw/`，不入库）。

构建内核（示意，实际路径按你的环境调整）：

```bash
SRC=$PWD/UniXNU TOOLCHAIN=$PWD/toolchains/unixnu-linux \
UNIXNU_KERNELCACHE=$PWD/ipsw/kernelcache.arm64 \
  bash unixnu-sagit/tools/build-xnu-sagit.sh
```

构建用户态与根镜像、打包 boot 镜像：

```bash
bash unixnu-sagit/tools/build-launchd-stub.sh out/rootfs
python3 unixnu-sagit/tools/mksagit-payload.py --kernel <kernel.macho> \
  --adt <adt> --initrd <root.img> --output out/payload.img --size-mib 56
python3 unixnu-sagit/tools/build-sagit-boot.py --base out/v6/boot-sagit.img \
  --ramdisk out/payload.img.lzma --output out/boot-sagit.img
```

刷入（设备在 OrangeFox recovery）：

```bash
adb push out/boot-sagit.img /sdcard/Download/
adb shell "dd if=/sdcard/Download/boot-sagit.img of=/dev/block/sde40"
```

完整流程与每步验证见 [docs/构建记录.md](docs/构建记录.md)。

## 五、已知边界

1. **UFS 块设备写**：ABL 留下的控制器状态无法安全接管，已回滚（只读诊断保留）；
2. **真 iOS 用户态**：需要解密的系统盘与 trust cache，不在本项目范围（用户态为自写实现）；
3. **SMP**：硬件 8 核，当前仅 1 核在线（单核已满足上述能力）；
4. GIC `ISENABLER0/IGROUPR0` 读回值异常，但定时器工作正常。

## 六、文档索引

| 文档 | 内容 |
|---|---|
| [发布说明](RELEASE.md) | 成果、镜像哈希、刷写步骤、已知问题 |
| [构建记录](docs/构建记录.md) | 逐日构建/调试记录（含屏幕证据与踩坑） |
| [移植报告](docs/移植报告-20261115.md) | 可分享的技术报告（补丁清单、关键经验） |
| [工程说明](docs/工程说明.md) | 与本 README 同步的工程总览 |

## 七、致谢

- [UniXNU](https://unixnu.com/) 与 [edk2-msm8998](https://github.com/edk2-porting/edk2-msm8998)
- 同 SoC（MSM8998）参考项目：[MC-XiaoXiao/ios_sagit](https://github.com/MC-XiaoXiao/ios_sagit)（GICv3/定时器实现参考）、[DinnerABC/UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger)
