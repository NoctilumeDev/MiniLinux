# MiniLinux

MiniLinux 是一个以 C 为主实现的教学操作系统。它用真实的用户态执行链解释：一个程序如何获得内存和 CPU 时间，以及如何通过内核入口读取文件中的字节。名字中的 Linux 表示学习对象和风格；本项目没有使用 Linux 内核源码，也不是 Linux 发行版或兼容实现。

当前只实现 **M0 实验台**：Windows 原生构建 x86_64 ELF，Limine 从光盘镜像启动，QEMU 进入 `kernel_main`，串口留下输出和预期的 panic，并由 GDB 命中两个断点。M0 不声称已经实现内存管理、中断、调度、系统调用或文件系统。

M0 的证明顺序是：固定源提交和工具基线 → 构建 ELF → 制作 ISO 并独立核对其中的内核载荷 → QEMU、串口与 GDB 观察 → 用坏镜像和端口冲突验证失败可接管 → 从 GitHub 精确提交克隆并重跑。任一门失败，就停在 M0 重判。

## 路线图

每个里程碑只回答一个机制问题，状态由实际运行和可回查的证据决定。编号表示依赖顺序，不承诺日期；下一项能讨论，不等于已经获准跳过本项的反证或开始写后续模块。

| 里程碑 | 要回答的问题 | 过门事实 | 状态 |
| --- | --- | --- | --- |
| M0 实验台 | 这台机器能否从精确提交进入 C 内核，并在失败后重新接管？ | 远端干净克隆通过 ELF、ISO 载荷、串口与 GDB；坏镜像有界拒绝。受测源码固定在 [`m0-windows-bios-qemu`](https://github.com/NoctilumeDev/MiniLinux/tree/m0-windows-bios-qemu)，事实见 [远端轮次记录](docs/M0-remote-round-record.md)。 | ✅ 本机 BIOS/QEMU 已验证 |
| M1 物理页 | 内核凭什么认定一页物理内存可用？ | 根据内存图取得不同的可用页；分配、释放与保留区域排除都能实际核对。 | 未开始 |
| M2 地址映射 | 一个虚拟地址怎样落到指定的物理页？ | 自己建立并检查页表映射；通过虚拟地址写入，再从对应物理页读回同一字节。 | 未开始 |
| M3 CPU 时间 | 两个 task 为什么看起来都在运行？ | 不靠主动让出 CPU，时钟中断实际打断当前 task、保存上下文并切换；两者的执行轨迹可观察。此时仍称 task。 | 未开始 |
| M4 用户边界 | 用户代码怎样获得执行上下文，又如何受控进入内核？ | 一个程序由调度器进入真正的用户态；GDB 可见特权级，syscall 进入内核并返回，直接访问内核专属页被拒绝。 | 未开始 |
| M5 文件字节 | 用户程序怎样读取 RamFS 中的字节？ | 同一用户程序经 syscall 请求读取，内核取出字节并返回用户态，程序把结果输出；主教学问题形成真实闭环。 | 未开始 |
| M6 进程声明 | 两个用户程序能否各自获得内存与 CPU 时间而不互相改写？ | 时钟驱动两者切换；两套地址空间在相同虚拟地址保存不同数据且互不改写。到此才把它们称为 process。 | 未开始 |

M5 关闭“一个用户程序获得内存和 CPU 时间、经内核入口读取文件字节”的主链；M6 验证并发与进程隔离的额外声明。Shell 可以作为展示入口，但不承担验收。到这条机制链成立就停，不把网络、真磁盘恢复、SMP、GUI、完整 POSIX 或生产 hardening 纳入路线图。每轮开工前再固定该轮输入与反证条件，通过后记录精确提交和观察结果；未开始的行只是证明目标，不是假装已经冻结实现方案。

公开源码位于 [NoctilumeDev/MiniLinux](https://github.com/NoctilumeDev/MiniLinux)。本机已验证的工作树位于 `C:\Users\lenovo\Desktop\GitHubProjects\MiniLinux`，工具安装在 `D:\DevTools\MiniLinux`。在这台机器的 PowerShell 中运行：

```powershell
cd C:\Users\lenovo\Desktop\GitHubProjects\MiniLinux
.\tools\m0.ps1
```

成功时会看到：

```text
MiniLinux M0: entered kernel_main
MiniLinux PANIC: M0 reached its intentional stop
Breakpoint 1, kernel_main (...)
Breakpoint 2, panic (...)
```

依赖的版本、安装路径、镜像制作、实际观察结果和已发现的坑记录在 [M0 实验记录](docs/M0.md)。首次失败、复验、精确提交和本地证据身份记录在 [M0 轮次记录](docs/M0-round-record.md)；故障路径的纠正与裁决范围记录在 [M0 复审](docs/M0-re-audit.md)；从 GitHub 精确提交克隆并重跑的事实见 [M0 远端轮次记录](docs/M0-remote-round-record.md)。内核入口和串口实现见 `kernel/main.c`。Limine 协议头文件是来自其独立协议仓库的固定版本，原有 0BSD 许可保留在 `include/limine.h`。

代码原则：能直写就直写；教学问题之外尽量复用工具，教学问题之内亲手实现机制。这里唯一的内联汇编用于 x86 的端口 I/O 和停机指令。
