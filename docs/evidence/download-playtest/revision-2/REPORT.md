# MiniLinux 20261002.2 独立真实浏览器复验

独立子智能体 /root/fresh_browser_playtest，自己的IAB browser2/tab1。2026-10-02，同一Windows宿主；不是另一台机器证明。真实客体地址 http://127.0.0.1:49396/。launcher由 /root/download_playtest 从全新中文空格目录原Start.cmd --no-browser冷下载/启动，本agent未启动第二个VM、未操作root或用户标签。

## 候选坐标

公开ZIP SHA256 9532d9e39d8eb710c0fca11a69d3938ba1811cea129f71ac0783215fc29bc840；source 6964b15c601bb69fa7886340645b5b295d3aa9bf。app.js SHA256 A04B661D8F03865AA5D98D704CED7547EC73798C11F73B984A3B8E26BD0ACCBB；style.css SHA256 D3C6861B5D6AC75111BFAEF8D4CD99309C4184682CBB01E1182478E5913BF005。ZIP/source由下载owner独立读回，app/style坐标由root提供；本agent以该冷启动URL实际UI为证，未用源码模拟玩法。

## 命令与快照

help、ls、cat hello.txt、ps、run hello、run reader均实际执行成功；ls返回/hello.txt17 bytes、/empty.txt0 bytes；cat/reader返回hello from ramfs；hello/reader分别PID3/4 exit0。第一组counterPID5/6均step1–5、marker a110/b220、exit0。

最近快照tick20693：PID5 CR3 104000，VA600000→PA10a000，marker a110；PID6 CR3 10f000，同VA600000→PA115000，marker b220。running按钮“回到实时观察”，快照明确“客体继续运行”。快照状态下真实执行run fault：PID7 vector14，status142；终端/事件继续，Inspector保持快照，退出后回实时tick24299与PID2页表。fault后cat继续hello from ramfs。

## 两栏独立滚动实证

使用文档化viewport.set(1024×700)未改变本子线程实际视口；DOM与截图均真实1280×720。因此此报告不声称独立1024尺寸通过。根代理另保留不同尺寸验证。

初始boot少量详情：clientHeight114 / scrollHeight114，没有强制详情溢出。选择真实fault #8834 tick22948，六字段PID7、Vector14、Error5、Addressffffffff80000000、RIP4000ea、Saved CPL3。日志clientHeight114 / scrollHeight1736，详情clientHeight114 / scrollHeight162，两个区域真实overflow。日志direction rtl，记录子项ltr；详情ltr；两栏边界处详情border-left为0.666667px。

- 左日志实际PageUp：scrollTop1622.666626→1544.666626；右详情scrollTop保持0，Selected#8834及六字段不变。
- 右详情实际PageDown：scrollTop0→48.666668；左日志scrollTop保持1544.666626，Selected#8834及六字段不变，屏幕可见RIP/Saved CPL底部。
- 左日志实际滚轮向下：回scrollTop1622.666626；右详情仍48.666668，Selected#8834及六字段不变。

真实截图：fault-two-pane-before-scroll.jpg、fault-left-scrolled-right-stable.jpg、fault-right-scrolled-left-stable.jpg、fault-left-wheel-right-stable.jpg。scroll-evidence.json保留操作前后DOM只读值。

## 原.1标签反例与.2复验

原.1已保留停止状态仍写“回到实时观察”的首败，不覆盖旧报告。.2 stop guest后STOPPED、末次tick43410；进入最近counter快照，实际按钮正确“回到末次记录”，快照tick20693明确“客体已停止”；点击返回末次tick43410，没有声称实时。修正通过。截图 stopped-counter-snapshot-label-fixed.jpg。

restart guest实际先RESTARTING→RUNNING，重建boot/PID1/2、快照disabled、tick1050；重启后cat成功。为断连复验，重启后重新counterPID3/4均step1–5、marker a110/b220、exit0，现在固定快照tick2709、running退出按钮“回到实时观察”；截图 pre-disconnect-counter-snapshot.jpg。

## 尚待补证

owner正在最终Ctrl+C清理。DISCONNECTED动态按钮和实际退出将补证；该项暂不标通过。独立公开REPLAY导航/读AX遇到浏览器超时，已告知root并停止重复；不作为产品失败，不把REPLAY冒充实时。公开REPLAY新标签由root另有实际验证。


## DISCONNECTED补证完成

owner从原session42663 Ctrl+C关闭launcher，独立CIM/TCP清理已由owner核对并保存。本agent在同一49396标签、同一最近counter快照实际读回DISCONNECTED，退出按钮从“回到实时观察”自动变成“回到末次记录”；快照标签为“快照 tick2709 · 客体状态未知”。Shell command/send/stop/restart实际disabled，指导“连接中断，客体当前状态未知。若已关闭启动窗口，请重开 LAB，并打开它显示的网址。”。

实际点击“回到末次记录”，回“末次记录 tick23242”及PID2页表，仍DISCONNECTED，没有虚称实时。running / STOPPED / DISCONNECTED三种真实状态标签与按钮行为均通过。截图 disconnected-counter-snapshot-label-fixed.jpg、disconnected-back-to-last-record.jpg，另存disconnected-dom.txt。

本agent未发现.2本地实玩中的新增标注困惑。上述同机独立浏览器复验完成；根代理负责公开REPLAY及多尺寸记录，下载owner负责下载原件、准备/启动/清理证据。
