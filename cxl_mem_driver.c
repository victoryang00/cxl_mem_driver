#include "cxl_mem_driver.h"

static void init_linklist(struct cxl_mem* p )
{
    p->headP=(areaNodeP)kmalloc(sizeof(areaNode));
    p->tailP=(areaNodeP)kmalloc(sizeof(areaNode));
    p->headP->prior = NULL;
    p->headP->next = p->tailP;

    p->tailP->prior = p->headP;
    p->tailP->next = NULL;

    p->headP->area.ID = 0;
    p->headP->area.state = BUSY; //首结点不会被使用，定义为占用状态防止分区合并失败

    p->tailP->area.offset = 0;
    // 以页为分配单位
    p->tailP->area.size = p->bar_infoP[0].len/PAGE_SIZE-1;
    p->tailP->area.ID = 0;
    p->tailP->area.state = FREE;
}
static int pci_device_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{   
    /**对每个设备创建一个字符设备文件*/

    int i,result;
    struct 	cxl_mem *cxl_memP = NULL;
    cxl_memP = kmalloc(sizeof(struct cxl_mem), GFP_KERNEL);
    cxl_memP->devno=MKDEV(DEVICE_MAJOR,cur_count);
    /**1.注册设备号*/
    result=register_chrdev_region(cxl_memP->devno,1,DEV_NAME);
    if(result<0)
    {
        printk("register_chrdev_region fail\n");
        return result;
    }
    /**2.建立cdev与 file_operations之间的连接*/
	cdev_init(&xl_memP->cdev,&cxl_mem_ops);
    /**3.向系统添加一个cdev以完成注册*/
    result = cdev_add(&cxl_memP->cdev, cxl_memP->devno, 1);
    if(result<0)
    {
        printk("cdev_add fail\n");
        unregister_chrdev_region(cxl_memP->devno,DEVICE_NR);
        return result;
    }
    /* 4. 在/dev下创建设备节点 */
    cxl_memP->cxl_mem_class=class_create(THIS_MODULE,"cxl_mem_class");
    if(IS_ERR(cxl_memP->cxl_mem_class))
    {
        printk(KERN_ERR"class_create()failed\n");
        result=PTR_ERR(cxl_memP->cxl_mem_class);
        return result;
    }
    device_create(cxl_memP->cxl_mem_class, NULL, cxl_memP->devno, NULL, DEV_NAME); 
    /* 4. 初始化设备文件内存分配链表 */
    init_linklist(cxl_memP);


    /**获取设备pci信息*/
    struct bar_info *bar_infoP;
    //使能pci设备
	if (pci_enable_device(pdev)){
        printk (KERN_ERR "IO pci_enable_device()failed.\n");
        return -EIO;
	}
    cxl_memP->pci_dev = pdev;
    cxl_memP->irq = pdev->irq;
    // 动态申请空间存放bar信息
    bar_infoP=( struct bar_info *)kmalloc(sizeof(struct bar_info)*bar_nr,GFP_KERNEL);
    for(i=0;i<bar_nr;i++)
    {
        bar_infoP[i].base =  pci_resource_start(pdev, i);
        bar_infoP[i].len = pci_resource_end(pdev, i) - bar_infoP[i].base + 1;
        bar_infoP[i].flags = pci_resource_flags(pdev,i);
        printk("start %llx %lx %lx\n",bar_infoP[i].base,bar_infoP[i].len,bar_infoP[i].flags);
        printk("PCI base addr 0 is io%s.\n",(bar_infoP[i].flags & IORESOURCE_MEM)? "mem":"port");
    }
    cxl_memP->bar_infoP=bar_infoP;
    cxl_memP->pci_dev=pdev;
    /* 对PCI区进行标记 ，标记该区域已经分配出去*/
    if(unlikely(pci_request_regions(pdev,DEV_NAME))){
		printk("failed:pci_request_regions\n");
		return  -EIO;
	}
    pci_set_drvarea(pdev,cxl_memP);

    cxl_memPs[cur_count]=cxl_memP;
    cur_count++;
    return result;
}

static void pci_device_remove(struct pci_dev *pdev)
{   
    pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static struct pci_driver pci_driver = {
	.name = "cxl_mem driver",
	.id_table = ids,
    // 设备和驱动匹配时调用probe
	.probe = pci_device_probe,
    // 移除调用
	.remove = pci_device_remove,
};


static long bf_allocate(struct file *fp,struct vm_area_struct *vma,int pid)
{   
    /*请求的页数*/
    long reqSize = (vma->vm_end-vma->vm_start)/PAGE_SHIFT;
    // struct cxl_mem *cxl_memP = fp->private_area;
    int surplus;//记录可用内存与需求内存的差值
    areaNodeP temp = (areaNodeP)kmalloc(sizeof(areaNode));
    areaNode *p = cxl_memP->headP->next;
    areaNode *q = NULL;//记录最佳位置

    temp->area.size = reqSize;
    temp->area.state = BUSY;
    temp->area.pid=pid;

    while(p)//遍历链表，找到第一个可用的空闲区间赋给q
    {
        if (p->area.state==FREE && p->area.size >= reqSize)
        {
            q = p;
            surplus = p->area.size - reqSize;
            break;
        }
        p=p->next;
    }
    while(p)//继续遍历，找到合适的位置
    {
        if (p->area.state == FREE && p->area.size == reqSize) //分区大小刚好是作业申请的大小
        {
            p->area.state = BUSY;
            p->area.pid = pid;
            return p->area.offset;
        }
        if (p->area.state == FREE && p->area.size > reqSize) //可用内存与需求内存的差值更小
        {
            if (surplus > p->area.size - reqSize)
            {
                surplus = p->area.size-reqSize;
                q = p;
            }
        }
        p=p->next;
    }
    if (q == NULL)//没有找到位置
        return 0;
    else//找到最佳位置
    {
        //将temp插入到结点q之前
        temp->next = q;
        temp->prior = q->prior;
        temp->area.offset = q->area.offset;

        q->prior->next = temp;
        q->prior = temp;

        q->area.size = surplus;
        q->area.offset += reqSize;
        return temp->area.offset;
    }
  
}

static long bf_recycle(struct innode * ip, struct file * fp,int pid)
{     
    areaNode *p =cxl_memP->headP->next; 
    while (p)
    {
        if (p->area.pid == pid)
        {
            p->area.pid = 0;
            p->area.state = FREE;
            if (!p->prior->area.state && p->next->area.state)//与前一个空闲区相邻，则合并
            {
                p->prior->area.size += p->area.size;
                p->prior->next = p->next;
                p->next->prior = p->prior;
            }
            if (!p->next->area.state && p->prior->area.state) //与后一个空闲区相邻，则合并
            {
                p->area.size += p->next->area.size;
                if(p->next->next)
                {
                    p->next->next->prior=p;
                    p->next = p->next->next;
                }
                else
                    p->next = p->next->next;
            }
            if(!p->prior->area.state && !p->next->area.state) //前后的空闲区均为空
            {
                p->prior->area.size += p->area.size + p->next->area.size;
                if(p->next->next)
                {
                    p->next->next->prior = p->prior;
                    p->prior->next = p->next->next;
                }
                else
                    p->prior->next = p->next->next;
            }
            printf("释放内存成功！\n");
            break;
        }
        p = p->next;
        if(!p)
            printf("内存中没有该需要释放内存的作业！");
    }
}

static int device_mmap (struct file *fp, struct vm_area_struct *vma)
{   int pid=current->pid;
    struct 	cxl_mem *p;
	int ret = 0;
	p = fp->private_area;
	// vma->vm_flags |= (VM_IO | VM_LOCKED | VM_DONTEXPAND | VM_DONTDUMP);
    // linux 3.7.0开始内核不再支持vm_area_struct结构体中flag标志使用值 VM_RESERVED
    vma->vm_flags |= VM_IO | VM_SHARED| VM_DONTEXPAND | VM_DONTDUMP; //保留内存区 
    // vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    /*调用分配内存函数,返回分配的连续空间的首地址*/ 
    unsigned long phyAddr = bf_allocate(fp,vma,pid)*PAGE_SIZE+p->bar_infoP[0].base;
    ret = remap_pfn_range(vma,		/* 映射虚拟内存空间 */  
                       vma->vm_start,/* 映射虚拟内存空间起始地址 */   
                       phyAddr>>PAGE_SHIFT,/* 与物理内存对应的页帧号，物理地址右移12位 */
                       (vma->vm_end - vma->vm_start),/* 映射虚拟内存空间大小,页大小的整数倍 */  
                       vma->vm_page_prot);/* 保护属性 */  
    return ret;
}
static int device_open (struct inode * ip, struct file * fp)
{
    /*
    可能有多个同类设备，驱动需要知道用户操作的是哪个设备；
    container_of()：根据结构体中某个成员的地址，从而获取到整个结构体的首地址；
    @ptr: 已知结构体成员的地址
    @type: 要获取的结构体的类型
    @member: 已知结构体成员的名字
    */
    struct cxl_mem * dev = container_of(ip->i_cdev, struct cxl_mem, cdev);
   
    fp->private_area = dev;
    if(cxl_memP == NULL)
    {
		printk("cxl_memP init fail!.\n");
		return -EFAULT;
	}
	// cxl_memP->mem_buf = (char *)kmalloc(MM_SIZE, GFP_KERNEL);
    // strcpy(cxl_memP->mem_buf,"1234567890");
    // printk("缓冲区内容：%s\n",cxl_memP->mem_buf);
    return 0;
}
/**
 * 1.应用程序关闭设备文件时,需要将分配的内存置空闲,修改链表
*/
static int device_release (struct innode * ip, struct file * fp)
{   
    /*获得关闭设备文件的进程号*/
    int pid=current->pid;
    bf_recycle(ip,fp,pid);
    return 0;
}

static long device_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{   
    switch (cmd)
    {
    case CXL_MEM_GET_SIZE:
        printk("请求空间\n");
        break;
 
    default:
        return -EINVAL;
    }
	return 0;
}

static struct file_operations cxl_mem_ops = {
	.owner   		=  THIS_MODULE,    
	.unlocked_ioctl =  device_ioctl,
    .open = device_open,// 打开
    .mmap = device_mmap,//内存重映射操作
    .release = device_release,// 释放设备
};

static int __init cxl_mem_init(void)
{   
    printk("cxl_mem_init\n");
    /* 1. 向内核注册pci驱动 */
	pci_register_driver(&pci_driver);
    /*为缓冲区分配内存*/
    cxl_memP->mem_buf = (char *)kmalloc(MM_SIZE, GFP_KERNEL);
    return 0;
}
/*
1.注销设备号
2.注销pci driver
*/
static void __exit cxl_mem_exit(void)
{   
    printk("cxl_mem_exit!\n");
    int i;
    for(i=0;i<cur_count;i++)
    {   
        struct cxl_mem* p = cxl_memPs[i];
        device_destroy(p->cxl_mem_class,p->devno);
        class_destroy(p->cxl_mem_class);
        /*取消设备注册，释放设备号*/
        cdev_del(&p->cdev);
        unregister_chrdev_region(p->devno,DEVICE_NR);
        /*取消注册pci driver*/
        pci_unregister_driver(&pci_driver);
        /*释放资源*/
        kfalse(p->mem_buf);
        p->cxl_mem_class = NULL;
        p->mem_buf = NULL;
        kfalse(p);
        p = NULL;
    }
    return;
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("cxl_mem driver");
module_init(cxl_mem_init);
module_exit(cxl_mem_exit);