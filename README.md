# MiniLinux

一个以 C 为主实现的教学操作系统，回答：**一个用户程序怎样获得内存和 CPU 时间，又怎样通过内核入口读到文件里的字节？**

Linux-inspired, independently implemented. 本项目没有使用 Linux 内核源码，不是 Linux 发行版、fork 或兼容实现。核心机制自己写，教学之外复用工具；代码以普通循环、数组和 switch 为主，少量汇编处理 CPU 入口。

**[在线实录交互回放](https://noctilumedev.github.io/MiniLinux/)** · **[下载 Windows 实时 LAB ZIP](https://github.com/NoctilumeDev/MiniLinux/releases/download/lab-preview-20261002.2/MiniLinux-LAB-Windows-x64.zip)** · **[顺着程序读代码](docs/WALKTHROUGH.md)**

Linux 宿主可按 [源码复现入口](docs/LINUX.md) 准备工具、构建并运行同一个 QEMU 客体；这一版 Linux 全流程仍待实跑复验，Windows ZIP 的支持平台不变。

**当前主线：M0–M6、用户态 init/shell、实时 LAB 与体验工具已合入 `main`。** 可以先在线试玩、下载运行，或直接克隆主线读代码。

![Windows LAB：故障被隔离，日志与详情分别滚动](docs/evidence/download-playtest/revision-2/fault-right-scrolled-left-stable.jpg)

## 先玩一次

在线页面是明确标注 **REPLAY** 的实录交互回放，选择命令就打开对应真实运行片段。终端、进程、地址映射和事件来自同一次 QEMU 录制，网页不运行新内核。

1. 点 `cat hello.txt`，看到 RamFS 字节返回终端，点击 syscall 查看参数。
2. 点 `run counter-a counter-b`，暂停回放或查看最近 counter 快照，选择两个 PID：相同虚拟地址 `0x600000` 对应不同物理页。
3. 点 `run fault`，选择日志里的 fault，查看地址、RIP、错误码与 CPL；只有指定程序退出。
4. 点击 `resume replay` 继续，或 `reset replay` 回到启动记录。

想输入命令、实时运行客体，就下载 Windows 包（[版本说明](https://github.com/NoctilumeDev/MiniLinux/releases/tag/lab-preview-20261002.2)）。Windows 10/11 x64 下解压整个 ZIP，打开 `Start.cmd`；首次联网下载约 220 MB 的官方 Python/QEMU 档案，只解包到包内 `runtime`，后续使用缓存。无需编译器、GDB、WSL 或预装 Python/QEMU。启动窗口保持打开，Ctrl+C 关闭桥接和它自己的 QEMU。中文开始、代理与重试说明见包内 `READ-ME.txt`。不想自动打开默认浏览器时，运行 `Start.cmd --no-browser`，手动打开它打印的网址。

counter 结束后可以查看最近的双实例快照：观察窗固定同一帧，客体与日志继续运行。日志的滚动条在左，详情的滚动条在右，中间竖线分区，各自滚动。stop 保留末次记录；断连时提示重开 LAB 并使用新网址，避免把旧记录看作当前状态。

这是固定程序、固定 RamFS 文件的教学体验；没有任意 ELF 加载器。浏览器是宿主观察工具，客体本身仍是串口系统。

## 已经证明什么

```text
Limine → kernel_main → 物理页 → 四级页表 → PIT / 中断 → 轮转
                                                           ↓
                            CPL3 init / shell → 用户程序
                                                           ↓ int 0x80
                                   syscall → RamFS → 用户核对 / 输出
                                                           ↓ exit / wait
                                   回收私有页 → shell 继续接受输入
```

| 里程碑 | 教学问题 | 实际证明 |
| --- | --- | --- |
| M0 实验台 | 怎样从精确源码进入 C，失败后重新接管？ | ELF/ISO 载荷、串口、GDB、有界失败及远端干净克隆。历史基线 [m0-windows-bios-qemu](https://github.com/NoctilumeDev/MiniLinux/tree/m0-windows-bios-qemu)。 |
| M1 物理页 | 哪些页可用？ | USABLE 分配、保留区拒绝、整页读写、耗尽与复用。 |
| M2 地址映射 | 虚拟地址怎样落到物理页？ | 实际更换 CR3、别名写入、物理读回、取消及重映射。 |
| M3 CPU 时间 | 两个 task 为什么都能运行？ | 时钟打断不 yield 的计算，保存/恢复上下文；坏寄存器反例被拒绝。 |
| M4 用户边界 | 用户怎样受控进入内核？ | 真正 CPL3、syscall 返回、指针拒绝，用户保护异常后另一 task 继续。 |
| M5 文件字节 | 文件怎样回到用户程序？ | 用户核对分段读取、独立偏移、EOF 与描述符/跨页边界。 |
| M6 进程声明 | 相同虚拟地址能否存不同数据？ | 不同页表根与物理页、独立标记、七种保护/异常探针、坏别名拒绝。 |
| LAB 薄上层 | 人怎样操作这条机制链？ | 用户态 init/shell、spawn/wait、hello/reader/counter/fault，故障接管与页回收。 |

M0–M6、机制反例、用户态与桥接已在合并后的干净主线重跑。独立公开下载试玩还覆盖全部命令、两栏独立滚动、快照三态与退出清理，发现的问题及首败见 [体验记录](docs/EXPERIENCE.md)。这是同一 Windows 宿主的验证，不代表其他机器或 GitHub CI。

下载包与在线回放保留各自原来的受测版本；主线维护不会移动历史标签或把 REPLAY 变成实时运行。

## 从源码运行

直接克隆默认的 `main`：

```powershell
git clone https://github.com/NoctilumeDev/MiniLinux.git
cd MiniLinux
# 先按下面的 M0 环境记录准备固定工具
.\tools\lab.ps1
```

在 <http://127.0.0.1:8080/> 操作真实 CPL3 shell。已有实验台上，`./tools/closed-loop.ps1` 验证 M0–M6，`./tools/counterexamples.ps1` 运行迁移来的错题，`./tools/check-lab.ps1` 验证上层、桥接及补充 LAB 边界。先停止预览，构建和客体串行运行。

内核和用户程序没有 libc 或第三方运行库；页面使用原生 HTML/CSS/JavaScript，桥接只用 Python 标准库。构建/启动工具仍依赖 Clang/LLD、Limine、QEMU/GDB、Python/pycdlib；“零运行库依赖”不等于这些工具不存在。

Linux 从源码进入同一条用户态闭环，详见 [Linux 准备、运行和检查](docs/LINUX.md)。它使用 PATH 中的 LLVM/QEMU、固定 Limine 与 pycdlib，并复用现有 LAB 测试；不是运行 Windows 体验 ZIP，也不替代历史 Windows/GDB 资格。

[M0 环境](docs/M0.md) · [M0 远端记录](docs/M0-remote-round-record.md) · [闭环坐标](docs/CLOSURE.md) · [错题本](docs/COUNTEREXAMPLES.md) · [薄用户态](docs/USERLAND.md) · [网页验证](design-qa.md)

M5 关闭主教学问题，M6 实证隔离后才升级 process 声明。网络、磁盘恢复、SMP、客体 GUI、完整 POSIX、动态链接与生产 hardening 不在目标内。学习 Linux/xv6 的机制与修复经验，没有复制外部内核实现。

## 阶段退出时收口

保留首败、关键反例、当前体验截图和可重跑的见证；回收失去职责的过程截图、构建副本与临时环境。先整理引用，再清理，验证后再回收验证产物，最后核对 [遗留物收口门禁](docs/HYGIENE.md)。

补充跨页、wait、INPUT 与故障幸存者探针已加入 LAB 检查；也可单独运行 `./tools/check-lab-boundaries.ps1`。引用和本仓库 build/cache 清理后，用 `D:\python-3.10.6\python.exe tools/check-hygiene.py` 做最后检查。门禁不生成新的哈希册或截图档案。

2026-10-05 收口通过：本地 32 个临时目录已回收，固定工具、首败与关键见证保留；详细职责和结果见上述门禁记录。
