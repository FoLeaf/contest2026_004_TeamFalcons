# QA-04 冻结后单次只读采集（2026-10-19 诊断固件）
#
# 前提：OpenOCD 已在 Windows 侧以 connect_assert_srst 方式连上并 resume，
# 板端已经冻结。本脚本只做 halt、读寄存器/内存、resume、detach。
#
# 关键区别（上一轮的教训）：读 PC/LR 用 info registers，不要用
# *(unsigned int*)$pc —— 后者读的是 PC 指向的地址里的数据。

set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt

printf "\n=== CPU ===\n"
info registers pc lr sp msp psp xpsr basepri primask control

printf "\n=== SCB ===\n"
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
printf "g_system_ticks / g_system_tick_lock\n"
x/2wx 0x24007340

printf "\n=== Freeze diagnostic counters ===\n"
print/x g_arm_freeze_diag
print g_arm_freeze_diag

printf "\n=== Acq thread progress (HMI live poll loop) ===\n"
print g_live_cycle
print g_live_n
print g_acq_started

printf "\n=== USART2 / USART3 registers (ISR offset 0x1c, RDR 0x24) ===\n"
printf "USART2 CR1 "
x/1wx 0x40004400
printf "USART2 ISR "
x/1wx 0x4000441c
printf "USART2 RDR "
x/1wx 0x40004424
printf "USART3 CR1 "
x/1wx 0x40004800
printf "USART3 ISR "
x/1wx 0x4000481c

monitor resume
detach
quit
