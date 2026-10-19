set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "RECV_STRUCT_FULL\n"
x/8wx 0x24000200
printf "HINT: size=%d head=%d tail=%d\n", *(int*)0x24000200, *(int*)0x24000204, *(int*)0x24000208
printf "XMIT_STRUCT\n"
x/8wx 0x240001e4
printf "USART3_RECV_STRUCT\n"
x/8wx 0x24000134
printf "priv_ie_field_candidates\n"
x/40wx 0x24000198
monitor resume
detach
quit
