/***************************** 
*
*   驱动程序模板
*   版本：V1
*   使用方法(末行模式下)：
*   :%s/gpio_int/"你的驱动名称"/g
*
*******************************/

#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/crash_dump.h>
#include <linux/backing-dev.h>
#include <linux/bootmem.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/aio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>  //----- gpio_to_irq()
#include <linux/interrupt.h> //---request_irq()

#define  GPIO_PIN_NUM 17

unsigned int INT_STATUS=0; //---1 interrupt triggered, 0 no interrupt
unsigned int GPIO_INT_NUM=0; 
unsigned int DISABLE_IRQ_TOKEN;

volatile unsigned long *GPIO_CTRL_0;   //--- GPIO0 to GPIO31 direction control register  0-input 1-output
volatile unsigned long *GINT_REDGE_0;  //--GPIO0 to GPIO31 rising edge interrupt enable register
volatile unsigned long *GINT_FEDGE_0;  //--GPIO0 to GPIO31 falling edge interrupt enable register
volatile unsigned long *GINT_STAT_0;  //---GPIO0 to GPIO31 interrupt status register 1-int  0 -no int
volatile unsigned long *GINT_EDGE_0;  //---GPIO0 to GPIO31 interrupt edge status register 1-rising 0-falling
volatile unsigned long *GPIO1_MODE; // GPIO1 purpose selection register,for SPIS or GPIO14-17 mode selection
volatile unsigned long *AGPIO_CFG; // analog GPIO configuartion,GPIO14-17 purpose 



/****************  基本定义 **********************/
//内核空间缓冲区定义
#if 0
	#define KB_MAX_SIZE 20
	#define kbuf[KB_MAX_SIZE];
#endif


//加密函数参数内容： _IOW(IOW_CHAR , IOW_NUMn , IOW_TYPE)
//加密函数用于gpio_int_ioctl函数中
//使用举例：ioctl(fd , _IOW('L',0x80,long) , 0x1);
//#define NUMn gpio_int , if you need!
#define IOW_CHAR 'L'
#define IOW_TYPE  long
#define IOW_NUM1  0x80


//初始化函数必要资源定义
//用于初始化函数当中
//device number;
	dev_t dev_num;
//struct dev
	struct cdev gpio_int_cdev;
//auto "mknode /dev/gpio_int c dev_num minor_num"
struct class *gpio_int_class = NULL;
struct device *gpio_int_device = NULL;



/********************   Init GPIO  ************************/ 
static void Init_GPIO(void)
{
  //----------   map gpio register   --------
      GPIO1_MODE=(volatile unsigned long *)ioremap(0x10000060,4); // GPIO1 purpose selection register,for SPIS or GPIO14-17 mode selection
      //AGPIO_CFG; // use default value, analog GPIO configuartion,GPIO14-17 purpose 
      GPIO_CTRL_0=(volatile unsigned long *)ioremap(0x10000600,4);   //--- GPIO0 to GPIO31 direction control register  0-input 1-output
      GINT_REDGE_0=(volatile unsigned long *)ioremap(0x10000650,4);  //--GPIO0 to GPIO31 rising edge interrupt enable register
      GINT_FEDGE_0=(volatile unsigned long *)ioremap(0x10000660,4);  //--GPIO0-31, falling edge interrupt enable register
      GINT_STAT_0=(volatile unsigned long *)ioremap(0x10000690,4);  //---GPIO0 to GPIO31 interrupt status register 1-int  0 -no int
      //GINT_EDGE_0;  //---GPIO0 to GPIO31 interrupt edge status register 1-rising 0-falling

  //---------   set GPIO purpose  -----------
      *GPIO1_MODE |=(0x1<<2); //---set SPIS as GPIO14-17	
      *GPIO1_MODE &=~(0x1<<3);	
      *GPIO_CTRL_0 &=~(0x1<<GPIO_PIN_NUM); //---bit set 0, input mode
      *GINT_REDGE_0 |=(0x1<<GPIO_PIN_NUM); //--bit set 1,enable Rising Edge interrupt  
      *GINT_FEDGE_0 &=~(0x1<<GPIO_PIN_NUM); //--bit set 0,disable Falling Edge interrupt  
      
}

/********************   GPIO_unmap  ************************/ 
static void GPIO_unmap(void)
{
      iounmap(GPIO1_MODE);
      iounmap(GPIO_CTRL_0);
      iounmap(GINT_REDGE_0);
      iounmap(GINT_STAT_0);
}

/********************   Get GPIO Interrupt_Map Number    ******************/
static void get_gpio_INT_num(void)
{
        GPIO_INT_NUM=gpio_to_irq(GPIO_PIN_NUM);  
        if(GPIO_INT_NUM!=0)
            printk("GPIO%d get IRQ= %d successfully!\n",GPIO_PIN_NUM,GPIO_INT_NUM); 
        else
            printk("Get GPIO_INT_NUM failed!\n");
}

/********************   Interrupt Handler     ******************/
static irqreturn_t gpio_int_handler(int irq,void *dev_id,struct pt_regs *regs)
{
   printk("-------- midas_GPIO Interrupt triggered! ------\n");   
   if(((*GINT_STAT_0)>>GPIO_PIN_NUM)&0x1)  //-----confirm the Interrupt
   {
       printk("GPIO Interrupt confirmed!\n"); 
       INT_STATUS=1; //----set intterupt token
    }

   *GINT_STAT_0 &=~(0x1<<GPIO_PIN_NUM);  //----reset GPIO INT STAT register
   *GINT_REDGE_0 |=(0x1<<GPIO_PIN_NUM);  //----re-enable Rising Edge interrupt  
   
   disable_irq_nosync(GPIO_INT_NUM);
   DISABLE_IRQ_TOKEN=1;
}

/********************   Register GPIO Interrupt     ******************/
static int register_gpio_IRQ(void)
{
  int int_result;
     //int_result=request_irq(GPIO_INT_NUM,gpio_int_handler,IRQF_SHARED,"GPIO_INT_midas",(void *)&gpio_int_cdev);
     int_result=request_irq(GPIO_INT_NUM,gpio_int_handler,IRQF_DISABLED,"Midas-INT",NULL);
     if(int_result!=0)
       {
           printk("GPIO Interrupt request_irq fail!\n"); 
           return 1;
        }
      else
           return 0;
      printk("GPIO Interrupt request_irq success!\n"); 
}



/**************** 结构体 file_operations 成员函数 *****************/
//open
static int gpio_int_open(struct inode *inode, struct file *file)
{
        int ret_v=0;
	printk("gpio_int driver open........ \n");
    
        //------------ init GPIO interrupt ----------
        Init_GPIO();
        get_gpio_INT_num();
        ret_v=register_gpio_IRQ(); //---register GPIO interrupt
        
       
	return ret_v;
}

//close
static int gpio_int_close(struct inode *inode , struct file *file)
{
	printk("gpio_int drive close...\n");
        
        //------------free irq resource----------------------------
        GPIO_unmap();  //-----free GPIO map
        //free_irq(GPIO_INT_NUM,(void *)&gpio_int_cdev); //----free irq SHARED
        free_irq(GPIO_INT_NUM,NULL); //----free irq NON-SHARED

	return 0;
}

//read
static ssize_t gpio_int_read(struct file *file, char __user *buffer,
			size_t len, loff_t *pos)
{
	int ret_v = 0;

        //disable_irq(GPIO_INT_NUM);
	printk("gpio_int drive read...\n");
        copy_to_user(buffer,&INT_STATUS,4);
        INT_STATUS=0; //----reset interrupt token

        msleep(100); //---deter interrupt enabling to avoid key-jitter
        if(DISABLE_IRQ_TOKEN)
         {
           enable_irq(GPIO_INT_NUM);
           DISABLE_IRQ_TOKEN=0;
         }

        ret_v=4;
	return ret_v;
}

//write
static ssize_t gpio_int_write( struct file *file , const char __user *buffer,
			   size_t len , loff_t *offset )
{
	int ret_v = 0;
	printk("gpio_int drive write...\n");


	return ret_v;
}

//unlocked_ioctl
static int gpio_int_ioctl (struct file *filp , unsigned int cmd , unsigned long arg)
{
	int ret_v = 0;
	printk("gpio_int drive ioctl...\n");

	switch(cmd)
	{
		//常规：
		//cmd值自行进行修改

	   	case 1:
		break;


 		//带密码保护：
		//请在"基本定义"进行必要的定义
		case _IOW(IOW_CHAR,IOW_NUM1,IOW_TYPE):
		{
			if(arg == 0x1) //第二条件
			{
				
			}

		}
		break;

		default:
			break;
	}

	return ret_v;
}


/***************** 结构体： file_operations ************************/
//struct
static const struct file_operations gpio_int_fops = {
	.owner   = THIS_MODULE,
	.open	 = gpio_int_open,
	.release = gpio_int_close,	
	.read	 = gpio_int_read,
	.write   = gpio_int_write,
	.unlocked_ioctl	= gpio_int_ioctl,
};


/*************  functions: init , exit*******************/
//条件值变量，用于指示资源是否正常使用
unsigned char init_flag = 0;
unsigned char add_code_flag = 0;

//init
static __init int gpio_int_init(void)
{
	int ret_v = 0;
        int intc_result;

	printk("------- GPIO_INT Driver Init......\n");

	//函数alloc_chrdev_region主要参数说明：
	//参数2： 次设备号
	//参数3： 创建多少个设备
	if( ( ret_v = alloc_chrdev_region(&dev_num,0,1,"gpio_int_proc") ) < 0 )
	{
		goto dev_reg_error;
	}
	init_flag = 1; //标示设备创建成功；

	printk("The drive info of gpio_int:\nmajor: %d\nminor: %d\n",
		MAJOR(dev_num),MINOR(dev_num));

	cdev_init(&gpio_int_cdev,&gpio_int_fops);
	if( (ret_v = cdev_add(&gpio_int_cdev,dev_num,1)) != 0 )
	{
		goto cdev_add_error;
	}

	gpio_int_class = class_create(THIS_MODULE,"gpio_int_class");
	if( IS_ERR(gpio_int_class) )
	{
		goto class_c_error;
	}

	gpio_int_device = device_create(gpio_int_class,NULL,dev_num,NULL,"gpio_int_dev");
	if( IS_ERR(gpio_int_device) )
	{
		goto device_c_error;
	}
	printk("auto mknod success!\n");

	//------------   请在此添加您的初始化程序  --------------------------//
/*
          Init_GPIO();
          get_gpio_INT_num();
          ret_v=register_gpio_IRQ(); //---register GPIO interrupt
*/


        //如果需要做错误处理，请：goto gpio_int_error;	

	 add_code_flag = 1;
	//----------------------  END  ---------------------------// 

	goto init_success;

dev_reg_error:
	printk("alloc_chrdev_region failed\n");	
	return ret_v;

cdev_add_error:
	printk("cdev_add failed\n");
 	unregister_chrdev_region(dev_num, 1);
	init_flag = 0;
	return ret_v;

class_c_error:
	printk("class_create failed\n");
	cdev_del(&gpio_int_cdev);
 	unregister_chrdev_region(dev_num, 1);
	init_flag = 0;
	return PTR_ERR(gpio_int_class);

device_c_error:
	printk("device_create failed\n");
	cdev_del(&gpio_int_cdev);
 	unregister_chrdev_region(dev_num, 1);
	class_destroy(gpio_int_class);
	init_flag = 0;
	return PTR_ERR(gpio_int_device);

//------------------ 请在此添加您的错误处理内容 ----------------//
gpio_int_error:
         		
	add_code_flag = 0;
	return -1;
//--------------------          END         -------------------//
    
init_success:
	printk("gpio_int init success!\n");
	return 0;
}

//exit
static __exit void gpio_int_exit(void)
{
	printk("gpio_int drive exit...\n");	

	if(add_code_flag == 1)
 	{   
           //----------   请在这里释放您的程序占有的资源   ---------//
   	    printk("free your resources...\n");	               
/*    ------ following resource will release in CLOSE operation function----      
            GPIO_unmap();  //-----free GPIO map
            free_irq(GPIO_INT_NUM,(void *)&gpio_int_cdev); //----free irq
*/             
	    printk("free finish\n");		               
	    //----------------------     END      -------------------//
	}					            

	if(init_flag == 1)
	{
		//释放初始化使用到的资源;
		cdev_del(&gpio_int_cdev);
 		unregister_chrdev_region(dev_num, 1);
		device_unregister(gpio_int_device);
		class_destroy(gpio_int_class);
	}
}


/**************** module operations**********************/
//module loading
module_init(gpio_int_init);
module_exit(gpio_int_exit);

//some infomation
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("from Midas");
MODULE_DESCRIPTION("gpio_int drive");


/*********************  The End ***************************/
