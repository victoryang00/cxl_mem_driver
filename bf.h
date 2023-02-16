#ifndef _BF_H
#define _BF_H
#include "cxl_mem_driver.h"

static long bf_allocate(struct file *fp, struct vm_area_struct *vma, int pid)
{
    struct cxl_mem *cxl_memP = fp->private_data;
    /*请求的页/块数*/
    long reqSize = (vma->vm_end - vma->vm_start) / PAGE_SIZE;

    int surplus; // 记录可用内存与需求内存的差值
    struct areaNode *temp = kmalloc(sizeof(struct areaNode), GFP_KERNEL);
    struct areaNode *p = cxl_memP->headP->next;
    struct areaNode *q = NULL; // 记录最佳位置

    temp->area.size  = reqSize;
    temp->area.state = BUSY;
    temp->area.pid   = pid;

    while (p) // 遍历链表，找到第一个可用的空闲区间赋给q
    {
        if (p->area.state == FREE && p->area.size >= reqSize)
        {  
            q = p;
            surplus = p->area.size - reqSize;
            break;
        }
        p = p->next;
    }
    while (p) // 继续遍历，找到合适的位置
    {   
        if (p->area.state == FREE && p->area.size == reqSize) // 分区大小刚好是作业申请的大小
        {
            p->area.state = BUSY;
            p->area.pid = pid;
            return p->area.offset;
        }
        if (p->area.state == FREE && p->area.size > reqSize) // 可用内存与需求内存的差值更小
        {
            if (surplus > p->area.size - reqSize)
            {
                surplus = p->area.size - reqSize;
                q = p;
            }
        }
        p = p->next;
    }
    if (q == NULL)  return -ENOSPC;
    else // 找到最佳位置
    {
        // 将temp插入到结点q之前
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

static int bf_recycle(struct inode *ip, struct file *fp, int pid)
{
    struct cxl_mem *cxl_memP = fp->private_data;
    struct areaNode *p = cxl_memP->headP->next;
    int ret=-EIO;
    while (p)
    {
        if (p->area.pid == pid)
        {   
            ret=EIO;
            p->area.pid = 0;
            p->area.state = FREE;
            if (!p->prior->area.state && p->next->area.state) // 与前一个空闲区相邻，则合并
            {
                p->prior->area.size += p->area.size;
                p->prior->next = p->next;
                p->next->prior = p->prior;
            }
            if (!p->next->area.state && p->prior->area.state) // 与后一个空闲区相邻，则合并
            {
                p->area.size += p->next->area.size;
                if (p->next->next)
                {
                    p->next->next->prior = p;
                    p->next = p->next->next;
                }
                else
                    p->next = p->next->next;
            }
            if (!p->prior->area.state && !p->next->area.state) // 前后的空闲区均为空
            {
                p->prior->area.size += p->area.size + p->next->area.size;
                if (p->next->next)
                {
                    p->next->next->prior = p->prior;
                    p->prior->next = p->next->next;
                }
                else
                    p->prior->next = p->next->next;
            }
            
        }
        p = p->next;
    }
    return ret;
}

void display(struct cxl_mem *cxl_memP)
{
    printk("\t\t\t\t%s内存分配表\n", cxl_memP->dev_name);
    printk("\t\t----------------------------------------------------\n");
    struct areaNode *p = cxl_memP->headP->next;
    printk("\t\t进程pid\t\t起始块号\t分区大小\t状态");
    while (p)
    {
        printk("\t\t %d\t\t %d\t\t %d\t\t%s", p->area.pid, p->area.offset, p->area.size, p->area.state ? "已分配" : "空闲");
        p = p->next;
    }
    printk("\t\t----------------------------------------------------\n");
}

#endif