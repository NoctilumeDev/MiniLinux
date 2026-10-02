# 体验入口与独立复验

在线演示是 **实录交互回放**：页面标注 REPLAY、录制日期、源码和片段进度。输入只选择已有片段，不在网页里执行新内核。console、PID、页映射和事件来自同一次真实 QEMU 录制；不会用预设故事生成假状态。可以暂停/继续、选择 PID 或事件、查看系统调用参数、清空视图和重置。

录制来自 `cc937317c7af9a99d8987059776654f0a85a57d8`，boot 加十条命令；原始快照在 [recording.json](evidence/replay/recording.json)。内核、用户 ELF 和 ISO 哈希均与本轮 LAB 载荷核验结果一致。用 `tools/export-demo.py --recording docs/evidence/replay/recording.json --output build/demo` 导出纯静态 HTML/CSS/JavaScript；没有浏览器 CPU 模拟器或远端执行服务。

Windows 启动包运行真实客体，包含固定 ISO、同一网页、标准库桥接和启动脚本。首次运行从官方来源下载约 220 MB 的 Python/QEMU 档案，仅解包到包内 runtime；后续使用缓存。无需编译器、GDB、WSL 或既有 Python/QEMU。7-Zip 解包工具附原许可及对应源码，Limine 和 Tux 许可保留。窗口保持打开，Ctrl+C 关闭桥接和自己的 QEMU；这仍是固定程序、固定文件的教学体验。

## 这次怎么找问题

三位子智能体独立检查运行链、前端反例和机制源码，随后另有产品评审。QEMU 测试与预览串行，使用独立工作区，保留首败；没有为通过修改守卫或降低既有判据。

- **完整运行：**固定 `cc937317` 的干净工作区顺序执行 closed-loop、counterexamples、check-lab，全部通过。覆盖 M0–M6、GDB、坏寄存器/别名/DF 拒绝、两档内存、文件/指针/权限边界、程序容量与回收、真实桥接 stop/restart。清理读回无 QEMU/GDB 或调试端口残留。
- **新增机制探针：**LAB 同时运行 fault/counter，顺序互换，幸存者都完成五步；三个结构输出跨未映射页拒绝且合法部分不改写，跨已映射 stack 正例成功；非子/自己/已回收 PID 的 wait 被拒绝；坏 INPUT 指针未消费预排队的字符。两档内存通过，独立夹具 `e6746766b2916d0abab3be570b19e0565e8d250e` 只改用户探针，内核/ABI 不变。
- **前端首败：**TRACE→FAULT→TRACE 使记录变成 `[3,1,2,4]`；新客体非空增量漏掉前缀；80 条窗口淘汰键盘焦点；恢复连接后告警残留；latest 没有回到末尾。逐项修补后原反例通过，正确排序的节点仍保留身份和焦点。
- **真实回放：**独立浏览器核对全部 11 段终端文字、事件序列、PID、映射、syscall 和 fault 原字段；未知命令、片段切换、暂停/继续、固定详情、筛选、清空和重置可操作。不是只检查 HTTP 200。
- **产品评审：**约五秒的 counter 片段不便仔细比较，原停止态无继续入口。仅给 REPLAY 增加暂停/继续与进度，暂停时允许选另一片段；加三种演示的观察提示，真实 LAB 的 stop/restart 不变。

本轮小证据集见 [experience-regression](evidence/experience-regression)：首败、复验结果、原源码测试报告、独立夹具 patch 与清理读回分别保存。完整原始 build 快照留在报告中的独立工作区；网页回测按文件 SHA256 绑定，与 `cc937317` 的内核运行坐标分开。

最终网页 `app.js` SHA256 为 `BD093A0B6BFF9D22AD382EA69E5B61C367EF463082E75BD9DE61A13959823FB7`，`replay.js` 为 `4E0CE2A833D4D752B51738B942161779B64DF3D913B45BB9284C34C41BA85416`。独立浏览器使用正常墙钟和实际点击，复验全部 11 段及暂停/继续；五个已知反例另以合法 API 夹具和正常轮询复验。结果分别在 `normal-clock-confirmed-results.json` 和 `normal-poll-api-fixture-results.json`，无失败或页面异常。一次 reset 检查提前命中了旧 boot 内容；等待新 REPLAY 读回后通过，原观察保留为 `normal-clock-pre-ack-first-failure.json`，不归为内核失败。

Windows v1 原始 ZIP 的首次官方网络下载成功，但解包到中文加空格目录后，QEMU 无法打开中文绝对 ISO 路径。原 ZIP 与日志保留，不能把后来的通过写回它。只将客体镜像和 BIOS 参数改为相对于包内 runtime 的路径，修后的同目录 launcher、64/256 MiB USERLAND 及 BRIDGE 原探针均通过；关闭后的 QEMU、包内 Python 和采样端口无残留。此修补不改变内核、用户代码或 ABI。

最终 v2 原始 ZIP 来自提交 `231fdd9a5bc2cc16f81e554d83f27ebc6b4b4182`，SHA256 `9210a0f44be614a99ae898cf0c2af370ca8822fa1fc00ba60a2086bc9b43d4f3`，2,930,931 字节。独立智能体原样解包到另一新的中文加空格目录，通过缓存准备、包内 Python `-I` launcher、完整 HTTP 命令、双 counter、fault 后 cat、stop/restart 和清理；运行中的 216 个模块没有 D 盘模块。v2 没有再重复首次网络下载或两档 USERLAND，前者在原 v1、后者在相同 Guest/ISO 的修后 v1 分别记录。Ctrl+C 的外层 PowerShell 返回 1，实际进程和端口清理通过，两种事实分开保留。

下载入口为 [Windows LAB preview](https://github.com/NoctilumeDev/MiniLinux/releases/tag/lab-preview-20261002)，标签固定在上述提交。发布后匿名 HTTPS 下载的字节数与 SHA256 均匹配受测 ZIP。三段资格、原始首败、哈希及清理读回见 [desktop](evidence/experience-regression/desktop)。这是同机独立解包资格，没有冒充另一台 Windows 的安装结果。

公开 [GitHub Pages](https://noctilumedev.github.io/MiniLinux/) 在 `gh-pages` 提交 `dc3399740b7e9fe78596852107733e2a3c944ae5` 部署成功。匿名下载 app/replay/recording 哈希与最终受测文件匹配；实际浏览器 cat 输出、选中 fault #1235 的六个字段均正确，1280×720 整页尺寸等于视口，warn/error 为空。[公开页面截图](evidence/replay/published-demo.jpg) 保留真实发布结果。

尚未验证 spawn 中途 OOM 的所有分配深度、全部异常向量或所有 GPR 的独立 sentinel。init/shell 的异常仍明确 panic。没有把现有通过扩大成完整异常恢复、任意 ELF、POSIX 或其他宿主的资格；绝对 free 数值是每次客体观测，判据是同一次运行前后恢复。
