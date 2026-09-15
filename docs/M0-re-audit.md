# M0 复审：失败后仍能重新接管吗？

本轮在 `1c0a3a8d7f9936d2d755b9c096ad8a61913f43be` 的[首次轮次记录](M0-round-record.md)之后重新审视“达标”二字。原记录的精确提交复验和正常启动结果仍有效；但它们没有验证 GDB 等不到断点时能否退出。这份复审保留这个缺口及纠正过程，不倒改首次结论。

## 问题、反证与失效范围

M0 的停止线要求同机实验链可复现、可观察、可重新接管。`debug.ps1` 原来直接执行 GDB `continue`，没有总时限；如果 Limine 停在错误画面，GDB 可以一直等待，QEMU 的 `finally` 清理也无法执行。因此此前的“可重新接管”只由正常路径支持，**故障路径尚未取得达标资格**。

另一个基线缺口是 `verify-tools.ps1` 当时校验 Limine 主机程序，却没有校验真正放入 ISO 的 `limine-bios-cd.bin`、`limine-bios.sys`；构建使用的 `llvm-readelf.exe` 也未纳入。这些文件被改动时，旧基线不能先行拦截。

本轮反证条件是：无效镜像被算作通过；GDB 或 QEMU 在失败后遗留；端口 1234 被占用却误连另一服务；从修正提交的无构建产物克隆无法重跑正常路径。任何一项成立，都不能声称 M0 本地达标。

## 修正与实际复验

代码修正坐标是 `6ffde0dd9893884c322c653ae0e507a145bfde5b`：串口和 GDB 各有默认 20 秒观察时限；GDB 超时会停止本轮调试进程，脚本退出时停止本轮 QEMU；端口 1234 已监听时启动前报错。Limine BIOS 文件与 ELF 检查器 SHA256 加入工具基线。

受控故障输入是 2048 字节全零文件 `build/invalid-m0.iso`，本地 SHA256 `e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad`。先在主工作树复验，再从 `6ffde0dd9893884c322c653ae0e507a145bfde5b` 本地克隆到 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-m0-reaudit-d16dcbb9`；克隆前无 `build` 目录，HEAD 精确一致。

克隆里的正常链通过工具哈希、ELF、ISO 有效载荷、串口与两个 GDB 断点。随后在同一精确提交克隆里把 `-IsoPath` 指向无效文件：串口观察在 3 秒内返回 `M0 serial evidence is missing`；GDB 观察在 5 秒内返回 `GDB exceeded the 5-second observation limit`。两次之后本轮 QEMU 和 GDB 进程数均为零。[故障复验记录](evidence/m0-6ffde0d-local/failure-results.txt)保存了提交与结果。另用临时 TCP listener 占住 1234，GDB 启动前返回 `GDB port 1234 is already in use`，没有启动 QEMU。最后重跑桌面主项目，让 D 盘 ISO 再次与主项目本轮 ELF 匹配。

正常克隆的[串口记录](evidence/m0-6ffde0d-local/serial.log)和[GDB 记录](evidence/m0-6ffde0d-local/gdb-trace.log)为本地诊断归档。克隆原始串口 SHA256 是 `6e71658b68e0a9f909cb7d1eee686d53c6d1701a61d79f642ee116f4f2362aad`，GDB 原始 SHA256 是 `5de7dfab39c2223c2a3f2ba74cc10b8b30276c2f1ed6630328532eccd0995c26`，克隆 ELF SHA256 是 `232476419d065ec77c2de78bb2f5f9655375013085f23e334b578eaf45fcc9d4`。归档文本按仓库规则使用 LF，GDB 结尾空白行已删去；原始文件哈希只指向克隆时的本地文件。

## 裁决范围与接管入口

**M0 在这台 Windows 机器的 BIOS/QEMU 路径上达到本地验收线。**正常路径可以从精确提交的干净克隆重跑；无效镜像和端口冲突能有界拒绝，观察进程不遗留。这个结论不覆盖另一台新机器的工具安装、UEFI、真实硬件、公开 CI 或 GitHub 远端主线。远端 PR、CI run、Release 和公开读回坐标仍为 **NOT YET AVAILABLE**，因此不把本地验收称为公开冻结。

这一段是建立 GitHub 仓库以前的复审结论；后续远端克隆及公开坐标见 [M0 远端轮次记录](M0-remote-round-record.md)。

接手时先核对当前 Git HEAD 和工作树，再运行 `tools/m0.ps1`；如果工具哈希、镜像载荷、串口或 GDB 任一门失败，先停在 M0 重判。这个实验台可以支撑后续机制讨论，不能把 M0 的结果升级成用户态、调度或文件系统的实现证据。
