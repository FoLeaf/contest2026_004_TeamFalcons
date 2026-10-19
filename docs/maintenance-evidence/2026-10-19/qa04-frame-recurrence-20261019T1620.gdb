set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "STATE\n"
info registers pc lr sp msp psp basepri primask
printf "ICSR\n"
x/1wx 0xE000ED04
printf "SYSTICK\n"
x/4wx 0xE000E010
printf "SHPR3\n"
x/1wx 0xE000ED20
printf "TICKS\n"
x/1wx 0x24007344
printf "IDLE_TCB\n"
x/4wx 0x24003d5c+108
x/2wx 0x24003d5c+168
printf "RUNNING\n"
x/2wx 0x24003d58
x/2wx 0x24000264
printf "MSP_WINDOW\n"
x/48wx 0x24047f80
printf "IDLE_STACK_BASE\n"
x/8wx 0x24047ccc
monitor resume
detach
quit
