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

尚未验证 spawn 中途 OOM 的所有分配深度、全部异常向量或所有 GPR 的独立 sentinel。init/shell 的异常仍明确 panic。没有把现有通过扩大成完整异常恢复、任意 ELF、POSIX 或其他宿主的资格；绝对 free 数值是每次客体观测，判据是同一次运行前后恢复。
