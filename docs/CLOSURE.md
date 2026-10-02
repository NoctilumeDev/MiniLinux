# 教学闭环：主线与历史证据范围

2026-10-02，[PR #1](https://github.com/NoctilumeDev/MiniLinux/pull/1) 与 [PR #2](https://github.com/NoctilumeDev/MiniLinux/pull/2) 已依次合入 `main`。合并坐标 `1c938a267995c62ce1be60c6d132cd68a83b15ec` 在干净工作区完成 M0–M6、错题本与 LAB 三条验证入口，随后只更新文档和证据；当前运行代码与受测主线一致。[合并复验](EXPERIENCE.md#主线状态与合并复验) 给出新记录。以下保留开发期间的坐标与当时状态，不覆盖旧轮次。

用户在 2026-10-02 授权长任务：按 M0–M6 形成教学闭环，用简单、可读的实现，不照搬外部内核，内核与用户程序不依赖运行库。本轮从本机 M1 候选 `36ab4e5` 继续，公开 main 的起点仍为 `5e459b0e3f22a59924e78202ea5e331071c56e27`。历史 M0 标签不移动。

## 开发轮次的历史坐标

当时分支为 `feat/teaching-os-closure`，受测修补源码固定在 `ae93e86f56592fd7b38732a5f664b6213b037c1a`。从 GitHub 干净克隆，完整 `tools/closed-loop.ps1` 与 `tools/counterexamples.ps1` 全部通过；本轮的页表有效权限、七种用户异常、OOM 与入口现场范围见 [错题本](COUNTEREXAMPLES.md)。该轮后续只整理文档和保留记录，不改变这个受测实现。

原闭环的历史坐标是 `d296971d2b7be381d94c7a3cad544f3b296964f3`：M2 `1d2c894`，M3 `d2bae88`，M4 `a5615a5`，M5 `f72c2bf`，M6 与完整脚本 `d296971`。当时本机和 GitHub 精确提交干净克隆均完成 `tools/closed-loop.ps1`：全部正常链、M3 寄存器损坏、M6 数据别名拒绝与正常接管、ELF 依赖检查通过。这些旧记录保留，但不替代后来修补的复验。

该轮读回时 [PR #1](https://github.com/NoctilumeDev/MiniLinux/pull/1) 仍 open、draft、未合入，base 精确为 `5e459b0e3f22a59924e78202ea5e331071c56e27`；当前合入状态见文首。原闭环到 `d5462b4` 之间只有文档与证据；该轮新增反例后修补了实现，并以 ae93e86 重新执行完整回归。当时没有宣称 GitHub CI、另一台宿主或 main 新版本已验证。

## 能证明什么

最终程序是独立编译的 freestanding C 映像，固定链接后嵌入内核。两个实例有独立页表根、代码/数据/用户栈，内核部分借用 Limine supervisor 映射。PIT 真实抢占，入口保存现场，数组轮转更换 CR3 和 TSS.RSP0，再经 `iretq` 返回 CPL3。

每个实例经 `int 0x80` 打开和读取只读 RamFS，自己核对 17 个字节并输出；指针与文件反例拒绝。一个实例完成后触发指定用户保护异常，另一个必须在故障之后继续。内核又核对相同 VA 的不同物理页、不同标记与计数，并归还全部实验页。至此是教学机制闭环；不是通用可安装 OS 或 Linux 兼容内核。

## 环境与依赖

沿用 M0 的 Windows 原生工具，QEMU 单核 TCG，64/256 MiB。没有安装 WSL、Docker 或新的运行库。M4 起使用同一份 LLVM 23.1.1 包内的 `llvm-objcopy` 提取用户映像，完整脚本使用 `llvm-nm` 检查未解析符号；两者版本与 SHA256 已增列到 `tools/verify-tools.ps1`，原工具校验值未变。

内核/用户 ELF 不链接 libc，没有未解析外部符号、INTERP 或 DYNAMIC 段。ISO 制作仍用 Python/pycdlib，启动仍用 Limine，调试仍用 GDB。这里只承诺零运行库依赖，不承诺没有构建、启动或观察依赖。

每轮日志放在 build；有意义的首败与该轮实跑保留在 docs/evidence。后来的通过不覆盖编译错误、无中断、错误守卫、故障次序缺口或无效 GDB 注入。整理的文本统一换行并去除行尾空白，不宣称与原始进程输出逐字节等同。

## 原闭环的远端干净克隆

从 GitHub 克隆候选分支，再 detached checkout 到原闭环的 d296971 完整 SHA，开跑前没有 build、工作树干净。目录是 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-closed-d296971-20261002`；工具继续使用 D 盘基线，没有借用原工作区的 ELF、用户二进制或生成源码。

| 观察 | 保留记录 |
| --- | --- |
| 全部里程碑与依赖、清理结果 | [总记录](evidence/closure-d296971/remote-closed-loop.log) |
| M0 / M1 / M2 | [M0](evidence/closure-d296971/M0-round.log)、[M1](evidence/closure-d296971/M1-round.log)、[M2](evidence/closure-d296971/M2-round.log) |
| M3 / M4 / M5 | [M3](evidence/closure-d296971/M3-round.log)、[M4](evidence/closure-d296971/M4-round.log)、[M5](evidence/closure-d296971/M5-round.log) |
| M6，含四种保护异常和物理读回 | [M6](evidence/closure-d296971/M6-round.log) |
| 活的 R12 被改坏后守卫拒绝 | [寄存器反例](evidence/closure-d296971/M3-register-rejection.log) |
| 数据 PTE 修改与用户标记拒绝 | [别名反例](evidence/closure-d296971/M6-alias-rejection.log) |
| 旧载荷、坏镜像、端口冲突与重新运行 | [故障接管](evidence/closure-d296971/failure-recovery.log) |

坏 ISO 串口约 3.29 秒拒绝，GDB 的 3 秒观察限约 6.05 秒返回（含启动收尾）；端口冲突在启动客体前拒绝。失败后独立观察到零 QEMU/GDB 进程、零 1234 监听，然后正常 M6 通过。这是本轮观察，不把脚本观察限写成严格总时限，也不夸成每次清理瞬间都零残留。

原轮记录完成到 d5462b4 时，`git diff d296971 d5462b4 -- kernel include user tools boot linker.ld` 为空。本轮因真实反例修改了源码，不能继续用原坐标声明当前实现受测。

## 本轮错题补充的远端干净克隆

`ae93e86` 从 GitHub 精确克隆后，M0–M6 全部重跑，包括新增 #UD/#GP/#DE、原有 R12 损坏与数据别名拒绝。随后五组攻击夹具、七种保护/异常探针、cld 指令修改和普通映像重新接管通过；最终 ELF 无未解析符号或动态段，工作树干净，客体、调试器与调试端口没有持续残留。[回归](evidence/counterexamples-ae93e86-regression.log)、[攻击汇总](evidence/counterexamples-ae93e86-summary.log) 和 [现场](evidence/counterexamples-ae93e86-witnesses.log) 分别保留。

核对后续文档提交是否仍对应本轮受测实现，使用 `git diff ae93e86 -- kernel include user tools boot linker.ld`。结束前已重新构建桌面工作区的普通 M6 映像并核对载荷、运行，D 盘没有留下克隆目录的 ELF 与桌面符号文件混配。M0 历史标签保持原坐标，主线合入状态仍由 PR 另行说明。
