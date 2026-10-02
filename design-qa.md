# MiniLinux 控制台视觉与交互复验

final result: passed

## 日志间距复查（2026-10-02）

用户实际截图暴露日志的附加信息被推到最右端，中间留下过宽空白（P2）。此前的全页截图保留为外框验证；最新日志局部见 [log-compact.jpg](docs/evidence/userland-console/log-compact.jpg)，829×157，截取自 1366×768 的真实页面。用户提供的 [局部原图](docs/evidence/userland-console/log-spacing-reference.png) 为 1572×327，与新截图放在同一次图像输入中比较；像素尺寸和显示倍率不同，不作逐像素匹配。

只修改日志两条 CSS：事件列按内容收紧，附加信息左对齐，相邻字段间距 12 px。外框比例、终端/表格布局、13 px 日志字号、行高、颜色和资产均未改变，实际记录与完整地址仍保留。1920×1080、1366×768 仍无整页溢出；390×844 的日志和整页均无横向溢出。长故障记录正常换行，FAULT/TRACE 切换可用，浏览器 warn/error 为空。中间过宽间距已消除，日志右侧保留自然留白。

## 当前对照与证据

- 原稿：[reference.png](docs/evidence/userland-console/reference.png)，1448×1086；上方约 74 px 为浏览器外壳。黑白终端、观察窗、日志、手册的结构保留，尺寸按用户后来选定的比例和“一窗完整外框”要求调整。
- 当前桌面：[viewport-desktop.jpg](docs/evidence/userland-console/viewport-desktop.jpg)，1366×768；CSS 视口同为 1366×768，devicePixelRatio 约 1。未缩小整页或图片来制造一屏效果。
- 当前窄窗：[viewport-narrow.jpg](docs/evidence/userland-console/viewport-narrow.jpg)，374×2412；CSS 视口 390×844，扣除滚动条后 clientWidth 为 375 px，截图有 1 px 取整差异。没有整页横向溢出。
- [viewport-layout.json](docs/evidence/userland-console/viewport-layout.json) 保留八档几何读回。桌面截图是完整页面截图，document scrollHeight 与 innerHeight 相等；因此不是把下排裁掉的视口局部截图。
- 当前桌面状态是真实 init/shell、cat 和并发 counter，窄窗状态是该轮程序退出后的记录。原稿带虚构 CFS/libc 等字段，不能逐值或逐像素声称相同。
- 2026-10-02 将原稿与当前桌面、窄窗截图放在同一次图像输入中比较。完整分辨率下标题、终端、地址表、按钮和手册文字可以直接审视，无需额外裁图；移动长图的展示缩略图不作为字号测量。

## 发现与修补历史

1. 过多小观察框挤压文字（P2）：合并调度状态和模式说明，保留终端 15 px、表格 14 px。用户选定上黑 38.2% / 白 61.8%，下排反转。
2. 长历史撑大上排、推远输入（P1）：此前限制上排 520–640 px，内容框内滚动。但整页仍可滚动，旧的 passed 没有证明一屏显示；旧 [desktop.jpg](docs/evidence/userland-console/desktop.jpg) 是历史状态。
3. 用户实际复查暴露下排被截住（P1）：1366×768 中旧页面高 1184 px。改为桌面 body 占 100dvh，页眉/导航/状态/页脚先占所需高度，上下两排共同分配剩余空间。六档桌面均 pageHeight = viewportHeight，输入和页脚在视口内。
4. 右上空白与左下日志右侧空白（P2）：进程/内存并排，syscall 横跨下一行；日志按实际时间、事件和地址等字段分列。完整地址保留，重复 PID/tick/地址说明收紧；PID 状态、tick、映射与调度切换仍各有可见出处。
5. 最矮窗口的进程记录区曾为零（P1）：纠正网格 fr 分配，短窗口多分一点高度给上排，未缩小正文。1152×640 中进程区约 67 px、trace 约 49 px，至少表头和一条完整记录可见，其余可框内滚动。
6. 手册新增框内滚动后，换主题可能停在旧位置（P2）：切换主题返回开头，实际滚动后切到 CAT(1) 读回 scrollTop = 0。

## 五项视觉检查

| 项目 | 比较与结论 |
| --- | --- |
| 字体与层级 | Georgia 大衬线标题，Courier New 等宽正文。桌面标题 46 px，终端 15 px / 1.5 行高，表格 14 px，地址 13 px；移动端终端 14 px、地址 12 px。只压缩标题区和重复信息，未缩小整页；完整地址不省略。 |
| 间距与布局 | 桌面上排框宽约 513/829 px，下排反转，间隙 8 px。四个外框、输入和页脚一窗可见；进程/地址/trace/终端/日志/手册各自滚动。移动端按上下排列，接受长页面。 |
| 颜色 | 墨黑 #0b0b0b、纸白 #eceae3、面板 #f7f6f1，细线边界，没有阴影渐变。绿色用于运行与输入，故障同时有文字提示。 |
| 图像 | 保留有出处许可的真实黑白 Tux SVG，缩放清晰，没有代码绘制的替代图标或占位图。 |
| 内容 | 显示实际轮转、页映射、syscall 与 recorded CPL 入口。没有 CFS、kthreadd、load average、libc 或虚构版本；日志分列使用原始记录字段。 |

## 本次浏览器验证

- 桌面 1920×1080、1448×900、1366×768、1280×720、1152×640、1024×700：document 宽高等于视口，页脚底部等于窗口底部。导航不会滚动整页。
- 1366×768 中双 counter 的四行进程记录可见；终端历史高于 1600 px，外框仍约 402 px，输入保持可见。
- 390×844、700×1000：document scrollWidth = clientWidth，长地址只在表格容器内横向滚动，移动端保留自然纵向布局。
- 真实 cat 返回文件；选择 PID 能查看 CR3/标记；暂停、恢复 trace 和点击参数可用；故障筛选显示 vector 14，shell 可继续 cat。
- 手册可键盘滚动；换主题返回开头；非 ASCII 提示出现时页面仍为 1366×768。输入 Enter 和上一条命令回忆可用。
- 停止后 STOPPED、输入禁用；重启后 RUNNING，只剩新客体 init/shell 和记录，旧终端清空。浏览器 warn/error 为空。
- 未逐档测试浏览器菜单的缩放倍率；上述较小有效视口是适配证据，不是所有缩放倍率的保证。更小的桌面高度可能需要在各框内查看更多记录。

本次只修改网页表现和文档，核对 kernel/include/user/tools/boot/linker.ld 与 ac3e5cb 无差异。M0–M6、LAB 和桥接的历史运行资格仍见 [USERLAND](docs/USERLAND.md)，没有用本次视觉通过升级内核声明。无未解决的 P0/P1/P2 项。
