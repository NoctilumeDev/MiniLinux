# M0 远端轮次：GitHub 上的精确提交能接管实验吗？

这轮只补 M0 的仓库身份与远端读回，不增加内核机制。前两轮的本地正常路径、首次失败和脚本故障复审仍保留在 [首次轮次记录](M0-round-record.md)与[复审](M0-re-audit.md)，不把后来的结果倒写进当时结论。

## 问题、边界与反证

问题是：从公开 GitHub `main` 上一个精确提交，克隆出没有构建产物的工作树后，能否在这台 Windows 机器上重跑工具核对、ELF 构建、ISO 载荷核对、Limine/QEMU 启动、串口和 GDB，并在受控故障后重新接管？若远端提交与本地不一致、克隆带有构建产物、任一正常门失败、坏镜像被算作通过、故障后不能重跑，就不宣称 M0 远端闭环。

本轮仍只验证 **Windows Native + 单核 BIOS/QEMU**。它不证明另一台新机器的工具安装、UEFI、真实硬件、用户态或内存管理。公开源码和诊断归档也不是公开 CI run。

## 起点和固定输入

2026-09-16，使用已经认证为 `NoctilumeDev` 的 Git 凭据，在此前不存在的地址建立公开空仓库 [NoctilumeDev/MiniLinux](https://github.com/NoctilumeDev/MiniLinux)，API 返回 `201 Created`。仓库没有由 GitHub 自动生成的初始提交；本地 `main` 推送后，`refs/heads/main` 的读回 SHA 是 [`45b160f4ff92e5577c6bc55abaff0b8c3d408f62`](https://github.com/NoctilumeDev/MiniLinux/commit/45b160f4ff92e5577c6bc55abaff0b8c3d408f62)。仓库、该提交和该 SHA 下的 `kernel/main.c` 页面都返回 HTTP 200。

该提交相对于已做故障复审的 `6ffde0dd9893884c322c653ae0e507a145bfde5b`，在 `kernel/`、`tools/`、`include/`、`boot/` 和 `linker.ld` 上没有差异；新增内容是本地复审文档及诊断归档。工具路径与二进制哈希仍由 `tools/verify-tools.ps1` 固定，D 盘 ISO 是本轮输出，不是 Git 提交内容。

稳定标签 [`m0-windows-bios-qemu`](https://github.com/NoctilumeDev/MiniLinux/tree/m0-windows-bios-qemu) 已推送并从远端读回，解引用后精确指向上述受测提交 `45b160f4ff92e5577c6bc55abaff0b8c3d408f62`。标签名只标记这条 Windows BIOS/QEMU 的 M0 源码基线，不宣称其它启动路径。

## 实际轨迹和红灯

从 GitHub 克隆 `main` 到 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-m0-remote-45b160f-20260916`。克隆 HEAD 精确等于上面的 SHA，工作树干净，24 个跟踪文件，克隆前没有 `build/`。在克隆中运行 `tools/m0.ps1`，工具哈希及 pycdlib 版本、ELF64 x86-64、ISO 内核/配置/BIOS 文件载荷、串口入口和预期 panic、GDB 的 `kernel_main` 与 `panic` 两个断点全部通过。

随后把 2048 字节全零文件作为坏 ISO，SHA256 为 `e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad`。串口观察在 3 秒时限内以 `M0 serial evidence is missing` 拒绝；GDB 观察在 5 秒时限内以 `GDB exceeded the 5-second observation limit` 拒绝。占住本机端口 1234 时，调试脚本在启动 QEMU 前以 `GDB port 1234 is already in use` 拒绝，释放监听后 QEMU/GDB 数均为零。

**不能省略的一次红灯：**GDB 超时脚本返回后立刻查进程，曾看到 1 个仍处于退出过程中的 QEMU，GDB 为零。因此本轮不能声称“脚本返回的瞬间进程数必为零”。下一次独立检查 QEMU/GDB 和端口均为空；紧接超时失败立刻重新运行完整 `tools/m0.ps1` 也成功。重跑正常链之后立即查询同样偶尔看到 1 个 QEMU，稍后查询为零。这证明没有持续遗留且实验能重新接管，但进程清退的瞬间状态应留在记录里，不能被改写成始终为零。

同一远端克隆的 ISO 在多次生成后曾得到不同 SHA256（例如 `210962c64a89919eecefd5e122c3c14a99ba67556d60b217d620e38e91d1074b` 和 `71571452159b0d2c01e0026fedc5c38189fc1aca7845c2fd07bb3e331ae33060`）。本轮证明的是**可重跑的实验链和镜像有效载荷身份**，没有证明 ISO 整体字节逐次相同。将两者混同会夸大证据。

最后在桌面主工作树重新运行完整 M0，使 `D:\DevTools\MiniLinux\images\minilinux-m0.iso` 再次装入桌面主项目本轮 ELF；工具、载荷、串口、两个断点均通过，稍后 QEMU/GDB 进程数为零。当次桌面 ELF SHA256 是 `d043589e98c4b2cee2d9e1541fbd046d8981ba303d74e3b1275778cd43d329db`，ISO SHA256 是 `93f1067f1c4a58ddfa93f2013836d1561b26413fb80ddccece481f7d80164742`。之后再生成镜像时必须重新核对，不能把这一次的 ISO 哈希当作永久产物哈希。

## 证据坐标与裁决

远端克隆正常链的[串口归档](evidence/m0-45b160f-remote/serial.log)和[GDB 归档](evidence/m0-45b160f-remote/gdb-trace.log)保留输出。克隆原始串口 SHA256 为 `6e71658b68e0a9f909cb7d1eee686d53c6d1701a61d79f642ee116f4f2362aad`，GDB 原始 SHA256 为 `7fe3ce0a8a6679e25c1609bc747a97516012170ea27a8b28133fbc43fd40caa4`，ELF SHA256 为 `6ab4f3cd16d074de44d4fa7f281cca50bd408720d584db3e81343eda966757e9`。归档统一为 LF，结尾空白行已去掉；原始文件哈希只指向该克隆当时的本地文件。

**M0 达到这台机器上的远端源码闭环：公开精确提交可读回，干净克隆可重跑正常链，故障可有界拒绝并重新接管。**范围仍限 Windows Native + BIOS/QEMU；没有 CI run、PR 或 Release，也没有跨机/UEFI 证据。今后如工具哈希、载荷身份或观察门失败，先回到 M0 重判，不借这个结论启动新的机制声明。
