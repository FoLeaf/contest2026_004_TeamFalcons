# QA-04 冻结后单次只读采集（2026-10-19 诊断固件 1b5e71d2f87）
#
# 前提：OpenOCD 在 Windows 侧以 connect_assert_srst 连上并 resume，板端已冻结。
# 本脚本只做 halt、读寄存器/内存、resume、detach，不写内存、不复位、不单步。
#
# 两个已知坑，均已在本脚本中规避：
#   1. 读 PC/LR 用 info registers，不要用 *(unsigned int*)$pc —— 后者读的是
#      PC 指向地址里的数据，不是寄存器值。
#   2. 当前镜像是 CONFIG_DEBUG_SYMBOLS=n 的发布构建，符号只有地址没有类型，
#      `print g_arm_freeze_diag` 会报 "has unknown type"。因此这里按
#      struct arm_freeze_diag_s 的实测布局逐字读取；布局由
#      arch/arm/src/common/arm_freeze_diag.h 编译探针得到（sizeof=80，
#      20 个字，字段偏移见下），不是推测。

set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt

printf "\n=== CPU ===\n"
info registers pc lr sp msp psp xpsr basepri primask control

printf "\n=== SCB / NVIC ===\n"
printf "ICSR   "
x/1wx 0xE000ED04
printf "SHPR3  "
x/1wx 0xE000ED20
printf "CFSR/HFSR/DFSR/MMFAR/BFAR\n"
x/5wx 0xE000ED28
printf "AIRCR  "
x/1wx 0xE000ED0C
printf "NVIC ACTIVE 0..3\n"
x/4wx 0xE000E300
printf "NVIC PENDING 0..3\n"
x/4wx 0xE000E200

printf "\n=== SysTick hardware ===\n"
x/4wx 0xE000E010
printf "g_system_tick_lock / g_system_ticks\n"
x/2wx 0x24007390

printf "\n=== Freeze diagnostic counters (g_arm_freeze_diag @ 0x24003cd0) ===\n"
printf "systick_entries      "
x/1uw 0x24003cd0
printf "systick_exits        "
x/1uw 0x24003cd4
printf "systick_last_tick    "
x/1uw 0x24003cd8
printf "pendsv_entries       "
x/1uw 0x24003cdc
printf "pendsv_exits         "
x/1uw 0x24003ce0
printf "pendsv_sigaction     "
x/1uw 0x24003ce4
printf "pendsv_null_regs     "
x/1uw 0x24003ce8
printf "pendsv_last_basepri  "
x/1uw 0x24003cec
printf "pendsv_last_primask  "
x/1uw 0x24003cf0
printf "pendsv_last_ipsr     "
x/1uw 0x24003cf4
printf "pendsv_last_xpsr     "
x/1uw 0x24003cf8
printf "pendsv_last_regs     "
x/1uw 0x24003cfc
printf "pendsv_last_saved    "
x/1uw 0x24003d00
printf "usart2_entries       "
x/1uw 0x24003d04
printf "usart2_exits         "
x/1uw 0x24003d08
printf "usart2_recv_before   "
x/1uw 0x24003d0c
printf "usart2_recv_after    "
x/1uw 0x24003d10
printf "usart2_last_isr      "
x/1uw 0x24003d14
printf "usart2_last_ie       "
x/1uw 0x24003d18
printf "usart2_last_rxlevel  "
x/1uw 0x24003d1c

printf "\n=== HMI acquisition thread progress ===\n"
printf "g_live_cycle         "
x/1uw 0x24020c1c
printf "g_live_n             "
x/1uw 0x24020c28
printf "g_acq_started        "
x/1uw 0x24020c24

printf "\n=== USART2 / USART3 registers (CR1 0x00, ISR 0x1c, RDR 0x24) ===\n"
printf "USART2 CR1 "
x/1wx 0x40004400
printf "USART2 ISR "
x/1wx 0x4000441c
printf "USART3 CR1 "
x/1wx 0x40004800
printf "USART3 ISR "
x/1wx 0x4000481c

monitor resume
detach
quit
