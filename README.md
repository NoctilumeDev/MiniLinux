# MiniLinux

MiniLinux 是一个以 C 为主实现的教学操作系统。它用真实的用户态执行链解释：一个程序如何获得内存和 CPU 时间，以及如何通过内核入口读取文件中的字节。名字中的 Linux 表示学习对象和风格；本项目没有使用 Linux 内核源码，也不是 Linux 发行版或兼容实现。

当前只实现 **M0 实验台**：Windows 原生构建 x86_64 ELF，Limine 从光盘镜像启动，QEMU 进入 `kernel_main`，串口留下输出和预期的 panic，并由 GDB 命中两个断点。M0 不声称已经实现内存管理、中断、调度、系统调用或文件系统。

项目源码位于 `C:\Users\lenovo\Desktop\GitHubProjects\MiniLinux`，工具安装在 `D:\DevTools\MiniLinux`。在 PowerShell 中运行：

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

依赖的版本、安装路径、镜像制作、实际观察结果和已发现的坑记录在 [M0 实验记录](docs/M0.md)。内核入口和串口实现见 `kernel/main.c`。Limine 协议头文件是来自其独立协议仓库的固定版本，原有 0BSD 许可保留在 `include/limine.h`。

代码原则：能直写就直写；教学问题之外尽量复用工具，教学问题之内亲手实现机制。这里唯一的内联汇编用于 x86 的端口 I/O 和停机指令。
