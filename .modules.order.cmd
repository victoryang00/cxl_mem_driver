cmd_/home/zjt/myproject/cxl_driver/modules.order := {   echo /home/zjt/myproject/cxl_driver/cxl_mem_driver.ko; :; } | awk '!x[$$0]++' - > /home/zjt/myproject/cxl_driver/modules.order
