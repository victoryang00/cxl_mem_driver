#ifndef _CXL_MEM_DRIVER_H
#define _CXL_MEM_DRIVER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <asm-generic/ioctl.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/interrupt.h> 
#include <linux/uaccess.h> 
#include <asm/current.h> 

/*为设备分配的主从设备号*/
#define DEVICE_MAJOR               500
#define DEVICE_MINOR                0
#define DEVICE_NR                   1
#define MM_SIZE                     4096
/*假设设备的厂商号和设备号*/
#define CXL_MEM_VENDOR_ID   0x666
#define CXL_MEM_DEVICE_ID   0x999
/*定义最大设备数量*/
#define MAX_NRs             8
/*假设设备使用两个bar寄存器，一个映射内存寄存器，一个映射存储空间*/
#define BAR_NRs              2
/*定义设备名称*/
#define DEV_NAME			"cxl_mem"
/*定义连续内存区域状态*/
#define BUSY true
#define FREE false
static int cur_count=0;
static struct cxl_mem* cxl_memPs[MAX_NRs];

/**ioctlCmd组成: 
 *设备类型8bit|序列号8bit|方向2bit 用户<->内核|数据尺寸8-14bit 
 * *
/

/*定义幻数，代表一类设备*/
#define CXL_MEM_TYPE '0xF7'
/* 定义具体的ioctl指令*/

/* 1.获取当前资源信息*/
#define CXL_MEM_GET_INFO _IO(CXL_MEM_TYPE,0)

/**内存区域结构体*/
typedef struct area
{
    int pid;//分区号
    int size;//分区大小
    int offset;//偏移量/地址
    bool state;//使用状态，BUSY(true)为占用，FREE(false)为空闲
} area;

/**链表中的区域节点，包含前后向指针*/
typedef struct areaNode
{
    area area;//数据域
    struct areaNode *prior;
    struct areaNode *next;
} areaNode, *areaNodeP;

static struct pci_device_id ids[] = {
   {PCI_DEVICE(CXL_MEM_VENDOR_ID,CXL_MEM_DEVICE_ID) },
   { 0,}  //最后一组是0，表示结束
};

struct bar_info
{
    //bar空间基地址 
   resource_size_t base;
    //bar空间大小和标志
   long len,flags;
};

struct cxl_mem
{
	struct pci_dev  *pci_dev;
    // 内核中的字符设备结构体，包含operations和设备号
	struct 	cdev 	cdev;
	dev_t			devno;
    struct class 	*cxl_mem_class;
    struct bar_info *bar_infoP;
    char			*mem_buf;
    int irq;
    areaNodeP headP;
    areaNodeP tailP;
};

#endif