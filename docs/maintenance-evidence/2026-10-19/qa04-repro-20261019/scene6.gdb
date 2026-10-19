set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
printf "=== A: raw halt, no monitor halt ===\n"
info registers pc sp
x/1wx 0x24007344
monitor resume
shell sleep 1
monitor halt
printf "=== B ===\n"
info registers pc sp
x/1wx 0x24007344
x/1wx 0xE000E018
monitor resume
shell sleep 1
monitor halt
printf "=== C ===\n"
info registers pc sp
x/1wx 0x24007344
x/1wx 0xE000E018
monitor resume
shell sleep 1
monitor halt
printf "=== D ===\n"
info registers pc sp
x/1wx 0x24007344
x/1wx 0xE000E018
monitor resume
detach
quit
