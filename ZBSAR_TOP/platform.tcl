# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct D:\project\ZBSAR\oc\ZBSAR_TOP\platform.tcl
# 
# OR launch xsct and run below command.
# source D:\project\ZBSAR\oc\ZBSAR_TOP\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {ZBSAR_TOP} -hw {D:\project\ZBSAR\ZBSAR_TOP.xsa} -out {D:/project/ZBSAR/oc}
platform write
domain create -name {freertos10_xilinx_microblaze_0} -display-name {freertos10_xilinx_microblaze_0} -os {freertos10_xilinx} -proc {microblaze_0} -runtime {cpp} -arch {32-bit} -support-app {freertos_lwip_echo_server}
platform generate -domains 
platform active {ZBSAR_TOP}
platform generate -quick
bsp reload
platform generate
bsp config total_heap_size "65536"
bsp config total_heap_size "0x100000"
bsp config max_task_name_len "32"
bsp config max_api_call_interrupt_priority "18"
bsp config max_priorities "32"
bsp config stream_buffer "true"
bsp config generate_runtime_stats "1"
bsp config timer_command_queue_length "10"
bsp setlib -name xilffs -ver 5.0
bsp setlib -name xiltimer -ver 1.2
bsp removelib -name xiltimer
bsp config fs_interface "2"
bsp config ramfs_size "3145728"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config xil_interrupt "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config xil_interrupt "true"
bsp reload
bsp setlib -name xiltimer -ver 1.2
bsp config en_interval_timer "false"
bsp config en_interval_timer "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config xil_interrupt "false"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config clocking "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config mem_size "131072"
bsp config mem_size "0x100000"
bsp config memp_n_udp_pcb "8"
bsp config memp_n_tcp_seg "256"
bsp config memp_n_pbuf "1024"
bsp config memp_n_tcp_seg "1024"
bsp config memp_num_netbuf "8"
bsp config memp_num_netbuf "4096"
bsp config default_tcp_recvmbox_size "4096"
bsp config default_udp_recvmbox_size "4096"
bsp config tcpip_mbox_size "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp write
platform generate -domains 
bsp reload
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
bsp config lwip_debug "true"
bsp config netif_debug "true"
bsp write
bsp reload
catch {bsp regenerate}
bsp reload
bsp config netif_debug "false"
bsp config lwip_debug "true"
bsp config netif_debug "false"
bsp config lwip_debug "true"
bsp write
bsp reload
catch {bsp regenerate}
bsp config lwip_debug "false"
bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform clean
platform generate
bsp config lwip_debug "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config ramfs_start_addr "0x90000000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform generate
platform config -updatehw {D:/project/ZBSAR/ZBSAR_TOP.xsa}
bsp reload
platform active {ZBSAR_TOP}
bsp reload
bsp config tcp_wnd "65535"
bsp config tcp_snd_buf "65535"
bsp config minimal_stack_size "1024"
bsp config total_heap_size "0x2000000"
bsp config use_idle_hook "true"
bsp config tick_rate "100"
bsp config timer_command_queue_length "16"
bsp config timer_command_queue_length "16"
bsp config lwip_tcp_keepalive "true"
bsp config no_sys_no_timers "false"
bsp config socket_mode_thread_prio "2"
bsp config socket_mode_thread_prio "10"
bsp config use_axieth_on_zynq "1"
bsp config lwip_debug "false"
bsp config mem_size "524"
bsp config mem_size "524288"
bsp config memp_n_pbuf "1024"
bsp config pbuf_pool_size "40960"
bsp config n_rx_descriptors "512"
bsp config n_tx_descriptors "512"
bsp config ramfs_size "0x10000000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config total_heap_size "0x100000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
bsp reload
bsp config mem_size "524288"
bsp config mem_size "0x100000"
bsp config total_heap_size "0x100000"
bsp config total_heap_size "0x200000"
bsp config tick_rate "100"
bsp config total_heap_size "0x200000"
bsp config total_heap_size "0x400000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config default_tcp_recvmbox_size "1024"
bsp config mem_size "131072"
bsp config default_udp_recvmbox_size "1024"
bsp config tcpip_mbox_size "1024"
bsp config pbuf_pool_size "4096"
bsp config tcp_snd_buf "8192"
bsp config tcp_wnd "2048"
bsp config n_rx_descriptors "128"
bsp config n_tx_descriptors "128"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config mem_size "0x100000"
bsp config pbuf_pool_size "40960"
bsp config pbuf_pool_size "4096"
bsp config pbuf_pool_bufsize "1700"
bsp config pbuf_pool_size "4096"
bsp config tcp_wnd "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config mem_size "0x100000"
bsp config total_heap_size "0x1000000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
catch {platform remove ZBSAR_TOP_1}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config memp_num_netbuf "1024"
bsp config default_tcp_recvmbox_size "1024"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp write
platform generate -domains 
bsp config mem_size "524288"
bsp config memp_n_tcp_pcb_listen "8"
bsp config memp_num_netbuf "4096"
bsp config default_tcp_recvmbox_size "1024"
bsp config tcpip_mbox_size "1024"
bsp config pbuf_pool_size "40960"
bsp config tcp_snd_buf "65535"
bsp config tcp_wnd "65535"
bsp config n_rx_descriptors "512"
bsp config n_tx_descriptors "512"
bsp config n_rx_descriptors "128"
bsp config n_tx_descriptors "128"
bsp config tcp_snd_buf "65535"
bsp write
bsp reload
catch {bsp regenerate}
bsp config no_sys_no_timers "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config no_sys_no_timers "false"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config tcp_snd_buf "65535"
bsp config tcp_snd_buf "8192"
bsp config tcp_wnd "65535"
bsp config tcp_snd_buf "65535"
bsp config n_rx_descriptors "512"
bsp config n_tx_descriptors "512"
bsp config mem_size "524288"
bsp config default_tcp_recvmbox_size "4096"
bsp config default_udp_recvmbox_size "4096"
bsp config tcpip_mbox_size "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform write
platform active {ZBSAR_TOP}
domain active {freertos10_xilinx_microblaze_0}
bsp reload
bsp reload
platform active {ZBSAR_TOP}
bsp config lwip_tcpip_core_locking_input "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
bsp reload
platform config -updatehw {D:/project/ZBSAR/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform config -updatehw {D:/project/ZBSAR/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform active {ZBSAR_TOP}
platform generate -domains 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR/ZBSAR_CPU_WCM_TOP.xsa}
platform config -updatehw {D:/project/ZBSAR/ZBSAR_CPU_WCM_TOP.xsa}
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
bsp reload
bsp reload
platform clean
platform generate
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
bsp reload
bsp config xil_interrupt "true"
bsp write
bsp reload
catch {bsp regenerate}
bsp reload
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
bsp config xil_interrupt "false"
bsp write
bsp reload
catch {bsp regenerate}
platform clean
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform generate
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform clean
platform clean
platform clean
platform clean
platform clean
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp reload
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
bsp reload
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform active {ZBSAR_TOP}
domain create -name {standalone_microblaze_0} -display-name {standalone_microblaze_0} -os {standalone} -proc {microblaze_0} -runtime {cpp} -arch {32-bit} -support-app {srec_spi_bootloader}
platform generate -domains 
platform write
domain active {freertos10_xilinx_microblaze_0}
domain active {standalone_microblaze_0}
platform generate -quick
platform generate -domains standalone_microblaze_0 
domain active {standalone_microblaze_0}
bsp reload
bsp reload
bsp reload
bsp reload
platform config -updatehw {D:/project/ZBSAR/oc/ZBSAR_TOP.xsa}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform generate -domains freertos10_xilinx_microblaze_0,standalone_microblaze_0 
platform clean
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
bsp reload
bsp reload
domain active {freertos10_xilinx_microblaze_0}
bsp reload
bsp config mem_size "524288"
bsp reload
bsp reload
platform active {ZBSAR_TOP}
domain active {freertos10_xilinx_microblaze_0}
bsp reload
platform generate -domains 
platform active {ZBSAR_TOP}
bsp reload
bsp write
platform generate -domains 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0,standalone_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-B/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-B/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-B/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-B/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-B/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform config -updatehw {D:/project/ZBSAR-B/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform active {ZBSAR_TOP}
bsp reload
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0,standalone_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
bsp reload
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform generate
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-B/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0,standalone_microblaze_0 
platform clean
platform generate
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-A/oc/ZBSAR_TOP.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
domain active {standalone_microblaze_0}
bsp reload
platform generate -domains 
platform generate -domains 
platform config -updatehw {D:/project/DSJ/DSJ_TOP.xsa}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
bsp reload
bsp reload
domain active {freertos10_xilinx_microblaze_0}
bsp reload
bsp reload
bsp config ramfs_start_addr "0x90000000"
bsp reload
bsp write
bsp write
bsp reload
platform generate -domains 
platform active {ZBSAR_TOP}
bsp reload
bsp reload
platform active {ZBSAR_TOP}
platform active {ZBSAR_TOP}
platform generate
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform generate -domains 
bsp reload
bsp reload
domain active {standalone_microblaze_0}
bsp reload
bsp reload
domain active {freertos10_xilinx_microblaze_0}
bsp config use_lfn "0"
bsp config use_lfn "3"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp reload
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A(1).xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp reload
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
bsp reload
bsp config udp_tx_blocking "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
domain active {standalone_microblaze_0}
domain active {freertos10_xilinx_microblaze_0}
bsp reload
bsp reload
bsp config memp_n_tcp_seg "1024"
bsp config memp_n_tcp_seg "4096"
bsp config memp_n_pbuf "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config tcp_snd_buf "65535"
bsp config tcp_snd_buf "8192"
bsp config tcp_wnd "2048"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config tcp_snd_buf "65535"
bsp config tcp_wnd "8192"
bsp config tcp_wnd "8192"
bsp config tcp_synmaxrtx "4"
bsp config tcp_snd_buf "65535"
bsp config memp_n_udp_pcb "8"
bsp config pbuf_pool_bufsize "4096"
bsp config pbuf_pool_size "4096"
bsp config tcp_snd_buf "32768"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config pbuf_pool_bufsize "1700"
bsp config pbuf_pool_size "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config tcp_wnd "16384"
bsp config pbuf_pool_size "4096"
bsp reload
bsp config tcp_snd_buf "32768"
bsp config tcp_synmaxrtx "4"
bsp config tcp_snd_buf "64"
bsp config tcp_snd_buf "64"
bsp config tcp_snd_buf "64"
bsp config tcp_snd_buf "64"
bsp config tcp_synmaxrtx "4"
bsp config tcp_snd_buf "65536"
bsp config tcp_wnd "8192"
bsp config tcp_wnd "8192"
bsp config tcp_wnd "58400"
bsp config default_tcp_recvmbox_size "200"
bsp config default_udp_recvmbox_size "100"
bsp config tcpip_mbox_size "200"
bsp write
bsp reload
catch {bsp regenerate}
bsp config default_tcp_recvmbox_size "4096"
bsp config default_udp_recvmbox_size "4096"
bsp config tcpip_mbox_size "4096"
bsp config pbuf_pool_size "10000"
bsp config pbuf_pool_size "10000"
bsp write
bsp reload
catch {bsp regenerate}
bsp config n_rx_coalesce "1"
bsp config n_rx_coalesce "1"
bsp config n_tx_coalesce "8"
bsp config n_rx_coalesce "8"
bsp config n_tx_coalesce "8"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config default_udp_recvmbox_size "1024"
bsp config tcpip_mbox_size "1024"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
bsp reload
bsp reload
bsp config ramfs_start_addr "0x90400000"
bsp config ramfs_size "0x6400000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform clean
bsp reload
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config n_tx_coalesce "1"
bsp config n_rx_descriptors "512"
bsp config n_rx_coalesce "1"
bsp config tcp_snd_buf "65536"
bsp config pbuf_pool_size "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config tcp_snd_buf "65536"
bsp reload
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
bsp reload
bsp write
bsp config udp_tx_blocking "true"
bsp config udp_tx_blocking "true"
bsp config lwip_udp "true"
bsp write
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config pbuf_pool_bufsize "4096"
bsp config pbuf_pool_size "40960"
bsp config pbuf_pool_size "4096"
bsp config pbuf_pool_bufsize "1700"
bsp config pbuf_pool_size "10000"
bsp config default_udp_recvmbox_size "4096"
bsp config tcpip_mbox_size "4096"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config mem_size "0x100000"
bsp config pbuf_pool_size "40960"
bsp config default_udp_recvmbox_size "1024"
bsp config tcpip_mbox_size "4096"
bsp config tcp_wnd "58400"
bsp config tcp_snd_buf "65535"
bsp config tcp_wnd "32768"
bsp config tcp_wnd "16384"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config mem_size "524288"
bsp config default_udp_recvmbox_size "4096"
bsp config tcp_wnd "65535"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config interval_timer "none"
bsp config tick_rate "100"
bsp config PSU_TTC0_Select "false"
bsp reload
platform generate -domains 
bsp config pbuf_pool_size "10000"
bsp config ramfs_start_addr "0x90000000"
bsp config ramfs_size "0x6400000"
bsp config ramfs_size "0x6400000"
bsp config ramfs_start_addr "0x90000000"
bsp config ramfs_size "0x6400000"
bsp config ramfs_start_addr "0x90400000"
bsp write
bsp reload
catch {bsp regenerate}
domain active {standalone_microblaze_0}
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
domain active {freertos10_xilinx_microblaze_0}
bsp reload
platform generate -domains 
bsp reload
bsp config sleep_timer "axi_timer_0"
bsp config en_interval_timer "true"
bsp write
bsp reload
catch {bsp regenerate}
bsp config sleep_timer "axi_timer_0"
bsp write
platform clean
bsp config sleep_timer "axi_timer_0"
bsp write
bsp config sleep_timer "none"
bsp write
bsp reload
catch {bsp regenerate}
bsp config sleep_timer "axi_timer_0"
bsp config sleep_timer "axi_timer_0"
bsp config en_interval_timer "true"
bsp write
bsp reload
catch {bsp regenerate}
bsp config interval_timer "none"
bsp config interval_timer "axi_timer_0"
bsp write
bsp reload
catch {bsp regenerate}
bsp config en_interval_timer "false"
bsp config interval_timer "none"
bsp write
bsp reload
catch {bsp regenerate}
bsp reload
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp config sleep_timer "none"
bsp config en_interval_timer "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp reload
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
domain active {standalone_microblaze_0}
domain active {freertos10_xilinx_microblaze_0}
bsp reload
bsp reload
bsp config udp_tx_blocking "false"
bsp reload
bsp config udp_tx_blocking "false"
bsp write
bsp reload
catch {bsp regenerate}
bsp write
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP.xsa}
platform generate -domains freertos10_xilinx_microblaze_0 
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-A/ZBSAR_TOP_A_bak.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains 
platform generate -domains 
platform generate
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate -domains 
platform generate -domains freertos10_xilinx_microblaze_0 
bsp reload
bsp config udp_tx_blocking "true"
bsp config n_rx_coalesce "8"
bsp config n_tx_coalesce "8"
bsp config tcp_snd_buf "65535"
bsp config tcp_wnd "262140"
bsp config tcp_snd_buf "262140"
bsp config tcp_maxrtx "12"
bsp config n_rx_coalesce "1"
bsp config n_tx_coalesce "1"
bsp config tcp_ip_tx_checksum_offload "false"
bsp config n_tx_coalesce "1"
bsp config tcp_rx_checksum_offload "false"
bsp config mem_size "0x100000"
bsp write
bsp reload
catch {bsp regenerate}
platform generate
bsp config n_rx_coalesce "8"
bsp config n_tx_coalesce "8"
bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform clean
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate -domains freertos10_xilinx_microblaze_0 
platform generate
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform clean
platform generate
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform generate
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform active {ZBSAR_TOP}
domain active {standalone_microblaze_0}
bsp reload
domain active {freertos10_xilinx_microblaze_0}
bsp reload
bsp config memp_n_udp_pcb "32"
bsp write
bsp reload
catch {bsp regenerate}
bsp config tcp_ip_rx_checksum_offload "true"
bsp write
bsp reload
catch {bsp regenerate}
bsp config tcp_ip_rx_checksum_offload "false"
bsp write
bsp reload
catch {bsp regenerate}
bsp config tcp_ip_rx_checksum_offload "false"
bsp config tcp_ip_rx_checksum_offload "false"
bsp config temac_use_jumbo_frames "false"
bsp config tcp_rx_checksum_offload "false"
bsp config tcp_ip_rx_checksum_offload "true"
bsp config tcp_ip_tx_checksum_offload "true"
bsp config tcp_rx_checksum_offload "true"
bsp config tcp_tx_checksum_offload "true"
bsp config temac_use_jumbo_frames "false"
bsp write
bsp reload
catch {bsp regenerate}
bsp config temac_use_jumbo_frames "true"
bsp config n_tx_coalesce "1"
bsp config n_rx_coalesce "1"
bsp write
bsp reload
catch {bsp regenerate}
bsp config tcp_ip_rx_checksum_offload "false"
bsp config tcp_ip_tx_checksum_offload "false"
bsp config tcp_rx_checksum_offload "false"
bsp config tcp_tx_checksum_offload "false"
bsp config tcp_tx_checksum_offload "false"
bsp config temac_use_jumbo_frames "false"
bsp config n_rx_coalesce "8"
bsp config n_tx_coalesce "8"
bsp write
bsp reload
catch {bsp regenerate}
bsp write
bsp write
platform generate
platform generate -domains freertos10_xilinx_microblaze_0 
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform active {ZBSAR_TOP}
bsp reload
bsp reload
platform generate -domains 
bsp reload
platform generate
platform active {ZBSAR_TOP}
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
bsp reload
bsp reload
platform generate
platform config -updatehw {D:/project/ZBSAR-C/ZBSAR_C.xsa}
platform generate
catch {platform remove project}
platform clean
platform generate
platform clean
platform clean
platform generate
platform clean
platform generate
platform clean
platform generate
platform clean
platform generate
platform generate -domains 
platform generate
platform active {ZBSAR_TOP}
bsp reload
bsp config stdin "axi_uartlite_0"
bsp reload
platform generate -domains 
platform clean
platform generate
