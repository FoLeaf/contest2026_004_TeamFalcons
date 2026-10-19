set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "HALT_A\n"
info registers pc sp
x/1wx 0x24007344
x/1wx 0xE000E018
x/1wx 0xE000ED04
monitor resume
shell sleep 2
monitor halt
printf "HALT_B\n"
info registers pc sp
x/1wx 0x24007344
x/1wx 0xE000E018
x/1wx 0xE000ED04
monitor resume
shell sleep 2
monitor halt
printf "HALT_C\n"
info registers pc sp
x/1wx 0x24007344
x/1wx 0xE000E018
x/1wx 0xE000ED04
monitor resume
detach
quit
