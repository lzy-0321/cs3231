set can-use-hw-watchpoints 0
define connect
dir /mnt/c/Users/Mando/Documents/cs3231/asst3-src/kern/compile/ASST3
target remote localhost:16161
b panic
end
