# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: D:\project\ZBSAR-B\oc\bootloader_system\_ide\scripts\debugger_bootloader-default.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source D:\project\ZBSAR-B\oc\bootloader_system\_ide\scripts\debugger_bootloader-default.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-SMT2 210251A08870" && level==0 && jtag_device_ctx=="jsn-JTAG-SMT2-210251A08870-43651093-0"}
fpga -file D:/project/ZBSAR-B/oc/bootloader/_ide/bitstream/ZBSAR_CPU_WCM_TOP.bit
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
loadhw -hw D:/project/ZBSAR-B/oc/ZBSAR_TOP/export/ZBSAR_TOP/hw/ZBSAR_TOP.xsa -regs
configparams mdm-detect-bscan-mask 2
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
rst -system
after 3000
targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2" }
dow D:/project/ZBSAR-B/oc/bootloader/Debug/bootloader.elf
bpadd -addr &main
