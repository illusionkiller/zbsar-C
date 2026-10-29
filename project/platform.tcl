# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct D:\project\ZBSAR-C\project\project\platform.tcl
# 
# OR launch xsct and run below command.
# source D:\project\ZBSAR-C\project\project\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {project}\
-hw {D:\project\ZBSAR-C\ZBSAR_C.xsa}\
-proc {microblaze_0} -os {freertos10_xilinx} -out {D:/project/ZBSAR-C/project}

platform write
platform generate -domains 
platform generate
platform generate -domains freertos10_xilinx_domain 
domain create -name {standalone} -os {standalone} -proc {microblaze_0} -arch {32-bit} -display-name {standalone} -desc {} -runtime {cpp}
platform generate -domains 
platform write
domain -report -json
domain active {freertos10_xilinx_domain}
bsp reload
bsp setlib -name lwip213 -ver 1.0
bsp removelib -name lwip213
bsp setlib -name lwip213 -ver 1.0
bsp setlib -name xilffs -ver 5.0
bsp setlib -name xilflash -ver 4.9
bsp removelib -name xilflash
bsp setlib -name xiltimer -ver 1.2
bsp write
bsp reload
catch {bsp regenerate}
domain -mss {D:\project\ZBSAR-C\project\ZBSAR_TOP\microblaze_0\standalone_microblaze_0\bsp\system_0.mss}
domain active {standalone}
bsp reload
domain active {freertos10_xilinx_domain}
domain -mss {D:\project\ZBSAR-C\project\ZBSAR_TOP\microblaze_0\standalone_microblaze_0\bsp\system.mss}
bsp reload
catch {bsp regenerate}
domain -mss {D:\project\ZBSAR-C\project\ZBSAR_TOP\microblaze_0\freertos10_xilinx_microblaze_0\bsp\system.mss}
platform generate
domain -mss {D:\project\ZBSAR-C\project\ZBSAR_TOP\microblaze_0\freertos10_xilinx_microblaze_0\bsp\system.mss}
bsp reload
platform generate -domains 
platform clean
platform clean
platform generate
