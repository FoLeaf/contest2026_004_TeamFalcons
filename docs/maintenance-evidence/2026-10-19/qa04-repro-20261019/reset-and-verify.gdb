set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor reset run
shell sleep 3
monitor halt
printf "AFTER_RESET_STATE\n"
info registers pc lr sp msp psp basepri primask
printf "SHPR\n"
x/3wx 0xE000ED18
printf "NVIC_PRIO_IRQ32_95\n"
x/16wx 0xE000E400+32
printf "SYSTICK\n"
x/4wx 0xE000E010
printf "TICKS\n"
x/1wx 0x24007344
printf "ICSR\n"
x/1wx 0xE000ED04
monitor resume
detach
quit
