set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "PRIO_A\n"
x/4wx 0xE000E400+32
x/3wx 0xE000ED18
printf "AIRCR\n"
x/1wx 0xE000ED0C
printf "XPSR_ICSR\n"
info registers xpsr
x/1wx 0xE000ED04
printf "PRIO_B_AFTER_1S\n"
monitor resume
shell sleep 1
monitor halt
x/4wx 0xE000E400+32
x/1wx 0xE000ED04
info registers pc
monitor resume
detach
quit
