set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
set $i = 0
while $i < 20
  monitor halt
  printf "S%02d pc=%08x sp=%08x lr=%08x ipsr=%d\n", $i, *(unsigned int*)$pc, $sp, *(unsigned int*)$lr, *(unsigned int*)(0xE000ED04) & 0x1ff
  monitor resume
  shell sleep 0.05
  set $i = $i + 1
end
monitor resume
detach
quit
