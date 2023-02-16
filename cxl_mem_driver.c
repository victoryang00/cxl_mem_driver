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
#include <linux/string.h>
#include<linux/mutex.h>
#include "cxl_mem_driver.h"
#include "bf.h"
MODULE_DEVICE_TABLE(pci, ids);

static int device_open(struct inode *ip, struct file *fp)
{
    /*
    可能有多个同类设备，驱动需要知道用户操作的是哪个设备；
    container_of()：根据结构体中某个成员的地址，从而获取到整个结构体的首地址；
    @ptr: 已知结构体成员的地址
    @type: 要获取的结构体的类型
    @member: 已知结构体成员的名字
    */
    struct cxl_mem *dev = container_of(ip->i_cdev, struct cxl_mem, cdev);
    fp->private_data = dev;
    return 0;
}
/**
 * 1.应用程序关闭设备文件时,需要将分配的内存置空闲,修改链表
 */
static int device_release(struct inode *ip, struct file *fp)
{   
    struct cxl_mem *p=fp->private_data;
    /*获得关闭设备文件的进程号*/
    int pid = current->pid;

    if(mutex_lock_interruptible(&p->mtx))   //-EINTR
		return -ERESTARTSYS;
    if(bf_recycle(ip, fp, pid)>=0){
        printk("已释放进程%d在设备%s上申请的内存！\n",pid,p->dev_name);
        display(p);
    }
    mutex_unlock(&p->mtx);

    return 0;
}

static int device_mmap(struct file *fp, struct vm_area_struct *vma)
{
    int pid = current->pid;
    struct cxl_mem *p;
    int ret = 0;
    p = fp->private_data;
    // vma->vm_flags |= (VM_IO | VM_LOCKED | VM_DONTEXPAND | VM_DONTDUMP);
    // linux 3.7.0开始内核不再支持vm_area_struct结构体中flag标志使用值 VM_RESERVED
    vma->vm_flags |= VM_IO | VM_SHARED | VM_DONTEXPAND | VM_DONTDUMP; // 保留内存区
    // vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    if(mutex_lock_interruptible(&p->mtx))   //-EINTR
		return -ERESTARTSYS;
    /*调用分配内存函数,返回分配的连续空间的首地址*/
    long offset=bf_allocate(fp, vma, pid);
    mutex_unlock(&p->mtx);

    printk("进程%d申请%dKB内存资源......\n", pid,(vma->vm_end - vma->vm_start)/KB);
    if(offset<0){
        printk("分配失败，无可用空间！\n");
        return offset;
    }
    else{
        resource_size_t phyAddr=offset* PAGE_SIZE + p->bar_infoP->base;
        ret = remap_pfn_range(vma,                           /* 映射虚拟内存空间 */
                          vma->vm_start,                 /* 映射虚拟内存空间起始地址 */
                          phyAddr >> PAGE_SHIFT,         /* 与物理内存对应的页帧号，物理地址右移12位 */
                          (vma->vm_end - vma->vm_start), /* 映射虚拟内存空间大小,页大小的整数倍 */
                          vma->vm_page_prot);            /* 保护属性 */
        printk("已经为进程%d在%s设备上分配内存,起始地址为0x%llx,起始设备内存块为%d\n", pid,p->dev_name,phyAddr,offset);
        display(p);
        return ret;            
    }
}

/*暂时没用应用场景*/
static long device_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
    case CXL_MEM_GET_INFO:
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static struct file_operations cxl_mem_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = device_ioctl,
    .open = device_open,       // 打开
    .mmap = device_mmap,       // 内存重映射操作
    .release = device_release, // 释放设备
};

static void init_linklist(struct cxl_mem *p)
{
    p->headP = kmalloc(sizeof(struct areaNode), GFP_KERNEL);
    p->tailP = kmalloc(sizeof(struct areaNode), GFP_KERNEL);

    p->headP->prior = NULL;
    p->headP->next = p->tailP;
    p->tailP->prior = p->headP;
    p->tailP->next = NULL;
    p->headP->area.pid = -1;
    p->headP->area.state = BUSY; // 首结点不会被使用，定义为占用状态防止分区合并失败
    p->tailP->area.offset = 0;
    // 以页为分配单位
    p->tailP->area.size = p->bar_infoP->len / PAGE_SIZE;
    p->tailP->area.pid = -1;
    p->tailP->area.state = FREE;
}

static int pci_device_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    printk("检测到第%d个cxl_mem设备：\n",cur_count);
    
    /**获取设备pci信息*/
    int i, result;
    struct cxl_mem *cxl_memP = kmalloc(sizeof(struct cxl_mem), GFP_KERNEL);
    /*设置设备名*/
    sprintf(cxl_memP->dev_name, "cxl_mem%d", cur_count);
    if (pci_enable_device(pdev))
    {
        printk("IO pci_enable_device()failed.\n");
        return -EIO;
    }
    cxl_memP->pci_dev = pdev;
    cxl_memP->irq = pdev->irq;
    /*动态申请空间存放bar信息*/
    struct bar_info *bar_infoP = kmalloc(sizeof(struct bar_info) , GFP_KERNEL);
    
    bar_infoP->base = pci_resource_start(pdev, BAR_POS);
    bar_infoP->len = pci_resource_end(pdev, BAR_POS) - bar_infoP->base + 1;
    bar_infoP->flags = pci_resource_flags(pdev, BAR_POS);
    printk("\t\t设备内存基地址: 0x%llx 设备内存大小: 0x%llx (%dMB)", bar_infoP->base, bar_infoP->len,bar_infoP->len/MB);
    printk("\t\t该设备内存空间是 io %s\n", (bar_infoP->flags & IORESOURCE_MEM) ? "mem" : "port");
    
    cxl_memP->bar_infoP = bar_infoP;
    cxl_memP->pci_dev = pdev;
    /* 对PCI区进行标记 ，标记该区域已经分配出去*/
    if (unlikely(pci_request_regions(pdev, cxl_memP->dev_name)))
    {
        printk("failed:pci_request_regions\n");
        return -EIO;
    }
    pci_set_drvdata(pdev, cxl_memP);

    /**对每个设备创建一个字符设备文件*/

    cxl_memP->devno = MKDEV(DEVICE_MAJOR, cur_count);
 
    /**1.注册设备号*/

    result = register_chrdev_region(cxl_memP->devno, 1, cxl_memP->dev_name);
    if (result < 0)
    {
        printk("register_chrdev_region fail\n");
        return result;
    }
    printk("\t\t已注册主设备号:%d,从设备号:%d\n",MAJOR(cxl_memP->devno),MINOR(cxl_memP->devno));
    /**2.建立cdev与 file_operations之间的连接*/

    cdev_init(&cxl_memP->cdev, &cxl_mem_ops);
    /**3.向系统添加一个cdev以完成注册*/

    result = cdev_add(&cxl_memP->cdev, cxl_memP->devno, 1);
    if (result < 0)
    {
        printk("cdev_add fail!\n");
        unregister_chrdev_region(cxl_memP->devno, 1);
        return result;
    }
    /* 4. 在/dev下创建设备节点 */

    cxl_memP->cxl_mem_class = class_create(THIS_MODULE, "cxl_mem_class");
    if (IS_ERR(cxl_memP->cxl_mem_class))
    {
        printk(KERN_ERR "class_create()failed\n");
        result = PTR_ERR(cxl_memP->cxl_mem_class);
        return result;
    }

    device_create(cxl_memP->cxl_mem_class, NULL, cxl_memP->devno, NULL, cxl_memP->dev_name);
    printk("\t\t已创建设备文件/dev/cxl_mem%d\n",cur_count);
    /* 5. 初始化设备文件内存分配链表 */

    init_linklist(cxl_memP);
    display(cxl_memP);
    /* 6. 初始化互斥量 */
    mutex_init(&cxl_memP->mtx);   
    cxl_memPs[cur_count] = cxl_memP;
    cur_count++;
    return result;
}
/*pci驱动取消注册、热插拔的设备移除时调用*/
static void pci_device_remove(struct pci_dev *pdev)
{
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    printk("pci设备驱动已移除！\n");
}

static struct pci_driver pci_driver = {
    .name = "cxl_mem driver",
    .id_table = ids,
    .probe = pci_device_probe,
    .remove = pci_device_remove,
};

static int __init cxl_mem_init(void)
{
    printk("cxl_mem驱动模块初始化完成\n");
    /* 1. 向内核注册pci驱动 */
    pci_register_driver(&pci_driver);
  
    return 0;
}
/*
1.注销设备号
2.注销pci driver
*/
static void __exit cxl_mem_exit(void)
{
    printk("驱动模块退出......\n");
    /*取消注册pci driver*/
    pci_unregister_driver(&pci_driver);
    int i;
    for (i = 0; i < cur_count; i++)
    {
        struct cxl_mem *p = cxl_memPs[i];
        device_destroy(p->cxl_mem_class, p->devno);
        class_destroy(p->cxl_mem_class);
        /*取消设备注册，释放设备号*/
        cdev_del(&p->cdev);
        unregister_chrdev_region(p->devno, 1);
        /*释放资源*/
        p->cxl_mem_class = NULL;
        kfree(p);
        p = NULL;
        printk("已释放cxl_mem%d设备资源......\n", i);
    }
    return;
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("cxl_mem driver");
module_init(cxl_mem_init);
module_exit(cxl_mem_exit);