# Linux 宿主：从源码进入同一个 QEMU 客体

这里新增的是宿主入口，内核、用户程序、Limine 协议和网页均沿用现有实现。Windows 体验 ZIP 仍只面向 Windows；Linux 请克隆源码。客体仍是 x86_64、BIOS CD、单核 TCG，无需 `/dev/kvm`、桌面或嵌套虚拟化。

**Linux 可以运行真实 MiniLinux，但目前需要从源码准备环境，不是下载 Windows ZIP 后直接打开。** 提交 `4eac29a6dc0ebf3802becb1fc291f085d2301290` 已在外部 Linux 沙箱的一份 QEMU 9.2.2 下通过官方 `doctor → run --stage M0 → run → check`，并完成 LAB 的 HTTP/API 操作。没有进行 Linux 浏览器视觉操作；这不是所有 Linux 发行版或所有 QEMU 版本的保证。

当前新增的 QEMU 版本检查在本机做宿主组件验证，不能冒充这份历史提交上的 Linux 客体实跑。内核、用户程序、构建配方和原客体测试判据均未因此改动。

2026-10-07 本机回归：新 Python 配方在 Windows 上实际启动 M0–M6（64 MiB）；LAB 的用户程序二进制及内核装载字节与原 PowerShell 配方逐字节一致。原用户态测试、桥接测试及两组补充边界测试通过，用户态/边界覆盖 64/256 MiB。没有改动它们的判据，也没有改动 guest 源码。一次临时补丁设置曾返回成功却没有改动文件，已经修正并实际运行了探针；首败与结果保留在 [简短回归记录](evidence/linux-host-windows-check.log)。这些结果只支持本机配方及 Windows 共享入口的回归。

## 准备一次

需要 Git、Python 3.10+、Clang/LLD、LLVM 工具、QEMU x86 系统模拟器及 SeaBIOS。入口对 QEMU 设定 **≥ 9.0.0 的保守版本前提**，优先参考本次实跑的 9.2.2；版本检查通过只表示可以继续尝试，实际结果以本机 `run`、`check` 为准。

**不要把 apt 安装成功当作环境合适。** 本次 Ubuntu 22.04 软件源装出的是 QEMU 6.2.0：它能进入 shell，却在双 counter 命令超时。下面只是 Debian/Ubuntu 的包名示例；若装出旧版本，必须先按发行版或 [QEMU 官方说明](https://www.qemu.org/download/#linux)准备较新的 QEMU 和对应固件，再继续 `doctor`。本项目不自动升级系统、不替用户编译 QEMU，也不把别人的自编译文件当成官方 Linux 体验包。

```bash
sudo apt-get update
sudo apt-get install git python3 python3-venv clang lld llvm qemu-system-x86 seabios
git clone --branch feat/linux-reproduction-20261007 https://github.com/NoctilumeDev/MiniLinux.git
cd MiniLinux
mkdir -p .tools/linux
python3 -m venv .tools/linux/venv
.tools/linux/venv/bin/python -m pip install pycdlib==1.20.0
command -v qemu-system-x86_64
qemu-system-x86_64 --version
.tools/linux/venv/bin/python -B tools/linux.py prepare
.tools/linux/venv/bin/python -B tools/linux.py doctor
```

`prepare` 只下载官方 Limine 12.9.0 ZIP，检查与 M0 相同的 SHA256，提取两份 BIOS 文件和许可证。它不安装系统软件，不运行 ZIP 内程序。源码内已有固定的 `include/limine.h`。镜像继续复用 `make-image.py` 和它的尾部余量处理，不额外引入 xorriso、NASM 或 Limine 宿主安装器。

下载或包安装遇到 403、DNS 或缺权限，要记录为环境阻断；请保留第一次错误，不跳过依赖检查。使用当前环境的代理设置，脚本不会写全局代理。已有相同档案时可以离线准备：

```bash
.tools/linux/venv/bin/python -B tools/linux.py prepare --limine-zip /path/to/limine-binary-v12.9.0.zip
```

LLVM 的工具不在 PATH 时，各命令都可以加 `--llvm-bin /path/to/llvm/bin`。QEMU 取 PATH 中第一个 `qemu-system-x86_64`；`doctor` 输出实际路径和版本。已有另一份 QEMU 时，可以只在当前终端选择它，不改系统安装或全局配置：

```bash
# 替换成已安装的 QEMU 所在目录；不是照抄这个占位路径。
export PATH="/path/to/qemu-9.2.2/bin:$PATH"
.tools/linux/venv/bin/python -B tools/linux.py doctor
```

后续命令在同一终端运行；新终端需重新选择。不要删除系统 QEMU 或用包装脚本伪造版本。手工解压 LLVM 的用户还需让动态库能被加载；缺库不是内核失败，应先修复并记录环境，再重新运行 `doctor`。

镜像与 Limine 默认放在 `.tools/linux`，需要换位置时加 `--tool-root /absolute/path`。不要与正在运行的 LAB 共用构建目录；先 Ctrl+C 停止它，再构建或检查。

`doctor`、`run`、`lab` 和 `check` 都会检查 QEMU，低于 9.0、版本无法识别、版本查询失败或查询超过 5 秒时立即拒绝，尚不构建或启动客体。`prepare`、仅 `build` 不要求 QEMU。该检查依据上游 PIC/APIC 修复选择保守范围，不探测发行版回移补丁；旧版本即使带回移补丁也会被拒绝，不等于已经证明它无法运行。

## 先跑通，再检查

先观察最小启动链，再运行用户态闭环：

```bash
.tools/linux/venv/bin/python -B tools/linux.py run --stage M0
.tools/linux/venv/bin/python -B tools/linux.py run
```

默认 `run` 重新构建 LAB、制作并核对 ISO，然后启动真正的 QEMU，顺序执行 help、ls、cat、ps、hello、reader、两个 counter、fault、check、mem，结束后回收自己的客体。`--stage M1` 到 `M6` 则检查对应串口见证；`--memory 256` 可切换客体内存。

打开实时观察页面：

```bash
.tools/linux/venv/bin/python -B tools/linux.py lab
```

手动打开打印的 <http://127.0.0.1:8080/>；服务器只监听回环地址，脚本不会弹出浏览器。Ctrl+C 关闭桥接及其 QEMU。云端没有本机浏览器时，先用 `run` 或 `check` 完成客体复验；不能把云端的回环地址当成本机地址。

完整的这条 Linux 检查入口：

```bash
mkdir -p build
set -o pipefail
.tools/linux/venv/bin/python -B tools/linux.py check 2>&1 | tee build/linux-check.log
```

这段使用 Bash 的 `pipefail`，避免 `tee` 成功掩盖检查失败。若继续执行 echo 等命令，要先保存退出码；终端单元最后一次 echo 的 0 不是被测入口的退出码。`check` 串行运行：

- M0–M6 在 64/256 MiB 的串口检查。
- M6 五组机制反例与其余六种异常探针，均在 64/256 MiB；核对原脚本的见证和三个 OOM 深度。
- 原来的 `test-userland.py`、`test-lab-bridge.py`，保持原判据。
- 原来的 `test-lab-boundaries.py`：故障幸存者、结构跨页、wait 和 INPUT 不消耗队列；历史补丁只应用到自动回收的临时源码副本。

构建同时拒绝未解析符号及 INTERP/DYNAMIC 段，镜像核对本轮 ELF、启动配置和 Limine BIOS 文件。每个串口案例有独立日志，QEMU 观察有超时，正常结束和异常路径都回收该次客体。LAB 正常镜像与边界探针镜像分开放置。

这条入口**不包含 Windows 原闭环的 GDB 现场篡改、坏 ISO/端口冲突测试，也不包含仓库外 monitor 的组件测试**，因此不能直接称为完整复制了 `closed-loop.ps1` 的资格。

## 返回什么结果

复验请给出 `git rev-parse HEAD`、`git status --short`、`doctor` 输出、整条命令的退出码及第一处失败日志。`build/linux-check.log` 包含每轮 ELF 哈希和构建参数；串口在 `build/linux-checks`，用户态/桥接结果在 `build/*-results.json`，补充探针在 `build/supplemental`。失败时先保留这些文件，再判断是准备环境、宿主入口、检查工具还是 guest 机制出了问题。

`build/lab-console.log`、`lab-trace.log`、`lab-qemu.log` 会在下一次 LAB 启动时覆盖。若要保存某次 `run` 的事实，必须在 `check` 或 `lab` 启动下一客体前复制它们；另一次客体的 trace 不能替代原记录。

exit 0 只证明这条命令所执行的范围；缺依赖、下载受阻、未执行和实际反例要分别记录。外部监测器失败时，保留它的错误与客体日志，不能把客体测试结果或整个流程单方面升级成 PASS。

本地工具和 venv 放在被忽略的 `.tools`，构建与日志放在 `build`。复验结束先保留真正承担证明责任的首败和结果，再按 [遗留物收口门禁](HYGIENE.md) 回收可重建输出及临时工具；不把整个 venv 或测试工作区提交到仓库。

## 已发生的 Linux 复验

以下都是 `4eac29a` 的历史结果，不升级为后续任意提交、宿主或 QEMU 文件的通过声明。来源为操作者交付的原始日志和更正后证据包，本项目核对了其中的日志、结果 JSON 和 ELF 字节，没有在 Windows 上假装重新执行 Linux 测试。

| 环境 / 场景 | 已观察到什么 |
| --- | --- |
| Ubuntu 22.04、LLVM 14、QEMU 6.2.0 | doctor 放行、M0 通过；LAB 前六条命令返回，双 counter 在 8 秒内未返回，入口退出 1。原 trace 的 tick 全为 0、无 schedule 事件。 |
| Linux 沙箱、LLVM 14、一份 QEMU 9.2.2 | M0、LAB 全部十条命令及 check 退出 0。36 个串口案例、64/256 MiB 用户态、16 条桥接 HTTP 检查及 baseline/fixture 边界探针通过。另一次实时 LAB 的 API 操作通过。 |

首败保留为 [QEMU 6.2 doctor](evidence/linux-reproduction/qemu-6.2-doctor.log)、[LAB 超时输出](evidence/linux-reproduction/qemu-6.2-run-lab.log)和[原始 trace](evidence/linux-reproduction/qemu-6.2-trace.log)；后轮的原始日志、结果与更正见 [QEMU 9.2.2 证据包](evidence/linux-reproduction/qemu-9.2.2-evidence.tar.gz)。证据包是外部原件，只作为数据保留，不运行包内文件。包内更正说明中的一处 LAB 哈希误写及 MANIFEST 自哈希行无效，不作为身份依据；八份 ELF 的实际字节可直接比较。

这两轮不是严格单变量实验：LLVM 的 Host CPU 分别记录为 `sapphirerapids` 和 `znver3`，变化原因不知道。早期 M0/LAB 的完整 ELF 哈希两轮相同；后期 LLVM 改为本地解包路径后，ELF 调试段路径不同，但核对八份比较文件，M0/LAB 各自的装载段及去调试后的对应文件相同。不能因此声称整个 ISO、宿主环境均逐字节相同。

9.2.2 轮保留了三次非零尝试：两次缺 clang，以及一次手工解包 LLVM 漏设动态库路径；后来的通过没有覆盖这些失败。该 QEMU 自编译时的原始档案和构建日志已丢失，来源链无法补核验。它证明那份二进制的已执行范围，不证明所有同版本二进制。

上游 [PIC/APIC 修复](https://github.com/qemu/qemu/commit/c2e6d7d8e7fc270a90c61944ef36574b1549ddcf)与这次时钟现象吻合，[9.0.0 的中断路由](https://github.com/qemu/qemu/blob/v9.0.0/hw/i386/x86.c)已包含修复；这是当前版本前提的依据，仍不把补丁写成两轮差异的唯一根因。8.2、9.0、9.1 没有在这轮实跑，Linux 浏览器视觉体验也未测。

旧 trace 中 spawn 时的 ready、marker=0 只反映建图时刻；wait 可以直接切换任务且不产生 schedule 事件，SYS_TICKS 也不记入 syscall 日志。因此不能从这些行断言 counter 始终没运行。挂住时 RIP、寄存器和数据页不知道。退出清理的报告与 SIGTERM 记录也不等于完整的进程/端口独立台账。

当前版本前置检查的轻量回归可运行 `python3 -B tools/test-linux-host.py`，不下载工具、不编译或启动客体；它覆盖旧版本拒绝、未知/失败/超时查询，以及跳过 doctor 时仍在运行入口提前拒绝。它不代替上述客体测试。

## 参考与取舍

[Limine 官方 C 模板](https://github.com/Limine-Bootloader/limine-c-template-x86-64)明确区分工具链、镜像和运行目标；这里借鉴它的分层，保留本项目原构建配方。[exec-sandbox 的宿主适配讨论](https://github.com/dualeai/exec-sandbox/issues/10)列出了路径、进程参数和回收语义的摩擦，但它是其他项目的设计讨论，不作为本项目已通过的证据。[Python 文档](https://docs.python.org/3/library/subprocess.html)规定非零 creationflags 只适用于 Windows；共享 LAB 已按宿主选择。QEMU 与系统包来源见 [官方 Linux 安装说明](https://www.qemu.org/download/#linux)及 [Ubuntu x86 系统包](https://packages.ubuntu.com/noble/qemu-system-x86)。没有复制外部内核或引入它们的隔离框架。
