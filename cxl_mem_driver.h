#ifndef _CXL_MEM_DRIVER_H
#define _CXL_MEM_DRIVER_H

/*为设备分配的主从设备号*/
#define DEVICE_MAJOR               500
#define DEVICE_MINOR               0
#define DEVICE_NR                  1
#define MM_SIZE                    4096
/*设备的厂商号和设备号*/
#define CXL_MEM_VENDOR_ID   0x8086
#define CXL_MEM_DEVICE_ID   0x7890
/*定义最大设备数量*/
#define MAX_NRs             8
/*要读取的bar空间寄存器的位置*/
#define BAR_POS             2

/*定义连续内存区域状态*/
#define BUSY true
#define FREE false

#define KB 1024
#define MB (1024*KB) 

int cur_count=0;
struct cxl_mem* cxl_memPs[MAX_NRs] = {NULL};

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
struct area
{
    int pid;//分区号
    int size;//分区大小
    int offset;//偏移量/地址
    bool state;//使用状态，BUSY(true)为占用，FREE(false)为空闲
};

/**链表中的区域节点，包含前后向指针*/
struct areaNode
{
    struct area area;//数据域
    struct areaNode *prior;
    struct areaNode *next;
};

static struct pci_device_id ids[] = {
   {PCI_DEVICE(CXL_MEM_VENDOR_ID,CXL_MEM_DEVICE_ID) },
   {PCI_DEVICE(0x10ec,0x8852)},
   { 0,}  //最后一组是0，表示结束
};

struct bar_info
{
    //bar空间基地址 
   resource_size_t base;
    //bar空间大小和标志
   resource_size_t len;
   long flags;
};

struct cxl_mem
{   /*设备名称*/
    char dev_name[20];
    /*设备pci结构体*/
	struct pci_dev  *pci_dev;
    /*字符设备结构体*/
	struct 	cdev 	cdev;
    /*设备号（主+从）*/
	int    		    devno;
    struct class 	*cxl_mem_class;
    /*bar指针*/
    struct bar_info *bar_infoP;
    unsigned int irq;
    struct areaNode* headP;
    struct areaNode* tailP;
    // 互斥锁
    struct mutex mtx;
};

#endif