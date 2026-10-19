set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "T0\n"
x/1wx 0x24007344
x/1wx 0x4000441c
x/1wx 0x4000481c
x/1wx 0xE000E204
x/1wx 0xE000E304
printf "PC_T0\n"
info registers pc sp
monitor resume
shell sleep 0.3
monitor halt
printf "T1\n"
x/1wx 0x24007344
x/1wx 0x4000441c
x/1wx 0x4000481c
x/1wx 0xE000E204
x/1wx 0xE000E304
printf "PC_T1\n"
info registers pc sp
monitor resume
shell sleep 0.3
monitor halt
printf "T2\n"
x/1wx 0x24007344
x/1wx 0x4000441c
x/1wx 0x4000481c
x/1wx 0xE000E204
x/1wx 0xE000E304
printf "PC_T2\n"
info registers pc sp
monitor resume
detach
quit
