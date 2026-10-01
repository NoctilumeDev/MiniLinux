# 教学闭环：当前候选与证据范围

用户在 2026-10-02 授权长任务：按 M0–M6 形成教学闭环，用简单、可读的实现，不照搬外部内核，内核与用户程序不依赖运行库。本轮从本机 M1 候选 `36ab4e5` 继续，公开 main 的起点仍为 `5e459b0e3f22a59924e78202ea5e331071c56e27`。历史 M0 标签不移动。

## 当前坐标

当前分支 `feat/teaching-os-closure`，受测源码固定在 `d296971d2b7be381d94c7a3cad544f3b296964f3`。M2 `1d2c894`，M3 `d2bae88`，M4 `a5615a5`，M5 `f72c2bf`，M6 与完整脚本 `d296971`。本机和 GitHub 精确提交干净克隆均完成 `tools/closed-loop.ps1`：全部正常链、M3 寄存器损坏、M6 数据别名拒绝与正常接管、ELF 依赖检查通过。

候选已发布到 [PR #1](https://github.com/NoctilumeDev/MiniLinux/pull/1)，open、draft、未合入；创建时 base 精确为 `5e459b0e3f22a59924e78202ea5e331071c56e27`。此后提交只整理文档和证据，受测内核、用户程序、链接和工具脚本不变。没有宣称 GitHub CI、另一台宿主或 main 新版本已验证。

## 能证明什么

最终程序是独立编译的 freestanding C 映像，固定链接后嵌入内核。两个实例有独立页表根、代码/数据/用户栈，内核部分借用 Limine supervisor 映射。PIT 真实抢占，入口保存现场，数组轮转更换 CR3 和 TSS.RSP0，再经 `iretq` 返回 CPL3。

每个实例经 `int 0x80` 打开和读取只读 RamFS，自己核对 17 个字节并输出；指针与文件反例拒绝。一个实例完成后触发指定用户保护异常，另一个必须在故障之后继续。内核又核对相同 VA 的不同物理页、不同标记与计数，并归还全部实验页。至此是教学机制闭环；不是通用可安装 OS 或 Linux 兼容内核。

## 环境与依赖

沿用 M0 的 Windows 原生工具，QEMU 单核 TCG，64/256 MiB。没有安装 WSL、Docker 或新的运行库。M4 起使用同一份 LLVM 23.1.1 包内的 `llvm-objcopy` 提取用户映像，完整脚本使用 `llvm-nm` 检查未解析符号；两者版本与 SHA256 已增列到 `tools/verify-tools.ps1`，原工具校验值未变。

内核/用户 ELF 不链接 libc，没有未解析外部符号、INTERP 或 DYNAMIC 段。ISO 制作仍用 Python/pycdlib，启动仍用 Limine，调试仍用 GDB。这里只承诺零运行库依赖，不承诺没有构建、启动或观察依赖。

每轮日志放在 build；有意义的首败与该轮实跑保留在 docs/evidence。后来的通过不覆盖编译错误、无中断、错误守卫、故障次序缺口或无效 GDB 注入。整理的文本统一换行并去除行尾空白，不宣称与原始进程输出逐字节等同。

## 远端干净克隆

从 GitHub 克隆候选分支，再 detached checkout 到上述完整 SHA，开跑前没有 build、工作树干净。目录是 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-closed-d296971-20261002`；工具继续使用 D 盘基线，没有借用原工作区的 ELF、用户二进制或生成源码。

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

核对任意后续文档提交是否仍对应受测实现，可运行 `git diff d296971 -- kernel include user tools boot linker.ld`；本次闭环记录完成时差异为空。重新构建原工作区的 M6 并核对 ISO 载荷，避免 D 盘最后留下克隆目录的 ELF。M0–M6 的实现闭合到此停止，余下是 PR 评审与主线合入。
