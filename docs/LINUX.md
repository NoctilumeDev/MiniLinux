# Linux：先试玩，或从源码开发

MiniLinux 客体是单核 x86_64、BIOS CD、TCG。Linux 宿主不需要 KVM、桌面或嵌套虚拟化。两条入口都运行真实内核，在线 REPLAY 则只是录制回放。

## 只想玩：下载免编译包

1. 下载 [Linux x86_64 LAB](https://github.com/NoctilumeDev/MiniLinux/releases/download/lab-linux-preview-20261018/MiniLinux-LAB-Linux-x86_64.tar.gz)。
2. 准备运行工具。Ubuntu 22.04 / 24.04 可直接用发行版包，无需自己编译 QEMU：

   ```bash
   sudo apt-get update
   sudo apt-get install --no-install-recommends python3 qemu-system-x86 seabios
   ```

3. 解压并启动：

   ```bash
   tar -xzf MiniLinux-LAB-Linux-x86_64.tar.gz
   cd MiniLinux-LAB
   sh Start.sh
   ```

4. 手动打开终端打印的本次网址。试 `cat hello.txt`、`run counter-a counter-b`、`run fault`；保持启动终端，Ctrl+C 关闭桥接与客体。

需要 Python 3.10+、QEMU 6.2+ 与相应固件。**不需要 Clang、LLD、pip、venv 或源码构建。** 包内已有 ISO，启动时不联网、不自动安装工具或弹出浏览器。系统工具由发行版管理；本包不替换它们。其他发行版请用自己的软件管理器安装这些运行工具，再以实际启动结果判断，不保证任意 Linux 都通过。

启动会核对包内 ISO 的哈希。端口默认自动选择；不要沿用上一次网址。需要固定端口时运行 `sh Start.sh --port 8088`；端口占用会明确提示换端口，并且尚未启动客体。

## 想读代码、改内核：源码开发

按以下顺序操作；已有系统工具可跳过第 1 步。

1. 安装开发工具（Debian/Ubuntu 包名）：

   ```bash
   sudo apt-get update
   sudo apt-get install --no-install-recommends git python3 python3-venv clang lld llvm qemu-system-x86 seabios
   ```

2. 克隆并一次准备项目环境：

   ```bash
   git clone https://github.com/NoctilumeDev/MiniLinux.git
   cd MiniLinux
   python3 -B tools/linux.py setup
   ```

   `setup` 先检查工具，再用当前 `python3` 创建 `.tools/linux/venv`，安装 `pycdlib==1.20.0`、准备固定 Limine 12.9.0，并运行 doctor。它不 sudo、不改系统 Python、全局 PATH 或代理；重复执行复用私有环境。Python 只要求 3.10+，实际解释器路径与版本以终端和 doctor 输出为准，不强制使用另一个系统 Python。

3. 启动真实 LAB：

   ```bash
   .tools/linux/venv/bin/python -B tools/linux.py lab
   ```

   打开打印的网址。`lab` 会构建、制作并核对 ISO；默认使用空闲回环端口。想固定端口可加 `--port 8088`。Ctrl+C 停止后再构建，不要让两个入口同时写同一工作区。

4. 按需要检查机制：

   ```bash
   .tools/linux/venv/bin/python -B tools/linux.py run --stage M0
   .tools/linux/venv/bin/python -B tools/linux.py run
   .tools/linux/venv/bin/python -B tools/linux.py check
   ```

   `run --stage M0` 观察最小启动；默认 `run` 用十条 shell 命令演示用户态闭环；`check` 做更广的机制、异常及桥接回归，会重建多次镜像。开发和试玩不必每次把三条全跑一遍；不同入口的实际执行范围分别记录，不把它们视为同一份结果。

`doctor` 输出源码坐标、工作区状态、实际 QEMU 路径、版本、CPU 配置和 LLVM 信息。LLVM 使用本机工具，不编造未经实跑的通用最低版本；当前已测版本见 CI 环境记录。工具不在 PATH 时可加 `--llvm-bin /absolute/llvm/bin`；也可在当前终端选择自己的 QEMU，但不需把发行版 QEMU 升级到 9。

构建、镜像与启动仍可单独调用 `build`、`prepare`、`doctor`；源码内固定 `include/limine.h`，`.S` 用 Clang 集成汇编器。镜像复用 `make-image.py`，不另加 NASM、xorriso 或 Limine 宿主安装器。

## 为什么现成的 QEMU 6.2 现在能用了

旧入口依赖 QEMU 默认 CPU。MiniLinux 在客体里关闭 LAPIC、使用传统 PIC/PIT；旧 QEMU 的这条中断路由存在兼容问题。之前的 QEMU 6.2 能进入 shell，却在双 counter 超时，tick 为 0；这条历史失败不撤销。

现在两个入口都显式使用 `-cpu qemu64,-apic`、`-smp 1`：虚拟 CPU 从启动时就不暴露 LAPIC，与这份 PIC/PIT 教学内核的目标一致。**没有修改内核中断、调度、隔离机制或原客体测试判据。** 版本检查仍拒绝低于 6.2、无法识别、查询失败或超过 5 秒的 QEMU；版本达标只允许继续运行，不代替实跑。

同机、同一 QEMU 6.2 与同一份映像的 [对照实验](https://github.com/NoctilumeDev/MiniLinux/actions/runs/39111827674)：原 CPU 超时、tick=0；显式 PIC CPU 完成十条 LAB 命令，trace 最大 tick=565、506 次调度。[原始小包](evidence/cpu-profile-experiment-20261018.zip)保留两组记录。

最终入口的 [Ubuntu 22.04 / 24.04 完整验证](https://github.com/NoctilumeDev/MiniLinux/actions/runs/39112894107)源码为 `2d20501`：分别使用发行版 QEMU 6.2 / 8.2.2、LLVM 14 / 18.1.3，均通过原有检查与 10 项宿主回归。体验包还在全新解压目录、只含 Python/QEMU/shell 工具的 PATH 下核对 cat、双 counter、fault、页回收、Ctrl+C 退出及端口/QEMU 回收。[22.04 原始记录](evidence/linux-onboarding-20261018-ubuntu22.zip)与[24.04 原始记录](evidence/linux-onboarding-20261018-ubuntu24.zip)保留；发布包绑定这个受测源码，后续证据/文档整理没有重打包或替换其字节。这不是任意后续提交的自动通过声明。验证工作流保留为手动入口，避免单纯文档或证据整理反复启动客体。

本机 Windows QEMU 11.1 的新 CPU 配置也完成了双 counter、时钟、fault 与页回收检查；使用的是固定工具中的既有 LAB 映像，只算宿主配置回归。Linux CI 的包测试通过 HTTP 操作，不冒充浏览器视觉体验。原 Windows/GDB 现场篡改、坏 ISO 等资格继续单列。

## 云主机与排错

服务器始终只绑定回环地址，不开放公网监听。云主机的 `127.0.0.1` 不是自己电脑的地址。可先固定远端端口：

```bash
# 远端源码入口；体验包可用 sh Start.sh --port 8088
.tools/linux/venv/bin/python -B tools/linux.py lab --port 8088
```

然后在自己电脑另开终端：

```bash
ssh -N -L 8088:127.0.0.1:8088 user@host
```

把 `user@host` 换成云主机账号与地址，本地浏览器打开 `http://127.0.0.1:8088/`。不提供 SSH 的云沙箱需用平台已有的浏览器或端口访问能力；没有就记录“浏览器未执行”，可单独用 `run` / `check` 验证机制。

下载遇到 403、DNS 或权限错误时，保留首败并作为环境阻断；脚本不设置全局代理。Limine 下载校验相同的固定 SHA256，也支持离线档案：

```bash
.tools/linux/venv/bin/python -B tools/linux.py prepare --limine-zip /path/to/limine-binary-v12.9.0.zip
```

复验记录 HEAD、工作区状态、doctor、第一处失败和入口退出码。Bash 使用 `set -o pipefail` 防止 tee 掩盖失败；若需要精确的入口码，在管道后立刻保存 `${PIPESTATUS[0]}`，不要拿末尾 echo 的 0 代替。`build/lab-console.log`、`lab-trace.log`、`lab-qemu.log` 在下一客体启动时会覆盖，需要保留时先复制，再运行另一个入口。

串口案例在 `build/linux-checks`，用户态/桥接 JSON 在 `build`，边界探针在 `build/supplemental`。`check` 包括 M0–M6、五组 attack、六组 fault 的 64/256 MiB 见证，以及原用户态、桥接与 baseline/fixture 边界；不包括 Windows/GDB 篡改或仓库外 monitor 组件。退出 0 只支持已执行范围。

`.tools`、venv 与 `build` 都是私有的可重建环境。先保留首败和有职责的结果，再按 [遗留物收口门禁](HYGIENE.md)回收，不把整套工具链或重复截图提交到仓库。

<details>
<summary>旧默认 CPU 配置下的 Linux 复验与失败边界</summary>

## 已发生的 Linux 复验

以下都是 `4eac29a` 的历史结果，不升级为后续任意提交、宿主或 QEMU 文件的通过声明。来源为操作者交付的原始日志和更正后证据包，本项目核对了其中的日志、结果 JSON 和 ELF 字节，没有在 Windows 上假装重新执行 Linux 测试。

| 环境 / 场景 | 已观察到什么 |
| --- | --- |
| Ubuntu 22.04、LLVM 14、QEMU 6.2.0 | doctor 放行、M0 通过；LAB 前六条命令返回，双 counter 在 8 秒内未返回，入口退出 1。原 trace 的 tick 全为 0、无 schedule 事件。 |
| Linux 沙箱、LLVM 14、一份 QEMU 9.2.2 | M0、LAB 全部十条命令及 check 退出 0。36 个串口案例、64/256 MiB 用户态、16 条桥接 HTTP 检查及 baseline/fixture 边界探针通过。另一次实时 LAB 的 API 操作通过。 |

首败保留为 [QEMU 6.2 doctor](evidence/linux-reproduction/qemu-6.2-doctor.log)、[LAB 超时输出](evidence/linux-reproduction/qemu-6.2-run-lab.log)和[原始 trace](evidence/linux-reproduction/qemu-6.2-trace.log)；后轮的原始日志、结果与更正见 [QEMU 9.2.2 证据包](evidence/linux-reproduction/qemu-9.2.2-evidence.tar.gz)。证据包是外部原件，只作为数据保留，不运行包内文件。包内更正说明中的一处 LAB 哈希误写及 MANIFEST 自哈希行无效，不作为身份依据；八份 ELF 的实际字节可直接比较。

这两轮不是严格单变量实验：LLVM 的 Host CPU 分别记录为 `sapphirerapids` 和 `znver3`，变化原因不知道。早期 M0/LAB 的完整 ELF 哈希两轮相同；后期 LLVM 改为本地解包路径后，ELF 调试段路径不同，但核对八份比较文件，M0/LAB 各自的装载段及去调试后的对应文件相同。不能因此声称整个 ISO、宿主环境均逐字节相同。

9.2.2 轮保留了三次非零尝试：两次缺 clang，以及一次手工解包 LLVM 漏设动态库路径；后来的通过没有覆盖这些失败。该 QEMU 自编译时的原始档案和构建日志已丢失，来源链无法补核验。它证明那份二进制的已执行范围，不证明所有同版本二进制。

上游 [PIC/APIC 修复](https://github.com/qemu/qemu/commit/c2e6d7d8e7fc270a90c61944ef36574b1549ddcf)与这次时钟现象吻合，[9.0.0 的中断路由](https://github.com/qemu/qemu/blob/v9.0.0/hw/i386/x86.c)已包含修复；这是当时将版本前提设为 ≥9 的依据，仍不把补丁写成两轮差异的唯一根因。8.2、9.0、9.1 没有在这轮实跑，Linux 浏览器视觉体验也未测。

旧 trace 中 spawn 时的 ready、marker=0 只反映建图时刻；wait 可以直接切换任务且不产生 schedule 事件，SYS_TICKS 也不记入 syscall 日志。因此不能从这些行断言 counter 始终没运行。挂住时 RIP、寄存器和数据页不知道。退出清理的报告与 SIGTERM 记录也不等于完整的进程/端口独立台账。

前置检查的轻量回归可运行 `python3 -B tools/test-linux-host.py`，不下载工具、不编译或启动客体；它覆盖旧版本拒绝、未知/失败/超时查询，以及跳过 doctor 时仍在运行入口提前拒绝。它不代替上述客体测试。


</details>

## 参考

[QEMU 官方 Linux 工具说明](https://www.qemu.org/download/#linux) · [上游 PIC/APIC 修复](https://github.com/qemu/qemu/commit/c2e6d7d8e7fc270a90c61944ef36574b1549ddcf) · [Python 子进程文档](https://docs.python.org/3/library/subprocess.html) · [Limine 官方 C 模板](https://github.com/Limine-Bootloader/limine-c-template-x86-64)。它们用于理解宿主工具边界，不作为本项目的运行证据，也没有复制外部内核。
