#ifndef _BF_H
#define _BF_H


static long bf_allocate(struct file *fp,struct vm_area_struct *vma,int pid)
{   
    struct cxl_mem *cxl_memP = fp->private_data;
    /*请求的页数*/
    long reqSize = (vma->vm_end-vma->vm_start)/PAGE_SHIFT;
   
    int surplus;//记录可用内存与需求内存的差值
    areaNodeP temp = (areaNodeP)kmalloc(sizeof(areaNode),GFP_KERNEL);
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

static long bf_recycle(struct inode* ip, struct file * fp,int pid)
{    
    struct cxl_mem *cxl_memP = fp->private_data;
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
            printk("释放内存成功！\n");
            break;
        }
        p = p->next;
        if(!p)
            printk("内存中没有该需要释放内存的作业！");
    }
    return 0;
}

#endif