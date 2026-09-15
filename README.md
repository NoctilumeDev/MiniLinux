# MiniLinux

MiniLinux 是一个以 C 为主实现的教学操作系统。它用真实的用户态执行链解释：一个程序如何获得内存和 CPU 时间，以及如何通过内核入口读取文件中的字节。名字中的 Linux 表示学习对象和风格；本项目没有使用 Linux 内核源码，也不是 Linux 发行版或兼容实现。

当前只实现 **M0 实验台**：Windows 原生构建 x86_64 ELF，Limine 从光盘镜像启动，QEMU 进入 `kernel_main`，串口留下输出和预期的 panic，并由 GDB 命中两个断点。M0 不声称已经实现内存管理、中断、调度、系统调用或文件系统。

M0 的证明顺序是：固定源提交和工具基线 → 构建 ELF → 制作 ISO 并独立核对其中的内核载荷 → QEMU、串口与 GDB 观察 → 用坏镜像和端口冲突验证失败可接管 → 从 GitHub 精确提交克隆并重跑。任一门失败，就停在 M0 重判。

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
