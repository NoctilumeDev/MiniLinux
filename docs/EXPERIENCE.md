# 体验入口与独立复验

## 主线状态与合并复验

2026-10-02，[PR #1](https://github.com/NoctilumeDev/MiniLinux/pull/1) 先合并到 `e1822e4ae4193e3f63d7bc973e2d6dbca3a7ac17`，再将 [PR #2](https://github.com/NoctilumeDev/MiniLinux/pull/2) 的 base 改为 `main` 并合并到 `1c938a267995c62ce1be60c6d132cd68a83b15ec`。两次冲突均仅为 README；保留新版试玩主页，运行源码与已复验的 `2e9279c` 没有差异。主线现在包含 M0–M6、真实用户态上层、前端与体验工具。

该合并坐标在本机干净工作区顺序执行 `tools/closed-loop.ps1`、`tools/counterexamples.ps1`、`tools/check-lab.ps1`，每轮重新构建 ELF/ISO 并核对载荷，全部通过。涵盖 M0–M6、64/256 MiB、寄存器/别名/DF 拒绝、权限与指针边界、异常接管、用户程序容量/回收以及真实 HTTP 桥接 stop/restart。结束工作树干净，QEMU/GDB 和 1234 监听均为 0。[总输出](evidence/main-1c938a2/all-checks.log)、[反例摘要](evidence/main-1c938a2/counterexamples-summary.log)、[用户态结果](evidence/main-1c938a2/userland-results.json)、[桥接结果](evidence/main-1c938a2/bridge-results.json) 与 [坐标及清理读回](evidence/main-1c938a2/receipt.json) 保留本轮事实；随后只更新主页、文档与这些证据。

在线回放和 `.2` 下载包保持原来经过独立试玩的字节及坐标，未重新发布或移动标签。以下是开发与交付期间的历史记录；合并复验不把同机结果升级成另一台 Windows 或 GitHub CI 的资格。

## 体验方式

在线演示是 **实录交互回放**：页面标注 REPLAY、录制日期、源码和片段进度。输入只选择已有片段，不在网页里执行新内核。console、PID、页映射和事件来自同一次真实 QEMU 录制；不会用预设故事生成假状态。可以暂停/继续、选择 PID 或事件、查看系统调用参数、清空视图和重置。

录制来自 `cc937317c7af9a99d8987059776654f0a85a57d8`，boot 加十条命令；原始快照在 [recording.json](evidence/replay/recording.json)。内核、用户 ELF 和 ISO 哈希均与本轮 LAB 载荷核验结果一致。用 `tools/export-demo.py --recording docs/evidence/replay/recording.json --output build/demo` 导出纯静态 HTML/CSS/JavaScript；没有浏览器 CPU 模拟器或远端执行服务。

Windows 启动包运行真实客体，包含固定 ISO、同一网页、标准库桥接和启动脚本。首次运行从官方来源下载约 220 MB 的 Python/QEMU 档案，仅解包到包内 runtime；后续使用缓存。无需编译器、GDB、WSL 或既有 Python/QEMU。7-Zip 解包工具附原许可及对应源码，Limine 和 Tux 许可保留。窗口保持打开，Ctrl+C 关闭桥接和自己的 QEMU；这仍是固定程序、固定文件的教学体验。

## 从公开下载重新走一遍

新的独立智能体从公开发布下载原始 v2 ZIP，在全新中文加空格目录首次联网准备并操作真实客体，没有借用 D 盘运行时。它先发现 `Start.cmd` 在 PowerShell 7 → cmd → Windows PowerShell 5.1 的继承模块环境中找不到 `Get-FileHash`，未能启动；首败保留。这不证明普通资源管理器双击失败，但确实推翻了该启动路径的资格。随后仅在单次子进程中调整模块路径以继续观察，包内 21 个原始文件的字节始终未改；这次受控通过不算原始启动脚本通过。

实际试玩完成 help、ls、cat、ps、hello、reader、两组 counter、fault 后继续 cat、stop/restart 与退出清理。试玩还发现九处具体入口、标注或观察问题。原报告、启动首败及清理见 [download-playtest/original](evidence/download-playtest/original/test-report.txt)。

| 真实问题 | 最小修补 |
| --- | --- |
| 启动继承错误的 PowerShell 模块路径 | Start.cmd 在自己的进程里选择系统 Windows PowerShell 模块，并允许直接传入代理地址；不改系统配置。 |
| 实录页面没有真实 LAB 下载下一步 | 右上增加 Windows 下载入口，未知输入提示指向该入口；README 明说实录回放。 |
| `run counters` 标题不是可输入的实际命令 | 按钮显示完整 `run counter-a counter-b`。 |
| counter 结束过快，来不及比较两个地址空间 | 保留最近一次两个 counter 同时存在的真实快照；PID、映射、free 和 syscall 采用同一帧，客体与日志继续。 |
| 停止/断连仍像当前可操作状态 | 标明末次记录或未知状态，禁用不适用的操作，指明 restart 或重开启动窗口并使用新网址。 |
| 中文读者不知道在哪输入代理命令 | 包内中文快速开始说明解压、打开所在目录的 PowerShell、代理、重试和退出。 |
| LIVE 的 Recorded 与 REPLAY 混淆 | LIVE 用 Observed；REPLAY 保留 Recorded。 |
| 手册按钮的动作与 fault 后果不清 | 明说按钮执行命令/选择实录，fault 提示 shell 继续。 |
| 首次准备与缓存阶段难辨 | 显示四步准备过程、首下载大小、缓存和重试说明。 |

新增安全提示和完整命令还触发小桌面手册正文仅 16px 的首败；保留字号与整体比例，让快捷区局部滚动并为正文留出完整行。修后的独立正常墙钟回放、合法 LIVE 夹具、free=0、五个原反例及六档布局全部通过，受测 app 为 `d94dc005…`；[最终小结果](evidence/download-playtest/frontend/snapshot-review-final-summary.json) 保留完整哈希。原首败 JSON 未改，原截图路径被后续脚本复写的事实也明确记录；另存同源码复现图，未冒充原始截图。

`lab-preview-20261002.1` 的公开原始 ZIP 从全新目录直接运行 Start.cmd，首次自下载并启动成功，没有人工修改模块路径。另一位有独立浏览器的智能体实际操作此客体，确认基本命令、双 counter 快照、快照时继续运行、fault 后 cat 与 stop/restart。它进一步发现停止后的“回到实时观察”其实返回末次记录；只修该标注，不改客体机制。

用户还指出复验反复自动打开默认浏览器造成干扰，以及日志滚动条位于黑框中间不直观。后续启动包增加 `Start.cmd --no-browser`（有代理时追加在代理后），打印网址供手动打开；正常双击仍打开一次。按用户选择，日志滚动条移到左侧，详情滚动条在右侧，中间竖线分区；记录仍从左到右读。实际分别用键盘滚动两栏，左侧翻到 0 时右侧仍在 64.67，右侧再翻到 0 时左侧保持 0，故障六字段不变。[独立滚动与六档几何](evidence/download-playtest/scroll/independent-scroll-results.json) 保留该轮实录观察，不冒充实时运行。

最终 `lab-preview-20261002.2` 固定源码 `6964b15c601bb69fa7886340645b5b295d3aa9bf`，公开原始 ZIP 为 2,933,901 字节、SHA256 `9532d9e39d8eb710c0fca11a69d3938ba1811cea129f71ac0783215fc29bc840`。独立智能体重新公开下载到全新中文加空格目录，直接用原始 `Start.cmd proxy --no-browser`，在原父模块环境冷准备并启动；两个官方运行时再次实际下载，没有旧包缓存或 D 盘模块。CIM 读回包内 Python `-I` 的实际 `--no-browser` 参数；原分发 21 个文件最终哈希均未改变。

第二位独立智能体操作这台客体的真实浏览器，完成所有命令、两次 counter、快照下继续执行、fault 后 cat、stop/restart 和断连。真实 1280×720 下日志 PageUp 从 1622.67 到 1544.67，详情保持 0；详情 PageDown 从 0 到 48.67，日志保持 1544.67；再滚日志时详情仍保持 48.67，选中的 fault 六字段不变。running/STOPPED/DISCONNECTED 分别显示“回到实时观察”/“回到末次记录”/“回到末次记录”，实际返回状态一致，原标注反例通过。[浏览器报告](evidence/download-playtest/revision-2/REPORT.md) 保留实际尺寸；该子线程的 viewport 设置没有改变实际尺寸，因此没有冒称它独立验证了 1024。

原 launcher Ctrl+C 后，其目录下 cmd/Python/QEMU、HTTP/两轮 COM 监听与已有连接均为 0；只运行 prepare 并传入不可达代理 `127.0.0.1:1` 仍成功（0.5566 秒），证明使用本包缓存，不宣称整机断网。下载代理负责原包身份与清理，浏览器代理负责实际点击；前者本轮浏览器工具不可用，明确没有把只读 API 旁证当成自己的 UI 操作。旧 ZIP 的首败保留，资格没有写回旧包。

最终公开页面部署为 `17b5184e83c1991fab3e03752e63e7b040b1ff8c`。匿名读回 app `a04b661d…`、replay `681a5214…`、style `d3c6861b…`、recording 均匹配导出文件；HTML 只有 Git 换行标准化，精确差异与完整哈希另存 [public-final](evidence/download-playtest/public-final/hashes.json)。实际浏览器确认 REPLAY、下载直链、正常文字方向、分区与一屏；[截图](evidence/download-playtest/public-final/published-demo.jpg) 保留实际发布结果。一次 CUA 下载事件等待超时，未取得文件路径，未归因于产品；实际公开 ZIP 下载与运行由上述独立代理完整验证。

## 这次怎么找问题

三位子智能体独立检查运行链、前端反例和机制源码，随后另有产品评审。QEMU 测试与预览串行，使用独立工作区，保留首败；没有为通过修改守卫或降低既有判据。

- **完整运行：**固定 `cc937317` 的干净工作区顺序执行 closed-loop、counterexamples、check-lab，全部通过。覆盖 M0–M6、GDB、坏寄存器/别名/DF 拒绝、两档内存、文件/指针/权限边界、程序容量与回收、真实桥接 stop/restart。清理读回无 QEMU/GDB 或调试端口残留。
- **新增机制探针：**LAB 同时运行 fault/counter，顺序互换，幸存者都完成五步；三个结构输出跨未映射页拒绝且合法部分不改写，跨已映射 stack 正例成功；非子/自己/已回收 PID 的 wait 被拒绝；坏 INPUT 指针未消费预排队的字符。两档内存通过，独立夹具 `e6746766b2916d0abab3be570b19e0565e8d250e` 只改用户探针，内核/ABI 不变。
- **前端首败：**TRACE→FAULT→TRACE 使记录变成 `[3,1,2,4]`；新客体非空增量漏掉前缀；80 条窗口淘汰键盘焦点；恢复连接后告警残留；latest 没有回到末尾。逐项修补后原反例通过，正确排序的节点仍保留身份和焦点。
- **真实回放：**独立浏览器核对全部 11 段终端文字、事件序列、PID、映射、syscall 和 fault 原字段；未知命令、片段切换、暂停/继续、固定详情、筛选、清空和重置可操作。不是只检查 HTTP 200。
- **产品评审：**约五秒的 counter 片段不便仔细比较，原停止态无继续入口。仅给 REPLAY 增加暂停/继续与进度，暂停时允许选另一片段；加三种演示的观察提示，真实 LAB 的 stop/restart 不变。

本轮小证据集见 [experience-regression](evidence/experience-regression)：首败、复验结果、原源码测试报告、独立夹具 patch 与清理读回分别保存。完整原始 build 快照留在报告中的独立工作区；网页回测按文件 SHA256 绑定，与 `cc937317` 的内核运行坐标分开。

第一次发布前的网页 `app.js` SHA256 为 `BD093A0B6BFF9D22AD382EA69E5B61C367EF463082E75BD9DE61A13959823FB7`，`replay.js` 为 `4E0CE2A833D4D752B51738B942161779B64DF3D913B45BB9284C34C41BA85416`。独立浏览器使用正常墙钟和实际点击，复验全部 11 段及暂停/继续；五个已知反例另以合法 API 夹具和正常轮询复验。结果分别在 `normal-clock-confirmed-results.json` 和 `normal-poll-api-fixture-results.json`，无失败或页面异常。一次 reset 检查提前命中了旧 boot 内容；等待新 REPLAY 读回后通过，原观察保留为 `normal-clock-pre-ack-first-failure.json`，不归为内核失败。

Windows v1 原始 ZIP 的首次官方网络下载成功，但解包到中文加空格目录后，QEMU 无法打开中文绝对 ISO 路径。原 ZIP 与日志保留，不能把后来的通过写回它。只将客体镜像和 BIOS 参数改为相对于包内 runtime 的路径，修后的同目录 launcher、64/256 MiB USERLAND 及 BRIDGE 原探针均通过；关闭后的 QEMU、包内 Python 和采样端口无残留。此修补不改变内核、用户代码或 ABI。

历史 v2 原始 ZIP 来自提交 `231fdd9a5bc2cc16f81e554d83f27ebc6b4b4182`，SHA256 `9210a0f44be614a99ae898cf0c2af370ca8822fa1fc00ba60a2086bc9b43d4f3`，2,930,931 字节。独立智能体原样解包到另一新的中文加空格目录，通过缓存准备、包内 Python `-I` launcher、完整 HTTP 命令、双 counter、fault 后 cat、stop/restart 和清理；运行中的 216 个模块没有 D 盘模块。v2 没有再重复首次网络下载或两档 USERLAND，前者在原 v1、后者在相同 Guest/ISO 的修后 v1 分别记录。Ctrl+C 的外层 PowerShell 返回 1，实际进程和端口清理通过，两种事实分开保留。后来的原始 Start.cmd 首败另在上节记录，这段通过不能覆盖它。

下载入口为 [Windows LAB preview](https://github.com/NoctilumeDev/MiniLinux/releases/tag/lab-preview-20261002)，标签固定在上述提交。发布后匿名 HTTPS 下载的字节数与 SHA256 均匹配受测 ZIP。三段资格、原始首败、哈希及清理读回见 [desktop](evidence/experience-regression/desktop)。这是同机独立解包资格，没有冒充另一台 Windows 的安装结果。

公开 [GitHub Pages](https://noctilumedev.github.io/MiniLinux/) 在 `gh-pages` 提交 `dc3399740b7e9fe78596852107733e2a3c944ae5` 部署成功。匿名下载 app/replay/recording 哈希与最终受测文件匹配；实际浏览器 cat 输出、选中 fault #1235 的六个字段均正确，1280×720 整页尺寸等于视口，warn/error 为空。[公开页面截图](evidence/replay/published-demo.jpg) 保留真实发布结果。

桌面验证的完整 HTTP 快照在 `desktop/03-desktop-http-results.json.gz`，无损压缩并逐字节解压核对，原文件仍在独立证据目录。产品评审剩余 P3 为移动端 PID 点击范围与控件间距，可作为后续体验微调；没有因此增添机制或扩大兼容范围。

尚未验证 spawn 中途 OOM 的所有分配深度、全部异常向量或所有 GPR 的独立 sentinel。init/shell 的异常仍明确 panic。没有把现有通过扩大成完整异常恢复、任意 ELF、POSIX 或其他宿主的资格；绝对 free 数值是每次客体观测，判据是同一次运行前后恢复。
