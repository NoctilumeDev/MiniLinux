# 从错题本回来攻击自己的实现

先推送候选 `d5462b4c3221f088ebb3562f8dbfd9be62dba992`，再扩充反例。外部修复提供问题线索；下面的实验由我们自己写，只运行自己的 QEMU 客体。不引入外部内核实现。

| 线索 | 我们要问的问题 | 先写下的判据 |
| --- | --- | --- |
| [xv6 拒绝 copyout 写只读页](https://github.com/mit-pdos/xv6-riscv/commit/d2b2dff7490f2c4b8e91f79940fc46f0361c216c) | 缓冲区检查是否真的与 CPU 权限一致？ | 可写叶项的上层只读时，read 必须返回 -2；不能进入内核 #PF，文件偏移和字节不动。 |
| [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) Vol. 3A §4.6 | 四级路径中谁决定 U/S、R/W、NX？ | 分别限制三个上层的三种权限，lookup 返回有效权限；非 present 不改输出。 |
| [xv6 修复 exec 的 OOM 清理](https://github.com/mit-pdos/xv6-riscv/commit/f2ec6777bd530f949c235d7f0386286314a2f601) | 页表只分配了一半时归谁所有？ | 真正耗尽页，仅留 0/1/2 页，让建表在不同深度失败；没有叶映射，销毁后页数恢复，正常映射能重新接管。 |
| [xv6 指针反例](https://github.com/mit-pdos/xv6-riscv/commit/a93321cb2547dbb48bf8ce9ad623ac19eefbecea) | 零长度、上界、跨页和整数溢出是否混在一起？ | 按本项目 256 字节 ABI 分别验证，失败不能写一半或吃掉文件字节。 |
| [SerenityOS 清除 DF](https://github.com/SerenityOS/serenity/commit/a15857ea2847cd8550c9b7414883cd1b8d4da782)、[Linux v6.10 入口](https://github.com/torvalds/linux/blob/v6.10/arch/x86/entry/entry_64.S) | 用户能设置的标志和寄存器是否污染 C 入口或返回？ | 用户带 DF=1 陷入，C 入口 DF=0；iret 恢复用户 DF 与寄存器，再由用户清 DF 继续。 |
| [xv6 用户异常与内核异常分开处理](https://github.com/mit-pdos/xv6-riscv/blob/d2b2dff7490f2c4b8e91f79940fc46f0361c216c/kernel/trap.c)、Intel SDM 的 #UD、#GP、#DE | 非页错误是否错误地落入整个内核 panic？ | 用户执行 ud2、端口 I/O、整数除零，分别得到 vector=6/13/0、error=0、CS=0x23；另一实例在故障后继续，最后回收完整。 |

这些是迁移后的问题，不把 RISC-V 页表规则照搬到 x86，也不为 fork、lazy allocation 或工业 Linux 增加需求。测试使用显式 `AttackCase` / `FaultCase`，默认构建没有攻击夹具；先保留失败，再修实现，不能改判据迎合输出。

## 先败，再修

运行前的夹具提交为 `d1417c5e7134d0fccb5e6822263bb10821917746`，其页表查询和异常分派仍是原候选的实现。真正复现了两类裂缝：

- **查询遗漏上层权限。** 夹具把任务 0 的数据页另映到 `0xa00000`，只清这个别名的 PDE 写位，保留可写叶项与正常代码、数据、栈。`read(fd, alias, 1)` 被用户指针检查放行，随后 CPU 在内核 memcpy 中报 #PF，error=3、CS=8、CR2=`0xa00000`。默认建表一直允许上层遍历，所以原闭环没有触及这个组合；这是映射权限变化后的接口裂缝，不宣称原用户程序能够任意修改页表。现在查询逐层收紧权限，read 返回 -2；后续仍读到完整 5+12 字节，证明偏移没被提前消耗。
- **只覆盖了用户 #PF。** 用户 `ud2`、端口输出、除零分别产生 #UD、#GP、#DE，现场都是 CS=`0x23`、error=0；原分派落入整个内核 panic，幸存者不能完成。现在这三种同步用户异常进入用户故障分支，并分别核对 vector=6/13/0。没有把内核异常一起吞掉，也没有把任意异常当作合格停机。

首败见 [未修补现场](evidence/counterexamples-unfixed.log)。原来的四个 #PF 仍保持各自的 CR2 与错误码判据；新异常是同一用户边界的补充证明。默认教学程序仍按指定故障、完成报告与幸存者行为严格验收，并非通用用户异常恢复系统。

另有三项**夹具/设置错误**，单列在 [设置记录](evidence/counterexamples-setup.log)：初轮 3 秒观察窗没拿到串口；新增 FaultCase 没有 switch 分支导致编译拒绝；权限矩阵把 `0x17` 与十进制 `17` 相比较。最后一项使原矩阵的失败原因不唯一，所以不把那条日志计作产品缺陷；真正的写权限首败来自上述 CPU #PF。没有降低编译警告，也没有改产品判据迎合失败。

## 可重跑的边界

```powershell
.\tools\counterexamples.ps1
```

五个 `AttackCase` 都在 64/256 MiB 下运行：真实 syscall 的只读上层、三个上层的 U/S/R/W/NX 与 non-present、真正耗尽物理页与 16 张页表上限、零长度/256 字节边界/跨页/非 canonical/溢出、DF 与九个寄存器。OOM 测试仅在自己的持有页内保存链表，失败后销毁自己拥有的半成品页表，再归还数据页与持有页；不要求 map 自动回滚全部空表。正常映射随后必须能重新接管。

用户 DF=1 保持至少四个时钟 tick，并经过真正的任务轮转和 int 0x80；C 入口读到 DF=0，用户返回读到自己的 DF=1，最后显式清 DF 再继续 C。GDB 单独确认并把 `interrupt_common` 的 cld 字节换成 nop，现场守卫必须拒绝；变更只在该次客体内存中，下一次正常映像仍通过。这个反例验证观察者能发现丢失 cld，不是 MiniLinux 原实现缺少 cld。

七种保护/异常探针分别运行，原始 #PF 加上 #UD/#GP/#DE；每轮仍要求任务 1 在故障之后计算、报告、退出，最终页数恢复。脚本最后重建普通映像并复验，检查没有持续客体、调试器或 1234 监听残留。完整 M0–M6 回归另用 `tools/closed-loop.ps1`，M6 已纳入七种异常。

修补源码固定在 `ae93e86f56592fd7b38732a5f664b6213b037c1a`，已推送并从 GitHub 干净克隆到 `C:\Users\lenovo\Desktop\GitHubProjects\tmp\minilinux-counterexamples-ae93e86-20261002`。开始时没有 build，工作树干净；先运行完整 `closed-loop.ps1`，再运行 `counterexamples.ps1`，全部通过。后者包含 22 次 64/256 MiB 客体运行、一次 cld 指令修改拒绝、两次普通映像重新接管；最后工作树仍干净，没有客体/调试器或 1234 监听残留。

精确 SHA、每个夹具的 ELF 哈希与载荷核对见 [汇总](evidence/counterexamples-ae93e86-summary.log)；客体现场、指令改前改后、两个进程的用户报告与物理页读回见 [实跑记录](evidence/counterexamples-ae93e86-witnesses.log)。M0–M6 回归与原有寄存器/别名反例也在同一个精确提交重跑，见 [回归记录](evidence/counterexamples-ae93e86-regression.log)。该轮结束时的后续提交只整理文档和记录，与 `ae93e86` 的运行源码一致；这描述当时的复验范围，不约束以后明确记录的维护改动。

这些实验覆盖列出的教学边界，不代表生产安全认证或穷尽所有硬件行为。
