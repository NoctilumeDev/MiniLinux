# MiniLinux 控制台：当前体验与历史错题

当前布局沿用黑白终端、观察窗、日志与手册。PC 上黑 38.2% / 白 61.8%，下排反转；外框在一窗内，内容按需在框内滚动。移动端自然纵向排列。以下列出已有验证，未因本次文档收口重做视觉资格。

## 当前版本的见证

- [Windows .2 实际桌面](docs/evidence/download-playtest/revision-2/fault-right-scrolled-left-stable.jpg) 与 [真实浏览器报告](docs/evidence/download-playtest/revision-2/REPORT.md)：1280×720，真实客体；故障六字段、全部命令、快照三态和 stop/restart。日志左侧滚动、详情右侧滚动，中间竖线，互不联动。
- [独立滚动读回](docs/evidence/download-playtest/revision-2/scroll-evidence.json) 保存两栏操作前后的位置；[六档几何](docs/evidence/download-playtest/scroll/independent-scroll-results.json) 来自另一次实录观察，不冒充实时客体或另一台机器。
- [最终公开页面](docs/evidence/download-playtest/public-final/published-demo.jpg) 与 [发布资源读回](docs/evidence/download-playtest/public-final/hashes.json)：明确 REPLAY 和 Windows 下载入口，录制与网页不被描述为新运行的内核。
- [前端最终小结果](docs/evidence/download-playtest/frontend/snapshot-review-final-summary.json) 保存正常墙钟回放、LIVE 夹具及首败回测范围；它绑定当时 app/style，随后最终资源身份见发布读回。

最新版 app/style 与这些最终下载/发布见证一致。更早的尺寸、版本与源身份仍在原报告中，不把所有历史材料称作“当前页面”。本次只整理文档和重复成功截图，没有改前端资源或发布包。

## 保留哪些设计错题

1. [原始参考稿](docs/evidence/userland-console/reference.png) 给出黑白结构来源；其中 CFS、libc 等虚构字段从未成为实现要求。
2. [早期过长页面](docs/evidence/userland-console/desktop.jpg) 是失败形态：上排撑高，下排不在一窗内。后来改为桌面 100dvh 分配空间，未靠缩小整页制造一屏；[该轮几何读回](docs/evidence/userland-console/viewport-layout.json) 保留结果。
3. [用户的日志间距原图](docs/evidence/userland-console/log-spacing-reference.png) 暴露两端分离与大片空白。字段收紧后，又按用户选择增加事件详情；[详情轮次几何](docs/evidence/userland-console/event-details-layout.json) 保留当时尺寸。其后才确定日志最左、详情最右的独立滚动方式。
4. [小桌面手册首败复现](docs/evidence/download-playtest/frontend/manual-first-failure-source-reproduced-1024.png) 显示正文被快捷按钮挤压；这是按原源码重现，不能冒充被复写的原截图。原失败结果与修补后的读回仍保留。
5. 下载试玩发现停止/断连时的提示错误、命令标题不完整、counter 太短而难观察；原报告、错误标注截图和最终回测仍在 [体验记录](docs/EXPERIENCE.md) 中。

仅为证明中间版本成功的重复桌面、窄窗与局部 polish 截图已移除；首败、当前体验、关键交互和几何记录继续保留。删除历史由 Git 承担，不另建图片归档或清理报告。

## 已确认的表达边界

终端和完整地址保持可读，不省略 CR3/RIP；长地址可在表格内横向查看。Tux 保留出处与许可。实际轮转、页映射、syscall 和 CPL 有来源，没有 kthreadd、CFS、libc 或虚构版本。LIVE 使用 Observed，REPLAY 使用 Recorded；停止/断连返回末次记录，不声称当前客体仍运行。

历史测试覆盖多个有效视口，没有逐档检查所有浏览器缩放倍率。较小桌面可在各框内查看更多记录，移动端接受长页面。详细范围和已知限制见 [EXPERIENCE](docs/EXPERIENCE.md)，这些同机观察不升级为跨宿主保证。
