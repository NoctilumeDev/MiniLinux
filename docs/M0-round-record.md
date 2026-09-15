# M0 轮次记录：本地实验台验收

这份记录保存决策、反例和本地诊断坐标，不把文档当作事实裁判。写法参考 [单兵工程法](https://github.com/NoctilumeDev/NoctilumeDev/blob/935c75b050e8cbbb61ec55f5558683192113d66f/docs/solo-engineering-method.md)与[每轮决策与事实记录](https://github.com/NoctilumeDev/NoctilumeDev/blob/935c75b050e8cbbb61ec55f5558683192113d66f/docs/iteration-decision-fact-record.md)：保留真实闭环、首次红灯、复验和证据身份。

## 本轮问题与停止线

在这台 Windows 机器上建立 Windows PowerShell → freestanding x86_64 ELF → Limine → QEMU → C 入口 → 串口/panic → GDB 的最小链。验收要求它能从一个精确 Git 提交的无构建产物工作树重跑，并能证明运行镜像内的内核确实属于本轮构建。M0 不证明内存管理、任务切换、用户态、系统调用或文件系统。

## 起点与冻结输入

起点是一个新建的本地 Git 仓库，没有前序提交；源码放桌面，工具和 ISO 放 D 盘。技术候选选定 Windows 原生 Clang/LLD、Limine BIOS CD、QEMU TCG。下载包长度和哈希在 [M0 实验记录](M0.md)，本机执行文件哈希及 pycdlib 版本在 `tools/verify-tools.ps1` 中。Limine 协议头固定到 `limine-protocol` 的 `da65184e91f80fcb397270121b1e2515a11e01ee`。

这个选择的理由是把 bootloader 和硬件模拟复用掉，只亲手写 C 入口、COM1 输出和停机路径。CPU 端口 I/O 与 `hlt` 用必要的少量内联汇编。

## 原方案、关键前提与反证条件

原方案用 pycdlib 在 Windows 直接生成带 El Torito BIOS 启动项的 ISO，Clang/LLD 输出 ELF，QEMU 串口和 GDB 分别观察执行。只看到串口文本并不足以确认运行了最新 ELF；镜像中的 `kernel.elf` 必须与该工作树本轮产物哈希相同。

以下任一情况都应停止宣称 M0 通过：ISO 无法启动到 C 入口；COM1 没有记录；GDB 命不中两个断点；镜像有效载荷与本轮构建不一致；从精确提交的干净工作树重跑失败；工具二进制不符合本轮基线。

## 实际轨迹与失效传播

1. 命令行 GitHub HTTPS 直连曾 DNS 超时。对下载和 Git 命令显式指定 `127.0.0.1:7897` 后，HTTPS 与远端 HEAD 可读；没有修改 Git 全局代理配置。这个网络前提只影响工具获取，M0 本地构建和启动无需联网。
2. 第一份 ISO 只有 450560 字节。SeaBIOS 已开始从 CD 启动，Limine 随后显示 `iso9660: failed to read file data`，串口为空。[首次失败画面](evidence/m0-b2046ca-local/first-iso-failure.png)是当时的本地截图，不能代表后来的通过状态。查阅 Limine 读取路径后，在 ISO 末端追加 1 MiB 空白扇区；未做 `bios-install` 的 BIOS CD 随即进入 `kernel_main`。读取余量不足是根据源码与实验作出的推断，修复效果由启动结果证实。
3. Windows 路径的反斜杠曾被 GDB 命令解析掉，导致 `file` 未加载 ELF 符号；改用正斜杠后，`kernel_main` 与 `panic` 断点、RIP、CS、调用栈可读。调试脚本现在把输出保存到 `build/gdb-trace.log`，并要求两个断点实际出现。
4. 从 Git 暂存内容导出一份无 `.git`、无 `build` 的干净副本，整条链首次通过。随后故意保留这份副本生成的 D 盘 ISO，在原工作树执行镜像校验；虽然两份内核能打印同一段串口文本，校验却正确拒绝 `ISO payload differs from this build: kernel.elf`。这推翻了“串口通过即证明本轮 ELF”的前提。加入由 Windows `tar` 独立提取、比较内核和 Limine 文件 SHA256 的门后，重新生成镜像通过。
5. 源码提交为 `b2046caebfce58c0776ce9d9ac830fa1485d1fbe`。从该精确提交本地克隆到 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-m0-checkout-f3843abb`；克隆时没有 `build` 目录，HEAD 与源提交相同。`tools/m0.ps1` 在克隆中依次通过工具哈希、ELF、镜像有效载荷、串口和 GDB；验收后没有遗留 QEMU 进程。最后在桌面主项目重跑并核对镜像，使 D 盘 ISO 再次匹配主项目的 ELF。
6. 写方法论引用时，曾把 `NoctilumeDev` 的本地 HEAD `65d6da9c1cb0733c6daa7adee6c66de1c5207ade` 当成 GitHub 公开提交构造链接，读回得到 HTTP 404。随后从远端 `main` 读到 `935c75b050e8cbbb61ec55f5558683192113d66f`，该 SHA 上两篇文章均返回 HTTP 200；上面的引用已改用这个可公开读回的精确坐标。本地 SHA 不能替代远端发布坐标。

## 变更与证据坐标

本轮源码坐标是本地 `main` 的 `b2046caebfce58c0776ce9d9ac830fa1485d1fbe`。干净克隆的本地诊断输出已抄存为[串口记录](evidence/m0-b2046ca-local/serial.log)和[GDB 记录](evidence/m0-b2046ca-local/gdb-trace.log)。原始文件在克隆目录的 SHA256 分别为 `6e71658b68e0a9f909cb7d1eee686d53c6d1701a61d79f642ee116f4f2362aad` 和 `b7858cf25bf3482f81ac504aeb43077e476ca3485473903b7f33876ed4238304`；克隆 ELF 是 `8c7cfc0106e418ffc484d3e8eb1908d452476706e3220c59f6e146082161d705`。当次 ISO 的本地 SHA256 是 `d545edda54a53f8519b6e1846bda4af8b7908e0b464b75decf25db7f34b13524`。归档文本经 Git 统一为 LF，GDB 记录还删掉了结尾空白行；因此原始文件 SHA256 不能当作归档副本哈希。归档 Git blob 分别是 `413f33227351f5b991f1ab380e7dfbafee160ed1`（串口）与 `4593772e89b731eccb8a9177b5fb0693eede0b28`（GDB）。这些坐标标识本地诊断与归档内容，不能冒充公开 CI 或远端发布证据。

当前没有 GitHub 远端、PR、CI run、远端 exact-main 或 Release 坐标；它们的状态是 **NOT YET AVAILABLE**。源码提交和本地克隆证明同机可复现，证据上限仍是本地 BIOS/QEMU 路径。

## 落点与下一轮边界

M0 是**本地已验证的实验台**，尚未公开发布或按远端证据冻结。以后接手者先核对 Git HEAD、工作树、`tools/verify-tools.ps1` 的二进制基线，再运行 `tools/m0.ps1`；如果任何反证出现，先修地基并重判结论。继续讨论项目机制时，可以使用这个实验台；不能把它的成功写成已经实现进程、用户态或文件系统。
