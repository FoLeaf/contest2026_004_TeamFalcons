set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "STATE\n"
info registers pc lr sp msp psp basepri primask control
printf "ICSR_SHPR\n"
x/1wx 0xE000ED04
x/3wx 0xE000ED18
printf "SYSTICK\n"
x/4wx 0xE000E010
printf "TICKS\n"
x/2wx 0x24007340
printf "SCB_FAULT\n"
x/1wx 0xE000ED28
x/1wx 0xE000ED2C
x/1wx 0xE000ED34
printf "NVIC_EN\n"
x/4wx 0xE000E100
printf "NVIC_PEND\n"
x/4wx 0xE000E200
printf "NVIC_ACT\n"
x/4wx 0xE000E300
printf "IDLE_TCB\n"
x/8wx 0x24003d5c
printf "IDLE_XCP\n"
x/4wx 0x24003e04
printf "RUNNING\n"
x/2wx 0x24003d58
x/4wx 0x24000264
printf "WORKER_TCB_LOOKUP\n"
printf "MSP_WINDOW\n"
x/64wx 0x24047d00
monitor resume
detach
quit
