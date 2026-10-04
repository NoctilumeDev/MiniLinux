# 遗留物收口门禁

阶段退出时，只把仍有职责的东西带入下一阶段。先盘点职责、整理引用，再清理；验证完成后回收验证本身的临时产物，最后检查门禁并展示 README。

## 四个过门条件

- 当前页面、README、文档和运行入口的引用仍成立；历史证据明确写成历史。
- 关键失败机制仍有可重跑的见证。保留首败和有意义的反例；删掉重复成功截图不影响这个责任。
- 没有丢掉仍需保留、但只有本地一份的源码、数据或失败见证。Git 能保存已提交的删除记录，不能替未跟踪文件、忽略文件或临时目录提供备份。
- 一次性构建、测试副本、进程和缓存已经收口；保留的工具、发布包与证据有明确用途。

体积只是盘点线索，不是删除依据。没有引用不自动等于可删；有旧引用也不自动等于必须永久留存。

## MiniLinux 的归属

| 材料 | 归属与处理 |
| --- | --- |
| 当前体验截图、首次失败、关键反例、最新实跑结果 | 保留在 docs/evidence；历史轮次不冒充当前结果。 |
| 普通过程截图、被后续见证覆盖的成功截图 | 整理引用后删除。Git 记录删除，不另造归档。 |
| build、ISO 解包目录、临时客体/下载 runtime、测试副本 | 可重建施工产物；核对唯一状态后回收。 |
| D 盘固定工具与其许可/来源材料 | 开发实验台继续使用，保留。 |
| 正式 ZIP 与历史标签 | GitHub Release 拥有；不移动标签，不覆盖旧字节。 |
| 实录 recording.json | 在线回放的输入，保留；不能作为普通临时日志删除。 |

ISO 检查在 finally 中移除自己的解包目录。补充 LAB 探针借用固定工具、使用私有镜像，结束移除临时用户夹具和 junction；工具目标不随之删除。探针结果写入 build/supplemental，失败时也保留故障文字与现场，供收口前核对。

## 怎么检查

补充边界已经加入 tools/check-lab.ps1；也可单独运行：

```powershell
.\tools\check-lab-boundaries.ps1
```

它重新构建当前 LAB，在 64/256 MiB 检查 fault/counter 两种顺序、结构输出跨页正反例、wait 的非子/自身/已回收 PID，以及坏 INPUT 指针不消费已排队字符。历史 fixture.patch（原提交由 `lab-boundary-fixture-20261002` 标签保留）只应用到临时用户源码，内核和 ABI 使用当前实现；补丁不再适用时直接失败，不能偷偷退回旧镜像。

验证按实际改动选择：文档/图片检查引用；构建或测试夹具改动运行相应实验。没有必要为每次删图重跑整个 M0–M6。把需要留下的首败或最终结果收进已有证据位置，回收 build 与 __pycache__ 后运行：

```powershell
& 'D:\python-3.10.6\python.exe' .\tools\check-hygiene.py
```

这条命令只检查本仓库使用的本地行内 Markdown 文件链接和 build/cache 残留，不删除文件，不访问外部环境，不检查远端 URL、引用式链接或标题锚点，也不代替上面的职责判断。扫描不跟随 junction/symlink。最后人工核对四项，阶段记录写一句结果即可；不增加清理哈希、截图或第二套验收体系。

## 本次收口：2026-10-05

状态：**待本地清理，门禁未通过**。引用整理和验证完成，不把这些通过等同于施工残留已经删除。

删除 10 张被后续见证覆盖的成功截图，保留首败、当前体验和独立滚动等关键证据；当前 LAB 完整检查及补充探针通过，见两份小结果：[普通 LAB](evidence/experience-regression/baseline-20261005-results.json)、[用户夹具](evidence/experience-regression/fixture-20261005-results.json)。坏 ISO、旧 ELF 载荷、不适用的用户补丁均被拒绝并清理各自的临时目录；断链、build 和 cache 反例也被门禁检查拒绝。没有改内核/用户实现、前端运行资源、旧发布包或历史判据。

原探针提交 e6746766 的 Git 身份已保留为 `lab-boundary-fixture-20261002`，其 patch 仍可用。首次启动失败的唯一 v1 ZIP 已核对原哈希，保留在本机 `D:\DevTools\MiniLinux\evidence\desktop-v1-first-failure.zip`，没有当作可重建缓存丢弃。M3 原始诊断补入已有证据位置；新验证只额外保留上述两个结果文件。

自动安全审核先后拒绝批量递归删除和单独删除主仓 build，只返回 `blocked by policy`，没有更具体原因。用户明确授权删除后再次尝试，批量和单目录命令仍在进程创建前被拒绝；32 个目录和八个 junction 均未删除，不再向用户重复索要授权。

供本机操作者使用的一次性脚本为 `D:\DevTools\MiniLinux\cleanup-approved-20261005.ps1`，已做语法检查，未由 agent 执行。它默认只预览，带 `-Execute` 才按下面的固定清单删除；会复查路径、原始 ZIP、历史提交、保留见证、运行进程与工具链接，遇到变动停止。成功清理并通过只读检查后移除脚本自身，不成为项目的永久清理系统。操作者执行后仍须重新核对实际结果，才能将门禁改为通过。待清理范围如下：

| 明确范围 | 本次盘点 |
| --- | --- |
| `C:\Users\lenovo\Desktop\GitHubProjects\MiniLinux\build` | 72.28 MiB，可重建产物；有职责的首败已另存。 |
| 同仓 `tools\__pycache__` | 可重建 Python 缓存。 |
| 工作区 tmp 下的九个旧 MiniLinux 验证目录 | 合计 48.76 MiB；源码已入历史或保留 patch/tag。仅 M0 无 Git 副本中的三份未提交草稿是已被当前实现取代的过程材料，不承接现行证明。 |
| Windows Temp 中先前盘点的 19 个 MiniLinux 临时目录 | 当前 17 个已空，其余是 7-Zip 解包副本和旧 Pages 对象，共约 9.66 MiB；正式工具档案、许可和发布仍有归属。先前数 GB runtime 已不在原路径，不计作本轮回收。 |
| 本次维护工作区的 build 与 tools/__pycache__ | 核对结果后回收；源码工作区仍用于审阅本次改动。 |

九个旧验证目录均位于 `C:\Users\lenovo\Desktop\GitHubProjects\tmp`：

- minilinux-closed-d296971-20261002
- minilinux-counterexamples-ae93e86-20261002
- minilinux-m0-checkout-f3843abb
- minilinux-m0-reaudit-d16dcbb9
- minilinux-m0-remote-45b160f-20260916
- minilinux-m0-verify-934080e1
- minilinux-m1-ec57b70-20261002
- minilinux-supplemental-cc937317-20261002-185828
- minilinux-userland-ac3e5cb-20261002

补充探针旧目录中的八个 junction 指向 D 盘工具：只解除链接，不递归其目标。残留全部收口后再运行检查、核对四项并将状态改为通过；本次不提前写 PASS，也没有为清理再造一套证据目录。

<details>
<summary>Windows Temp 的明确目录名（只限这些路径）</summary>

根为 `C:\Users\lenovo\AppData\Local\Temp`：

- MiniLinux desktop ASCII cc93731 20261002
- MiniLinux desktop 最终 v2 231fdd9 20261002
- MiniLinux desktop 独立验证 cc93731 20261002
- MiniLinux 新版下载者 20261002.1 独立试玩
- MiniLinux 新版浏览器 20261002.1 独立试玩
- MiniLinux 新版浏览器 20261002.2 独立试玩
- MiniLinux 陌生下载者 20261002 独立试玩
- MiniLinux 静默下载者 20261002.2 独立试玩
- MiniLinux-download-feasibility-cc93731-20261002
- MiniLinux-homepage-20261002
- MiniLinux-merge-20261002
- MiniLinux-pages-20261002
- MiniLinux-pages-readback-20261002
- MiniLinux-payload-check-20261002-new
- minilinux-product-audit
- MiniLinux-public-delivery-20261002.2
- MiniLinux-release-readback-20261002
- minilinux-review-cc93731
- MiniLinux-runtime-cc93731

本次维护工作区为 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-hygiene-20261005`，只清其中的 build 与 tools/__pycache__；PR 源码和当前审阅文件保留。

</details>
