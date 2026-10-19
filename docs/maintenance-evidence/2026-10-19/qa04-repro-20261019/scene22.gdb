set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "MSP_FULL_WINDOW\n"
x/160wx 0x24047d80
monitor resume
detach
quit
