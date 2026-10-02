# MiniLinux .1 独立浏览器实玩证据

2026-10-02，独立新子智能体 /root/fresh_browser_playtest，自己的CUA IAB browser 2、tab 1。真实运行地址 http://127.0.0.1:52654/。没有启动第二个客体；原Start.cmd冷下载/启动由 /root/download_playtest 独立执行。

候选坐标由root提供：源 49554f2430c46924fba30c38c609f6a2c88433b2；raw ZIP SHA256 1c27d973c797bbdf978f578016b57492aacb2175eff8784a70c7c181d9e8121b；app.js SHA256 D94DC005938BDB5F29B5A35095189E1EC74919C11420C8B789275E0A722ABBC9；style.css SHA256 76E837C78C9E87333283D26E19F29FB201BC3A8242F9F741157469BAFFAFFD12。浏览器DOM独立读回引用 live.js / app.js / style.css。本agent未以源码代替运行观察，也未独立重新计算ZIP哈希。

## 真正操作结果

- help：返回 help | ls | cat FILE | ps | run PROGRAM [PROGRAM]；programs hello reader counter-a counter-b fault。
- ls：/hello.txt 17 bytes，/empty.txt 0 bytes。
- cat hello.txt：hello from ramfs。
- ps：PID1/init waiting，PID2/shell ready，CR3分别77000/82000。
- run hello：hello from user mode；PID3 exited status0。
- run reader：reading /hello.txt through syscalls；hello from ramfs；PID4 exit0。
- counter快捷按钮真正执行 run counter-a counter-b：PID5/a、PID6/b，均step1–5，各自marker a110/b220，最后exit0。
- counter结束后快照按钮启用；进入固定tick33955快照。PID5 CR3 104000，VA600000→PA10a000，marker a110；PID6 CR3 10f000，同VA600000→PA115000，marker b220。其他相同VA也映射不同PA。
- 快照下再cat返回hello from ramfs；run hello返回PID7 exit0。快照tick仍33955，Kernel Events新增spawn #15188 tick41569、exit #15201 tick41570、memory #15204 tick41570；旧Inspector/syscall切片保持，terminal/events真正继续。
- 回到实时观察后Inspector tick44671，Memory Map恢复PID2。
- run fault快捷按钮：PID8 vector14，exit status142；随后cat再次hello from ramfs。顶栏FAULT按预期筛选fault事件，读回#16964 tick46611。
- stop guest：STOPPED；Shell command/send/manual命令按钮disabled；明确提示“客体已停止；观察窗保留末次记录。点 restart guest 从启动重新运行。”；末次tick50270。
- restart guest：先RESTARTING再RUNNING，重建boot/PID1/PID2，counter快照disabled，tick重置到750；随后cat hello.txt仍成功。

## 已重复验证的标注困惑

counter结束→stop guest→查看最近counter快照：快照状态正确“快照 tick33955 · 客体已停止”，但退出按钮为“回到实时观察”。实际点击后回“末次记录 tick50270”，客体仍STOPPED；因此该状态下按钮文字承诺的实时观察与实际结果不符。建议非running时改为“回到末次记录”，running保留“回到实时观察”。root已收通知。

截图：snapshot-counter-a.jpg、snapshot-counter-b-live-terminal.jpg、fault-shell-survives.jpg、stopped-guidance.jpg、stopped-counter-snapshot-label.jpg、restarted-fresh-guest.jpg。

launcher关闭后的断连恢复提示待ownsVM代理结束原Start后补证。.2修正版尚未在本报告中标为通过。


## launcher关闭后补证

原Start.cmd owners于20:53:09前后Ctrl+C完成清理；本agent保留同一52654标签，实时读回DISCONNECTED，输入/send/stop/restart均disabled，中文恢复指导通过；截图 launcher-closed-disconnected-guidance.jpg。launcher进程/监听清理证据由 /root/download_playtest 独立保存，不由本agent浏览器画面推导。
