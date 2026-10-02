# 从错题本回来攻击自己的实现

先推送候选 `d5462b4c3221f088ebb3562f8dbfd9be62dba992`，再扩充反例。外部修复提供问题线索；下面的实验由我们自己写，只运行自己的 QEMU 客体。不引入外部内核实现。

| 线索 | 我们要问的问题 | 先写下的判据 |
| --- | --- | --- |
| [xv6 拒绝 copyout 写只读页](https://github.com/mit-pdos/xv6-riscv/commit/d2b2dff7490f2c4b8e91f79940fc46f0361c216c) | 缓冲区检查是否真的与 CPU 权限一致？ | 可写叶项的上层只读时，read 必须返回 -2；不能进入内核 #PF，文件偏移和字节不动。 |
| [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) Vol. 3A §4.6 | 四级路径中谁决定 U/S、R/W、NX？ | 分别限制三个上层的三种权限，lookup 返回有效权限；非 present 不改输出。 |
| [xv6 修复 exec 的 OOM 清理](https://github.com/mit-pdos/xv6-riscv/commit/f2ec6777bd530f949c235d7f0386286314a2f601) | 页表只分配了一半时归谁所有？ | 真正耗尽页，仅留 0/1/2 页，让建表在不同深度失败；没有叶映射，销毁后页数恢复，正常映射能重新接管。 |
| [xv6 指针反例](https://github.com/mit-pdos/xv6-riscv/commit/a93321cb2547dbb48bf8ce9ad623ac19eefbecea) | 零长度、上界、跨页和整数溢出是否混在一起？ | 按本项目 256 字节 ABI 分别验证，失败不能写一半或吃掉文件字节。 |
| [SerenityOS 清除 DF](https://github.com/SerenityOS/serenity/commit/a15857ea2847cd8550c9b7414883cd1b8d4da782)、[Linux v6.10 入口](https://github.com/torvalds/linux/blob/v6.10/arch/x86/entry/entry_64.S) | 用户能设置的标志和寄存器是否污染 C 入口或返回？ | 用户带 DF=1 陷入，C 入口 DF=0；iret 恢复用户 DF 与寄存器，再由用户清 DF 继续。 |
| Intel SDM 的 #UD、#GP、#DE 与现有异常入口 | 非页错误是否错误地落入整个内核 panic？ | 用户执行 ud2、端口 I/O、整数除零，分别得到 vector=6/13/0、error=0、CS=0x23；另一实例在故障后继续，最后回收完整。 |

这些是迁移后的问题，不把 RISC-V 页表规则照搬到 x86，也不为 fork、lazy allocation 或工业 Linux 增加需求。测试使用显式 `AttackCase` / `FaultCase`，默认构建没有攻击夹具；先保留失败，再修实现，不能改判据迎合输出。

结果待实测填入。原来的四个 #PF 仍保持各自的 CR2 与错误码判据；新异常属于同一用户边界的补充证明。
