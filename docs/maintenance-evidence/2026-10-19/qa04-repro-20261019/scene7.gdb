set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
set $i = 0
while $i < 24
  monitor halt
  printf "S%02d ", $i
  x/1wx 0xE000ED04
  x/1wx 0x24007344
  monitor resume
  shell sleep 0.15
  set $i = $i + 1
end
monitor halt
printf "FINAL\n"
x/1wx 0xE000ED04
x/1wx 0x24007344
x/1wx 0x40004400
x/1wx 0x40004404
x/1wx 0x40004408
x/1wx 0x40004800
monitor resume
detach
quit
