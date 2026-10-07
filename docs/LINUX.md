# Linux 宿主：从源码进入同一个 QEMU 客体

这里新增的是宿主入口，内核、用户程序、Limine 协议和网页均沿用现有实现。Windows 体验 ZIP 仍只面向 Windows；Linux 请克隆源码。客体仍是 x86_64、BIOS CD、单核 TCG，无需 `/dev/kvm`、桌面或嵌套虚拟化。

**当前资格：Linux 入口已提交供复验，尚未完成这一版入口的 Linux 全流程实跑。** 本机 Windows 的构建与共享 LAB 回归结果会记录在本页；先前云端的机制测试和外部 monitor 组件测试不能代替这次复验。

2026-10-07 本机回归：新 Python 配方在 Windows 上实际启动 M0–M6（64 MiB）；LAB 的用户程序二进制及内核装载字节与原 PowerShell 配方逐字节一致。原用户态测试、桥接测试及两组补充边界测试通过，用户态/边界覆盖 64/256 MiB。没有改动它们的判据，也没有改动 guest 源码。一次临时补丁设置曾返回成功却没有改动文件，已经修正并实际运行了探针；首败与结果保留在 [简短回归记录](evidence/linux-host-windows-check.log)。这些结果只支持本机配方及 Windows 共享入口的回归。

## 准备一次

需要 Git、Python 3.10+、Clang/LLD、LLVM 工具、QEMU x86 系统模拟器及 SeaBIOS。下面以 Debian/Ubuntu 的包名示例；发行版版本会影响工具版本，由 `doctor` 输出实际环境，不冒充 Windows 的固定工具基线。

```bash
sudo apt-get update
sudo apt-get install git python3 python3-venv clang lld llvm qemu-system-x86 seabios
git clone --branch feat/linux-reproduction-20261007 https://github.com/NoctilumeDev/MiniLinux.git
cd MiniLinux
mkdir -p .tools/linux
python3 -m venv .tools/linux/venv
.tools/linux/venv/bin/python -m pip install pycdlib==1.20.0
.tools/linux/venv/bin/python -B tools/linux.py prepare
.tools/linux/venv/bin/python -B tools/linux.py doctor
```

`prepare` 只下载官方 Limine 12.9.0 ZIP，检查与 M0 相同的 SHA256，提取两份 BIOS 文件和许可证。它不安装系统软件，不运行 ZIP 内程序。源码内已有固定的 `include/limine.h`。镜像继续复用 `make-image.py` 和它的尾部余量处理，不额外引入 xorriso、NASM 或 Limine 宿主安装器。

下载或包安装遇到 403、DNS 或缺权限，要记录为环境阻断；请保留第一次错误，不跳过依赖检查。使用当前环境的代理设置，脚本不会写全局代理。已有相同档案时可以离线准备：

```bash
.tools/linux/venv/bin/python -B tools/linux.py prepare --limine-zip /path/to/limine-binary-v12.9.0.zip
```

LLVM 的工具不在 PATH 时，各命令都可以加 `--llvm-bin /path/to/llvm/bin`；QEMU 的 `qemu-system-x86_64` 须在 PATH。镜像与 Limine 默认放在 `.tools/linux`，需要换位置时加 `--tool-root /absolute/path`。不要与正在运行的 LAB 共用构建目录；先 Ctrl+C 停止它，再构建或检查。

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

这段使用 Bash 的 `pipefail`，避免 `tee` 成功掩盖检查失败。`check` 串行运行：

- M0–M6 在 64/256 MiB 的串口检查。
- M6 五组机制反例与其余六种异常探针，均在 64/256 MiB；核对原脚本的见证和三个 OOM 深度。
- 原来的 `test-userland.py`、`test-lab-bridge.py`，保持原判据。
- 原来的 `test-lab-boundaries.py`：故障幸存者、结构跨页、wait 和 INPUT 不消耗队列；历史补丁只应用到自动回收的临时源码副本。

构建同时拒绝未解析符号及 INTERP/DYNAMIC 段，镜像核对本轮 ELF、启动配置和 Limine BIOS 文件。每个串口案例有独立日志，QEMU 观察有超时，正常结束和异常路径都回收该次客体。LAB 正常镜像与边界探针镜像分开放置。

这条入口**不包含 Windows 原闭环的 GDB 现场篡改、坏 ISO/端口冲突测试，也不包含仓库外 monitor 的组件测试**，因此不能直接称为完整复制了 `closed-loop.ps1` 的资格。

## 返回什么结果

复验请给出 `git rev-parse HEAD`、`git status --short`、`doctor` 输出、整条命令的退出码及第一处失败日志。`build/linux-check.log` 包含每轮 ELF 哈希和构建参数；串口在 `build/linux-checks`，用户态/桥接结果在 `build/*-results.json`，补充探针在 `build/supplemental`。失败时先保留这些文件，再判断是准备环境、宿主入口、检查工具还是 guest 机制出了问题。

exit 0 只证明这条命令所执行的范围；缺依赖、下载受阻、未执行和实际反例要分别记录。外部监测器失败时，保留它的错误与客体日志，不能把客体测试结果或整个流程单方面升级成 PASS。

本地工具和 venv 放在被忽略的 `.tools`，构建与日志放在 `build`。复验结束先保留真正承担证明责任的首败和结果，再按 [遗留物收口门禁](HYGIENE.md) 回收可重建输出及临时工具；不把整个 venv 或测试工作区提交到仓库。

## 参考与取舍

[Limine 官方 C 模板](https://github.com/Limine-Bootloader/limine-c-template-x86-64)明确区分工具链、镜像和运行目标；这里借鉴它的分层，保留本项目原构建配方。[exec-sandbox 的宿主适配讨论](https://github.com/dualeai/exec-sandbox/issues/10)列出了路径、进程参数和回收语义的摩擦，但它是其他项目的设计讨论，不作为本项目已通过的证据。[Python 文档](https://docs.python.org/3/library/subprocess.html)规定非零 creationflags 只适用于 Windows；共享 LAB 已按宿主选择。QEMU 与系统包来源见 [官方 Linux 安装说明](https://www.qemu.org/download/#linux)及 [Ubuntu x86 系统包](https://packages.ubuntu.com/noble/qemu-system-x86)。没有复制外部内核或引入它们的隔离框架。
