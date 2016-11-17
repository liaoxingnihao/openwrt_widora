/***************************** 
*
*   驱动程序模板
*   版本：V1
*   使用方法(末行模式下)：
*   :%s/DHT11/"你的驱动名称"/g
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



//配置连接温度传感器的引脚
unsigned char PIN_NUM=40; //--GPIO defined in GPIO2_MODE, set bit Px_LED_AN_MODE, GPIO39-42. default 40
//for register GPIO2_MODE bif shift
unsigned char BIT_SHIFT;
unsigned char PIN_SET_TOKEN=0;//--1 when pins have prepared, 0 not prepare
//ioclt cmd parameter
#define SET_GPIO39 0
#define SET_GPIO40 1
#define SET_GPIO41 2
#define SET_GPIO42 3
//#define PIN_NUM 40  //--GPIO for DHT11 data pin 32 - 63
#define DHT11_L	   	        *GPIO_DATA_1 &= ~(1<<(PIN_NUM-32))  //低电平	
#define DHT11_H		        *GPIO_DATA_1 |=  (1<<(PIN_NUM-32))  //高电平
#define DHT11_OUT	        *GPIO_CTRL_1 |=  (1<<(PIN_NUM-32))  //输出	
#define DHT11_IN		*GPIO_CTRL_1 &= ~(1<<(PIN_NUM-32))  //输入	
#define DHT11_STA		(((*GPIO_DATA_1)>>(PIN_NUM-32)) & 0x01)  // get DHT11 data

//寄存器定义
volatile unsigned long *GPIO_DATA_1; //GPIO32-GPIO63 data register
volatile unsigned long *GPIO_CTRL_1; //GPIO32-GPIO63 direction control register
volatile unsigned long *GPIO2_MODE; // GPIO1 purpose selection register

/****************  基本定义 **********************/
//初始化函数必要资源定义
//用于初始化函数当中
//device number;
	dev_t dev_num;
//struct dev
	struct cdev DHT11_cdev;
//auto "mknode /dev/DHT11 c dev_num minor_num"
struct class *DHT11_class = NULL;
struct device *DHT11_device = NULL;

/********************  DHT11有关的函数   ****************************/
//----prepare pin and device
static void prepare_pin(void)
{
    //-------------- set PIN_NUM pin for  GPIO  purpose, default PIN_NUM=40 
       BIT_SHIFT=11-2*(PIN_NUM-39);   
      *GPIO2_MODE &=~(1<<BIT_SHIFT); //---set bit Px_LED_AN_MODE as 01 for GPIO
      *GPIO2_MODE |=(1<<(BIT_SHIFT-1));     
            
    //------------- init DHT11
      DHT11_OUT;
      DHT11_H;
    //-------------- pin-set token
      PIN_SET_TOKEN=1;
}



//从DHT11中读取一个字节
static unsigned char read_byte(void)
{
	unsigned char r_val = 0; 
	unsigned char t_count = 0; //计时器，防止超时；
        unsigned char i;

	for(i = 0 ; i < 8 ; i++)
	{
		t_count = 0;
		
		while(!DHT11_STA)
		{
			udelay(1);
			t_count++;
			if(t_count>60)
			{
				printk("read_byte error1\n");
				return 100;
			}
		}
		t_count = 0;

		udelay(35); //-------------------- original is 32

		if(DHT11_STA == 1)
		{
			r_val <<= 1;
			r_val |= 1;
		}
		else
		{
			r_val <<= 1;
			continue;
		}

		while( DHT11_STA == 1 )
		{
			udelay(2);
			t_count++;
			if(t_count>50) //------original 250
			{
				printk("read_byte error2\n");
				return 100;
			}
	 	 }
	}

	return r_val;
}

//从DHT11中读出数据
static unsigned int read_DHT11(void)
{
	 unsigned char t_count = 0; //计时器；
	 unsigned int  DHT11_DATA = 0;
	 unsigned char h_i = 0 , h_f = 0;
	 unsigned char t_i = 0 , t_f = 0;
	 unsigned char check_sum = 0;

	 DHT11_OUT;

	 DHT11_L;
	 mdelay(25); //>18ms;
	 DHT11_H;
	 udelay(30);

	 DHT11_IN;
	 while(DHT11_STA == 1)
	 {
	 	udelay(1);
		t_count++;

		if(t_count > 50)
		{
	 		printk("device error: DHT11!\n");
			return 0;
		}
	 }
	 t_count = 0;

 	 while(!DHT11_STA)
	 {
		udelay(1);
		t_count++;

		if(t_count > 250)
		{
			printk("read_DHT11 error1\n");
			return 0;
		}
	 }

	 t_count = 0;

	 udelay(50);

	 while(DHT11_STA)
	 {
		udelay(1);
		t_count++;
		if(t_count > 250)
		{
			printk("read_DHT11 error2\n");
			return 0;
		}
	 }

	 h_i = read_byte();
	 h_f = read_byte();
	 t_i = read_byte();
	 t_f = read_byte();
	 check_sum = read_byte();

	 if(check_sum == (h_i+h_f+t_i+t_f) || (h_i!=100 && t_i != 100))
	 {
/*
		DHT11_DATA = t_i;
		DHT11_DATA <<= 8;
		DHT11_DATA += h_i;
*/
		DHT11_DATA = t_i;
		DHT11_DATA <<= 8;
                DHT11_DATA +=t_f;
                DHT11_DATA <<= 8;
		DHT11_DATA += h_i;
                DHT11_DATA <<= 8;
                DHT11_DATA += h_f;
                printk("t_i=%0x h_i=%0x\n",t_i,h_i);
                printk("read DHT11 successfully!\n");
	 }
	 else
	 {
		DHT11_DATA = 0;
		printk("read_DHT11 error3\n");
	 }

	 return DHT11_DATA;
}


/**********************************************************************/

/**************** 结构体 file_operations 成员函数 *****************/
//open
static int DHT11_open(struct inode *inode, struct file *file)
{
	printk("DHT11 drive open...\n");

/*
	DHT11_OUT;
	DHT11_H;
*/
	return 0;
}

//close
static int DHT11_close(struct inode *inode , struct file *file)
{
	return 0;
}

//read
static ssize_t DHT11_read(struct file *file, char __user *buffer,
			size_t len, loff_t *pos)
{
	unsigned int DHT11_DATA; 
	printk("DHT11 drive read...\n");
        
        //-----prepare pin and device if necessary
        if(PIN_SET_TOKEN==0) 
               prepare_pin();  

	 DHT11_DATA = read_DHT11();
	 copy_to_user(buffer, &DHT11_DATA, 4);

	return 4;
}

//unlocked_ioctl
static int DHT11_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  int ret_v=0;
  printk("DHT11 drive ioctl...\n");

  switch(cmd)
  {
      case SET_GPIO39:
         PIN_NUM=39;
      break;
      
      case SET_GPIO40:
         PIN_NUM=40;
      break;     

      case SET_GPIO41:
         PIN_NUM=41;
      break;     

      case SET_GPIO42:
         PIN_NUM=42;
      break;     

      default:
         ret_v=1; //----PIN number error
         break;
    }

    PIN_SET_TOKEN=0; //----reset PIN_SET_TOKEN    
    return ret_v;
}


/***************** 结构体： file_operations ************************/
//struct
static const struct file_operations DHT11_fops = {
	.owner   = THIS_MODULE,
	.open	 = DHT11_open,
	.release = DHT11_close,	
	.read	 = DHT11_read,
        .unlocked_ioctl =DHT11_ioctl,
};


/*************  functions: init , exit*******************/
//条件值变量，用于指示资源是否正常使用
unsigned char init_flag = 0;
unsigned char add_code_flag = 0;

//init
static __init int DHT11_init(void)
{
	int ret_v = 0;
	printk("DHT11 drive init...\n");

	//函数alloc_chrdev_region主要参数说明：
	//参数2： 次设备号
	//参数3： 创建多少个设备 ????????
        //PARAMETER 4:
	if( ( ret_v = alloc_chrdev_region(&dev_num,0,1,"DHT11") ) < 0 )
	{
		goto dev_reg_error;
	}
	init_flag = 1; //标示设备创建成功；

	printk("The drive info of DHT11:\nmajor: %d\nminor: %d\n",
		MAJOR(dev_num),MINOR(dev_num));

	cdev_init(&DHT11_cdev,&DHT11_fops);
	if( (ret_v = cdev_add(&DHT11_cdev,dev_num,1)) != 0 )
	{
		goto cdev_add_error;
	}

	DHT11_class = class_create(THIS_MODULE,"DHT11");
	if( IS_ERR(DHT11_class) )
	{
		goto class_c_error;
	}

	DHT11_device = device_create(DHT11_class,NULL,dev_num,NULL,"DHT11");
	if( IS_ERR(DHT11_device) )
	{
		goto device_c_error;
	}
	printk("auto mknod success!\n");

	//------------   请在此添加您的初始化程序  --------------//

	GPIO2_MODE=(volatile unsigned long *)ioremap(0x10000064,4);
        GPIO_CTRL_1=(volatile unsigned long *)ioremap(0x10000604,4);
        GPIO_DATA_1=(volatile unsigned long *)ioremap(0x10000624,4);
         
  /*      
        //-------- set pin for  GPIO  purpose, default PIN_NUM=40 
         BIT_SHIFT=11-2*(PIN_NUM-39);   
        *GPIO2_MODE &=~(1<<BIT_SHIFT); //---set bit Px_LED_AN_MODE as 01 for GPIO
        *GPIO2_MODE |=(1<<(BIT_SHIFT-1)); 
  */
        
        //如果需要做错误处理，请：goto DHT11_error;	

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
	cdev_del(&DHT11_cdev);
 	unregister_chrdev_region(dev_num, 1);
	init_flag = 0;
	return PTR_ERR(DHT11_class);

device_c_error:
	printk("device_create failed\n");
	cdev_del(&DHT11_cdev);
 	unregister_chrdev_region(dev_num, 1);
	class_destroy(DHT11_class);
	init_flag = 0;
	return PTR_ERR(DHT11_device);

//------------------ 请在此添加您的错误处理内容 ----------------//
DHT11_error:
		



	add_code_flag = 0;
	return -1;
//--------------------          END         -------------------//
    
init_success:
	printk("DHT11 init success!\n");
	return 0;
}

//exit
static __exit void DHT11_exit(void)
{
	printk("DHT11 drive exit...\n");	

	if(add_code_flag == 1)
 	{   
           //----------   请在这里释放您的程序占有的资源   ---------//
	    printk("free your resources...\n");	               

		//iounmap(AGPIO_CFG);
		iounmap(GPIO2_MODE);
                iounmap(GPIO_CTRL_1);
                iounmap(GPIO_DATA_1);

	    printk("free finish\n");		               
	    //----------------------     END      -------------------//
	}					            

	if(init_flag == 1)
	{
		//释放初始化使用到的资源;
		cdev_del(&DHT11_cdev);
 		unregister_chrdev_region(dev_num, 1);
		device_unregister(DHT11_device);
		class_destroy(DHT11_class);
	}
}


/**************** module operations**********************/
//module loading
module_init(DHT11_init);
module_exit(DHT11_exit);

//some infomation
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("from Jafy");
MODULE_DESCRIPTION("DHT11 drive");



/*********************  The End ***************************/
