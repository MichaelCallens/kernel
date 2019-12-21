#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h> 
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/interrupt.h> 

#include "query_ioctl.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static int time = 1, teller = 0;

static int 			 Input	=  27;
static struct timer_list blink_timer;
static long data=0;
static int 			 OutArray[2] 	= {4,17};

static struct gpio buttons[] = {
		{ 17, GPIOF_IN, "BUTTON 1" },	// turns LED on
};

/* Later on, the assigned IRQ numbers for the buttons are stored here */
static int button_irqs[] = { -1, -1 };

static void blink_timer_func(struct timer_list* t)
{
	int i = 0;
	printk(KERN_INFO "%s\n", __func__);
	
	data=!data; 
	for (i = 0; i < (sizeof OutArray / sizeof (int)); i++)
	{
		gpio_set_value(OutArray[i], data);
	}
	//gpio_set_value(LED1, data);
	
	
	/* schedule next execution */
	//blink_timer.data = !data;						// makes the LED toggle 
	blink_timer.expires = jiffies + (time*HZ); 		// 1 sec.
	add_timer(&blink_timer);
}

static irqreturn_t button_isr(int irq, void *data)
{
	if(irq == button_irqs[0]) {
			teller++;
	}
    printk(KERN_ERR "Aantal flanken: %d\n", teller);
	return IRQ_HANDLED;
}

/*
 * Module init function
 */
static int InitLeds_Buttons(void)
{
	int ret = 0;
    int i = 0;
	
	printk(KERN_INFO "%s\n", __func__);
	
	for (i = 0; i < (sizeof OutArray / sizeof (int)); i++)
	{
		// register, turn off 
		gpio_free(OutArray[i]);
		ret = gpio_request_one(OutArray[i], GPIOF_OUT_INIT_LOW, "led1");
	}
	
//	gpio_free(LED1);
//	ret = gpio_request_one(LED1, GPIOF_OUT_INIT_LOW, "led1");

	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
		return ret;
	}

	/* init timer, add timer function */
	//init_timer(&blink_timer);
	 timer_setup(&blink_timer, blink_timer_func, 0);

	blink_timer.function = blink_timer_func;
	//blink_timer.data = 1L;							// initially turn LED on
	blink_timer.expires = jiffies + (time*HZ); 		// 1 sec.
	add_timer(&blink_timer);

	// register BUTTON gpios
	buttons[0].gpio= Input;
	ret = gpio_request_array(buttons, ARRAY_SIZE(buttons));

	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs for BUTTONs: %d\n", ret);
		goto fail1;
	}

	printk(KERN_INFO "Current button1 value: %d\n", gpio_get_value(buttons[0].gpio));
	
	ret = gpio_to_irq(buttons[0].gpio);

	if(ret < 0) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail1;
	}

	button_irqs[0] = ret;

	printk(KERN_INFO "Successfully requested BUTTON1 IRQ # %d\n", button_irqs[0]);

	ret = request_irq(button_irqs[0], button_isr, IRQF_TRIGGER_RISING /*| IRQF_DISABLED*/, "gpiomod#button1", NULL);

	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail1;
	}

	return 0;

	// cleanup what has been setup so far

fail1:
	gpio_free(OutArray[0]);

	return ret;	
}

static void ExitLeds_Buttons(void)
{
	int i=0;
	printk(KERN_INFO "%s\n", __func__);

	// deactivate timer if running
	del_timer_sync(&blink_timer);

	for (i = 0; i < (sizeof OutArray / sizeof (int)); i++)
	{
		// turn LED off
		gpio_set_value(OutArray[i], 0); 
		
		// unregister GPIO 
		gpio_free(OutArray[i]);
	}

	// free irqs
	free_irq(button_irqs[0], NULL);
		
	// unregister
	gpio_free_array(buttons, ARRAY_SIZE(buttons));
}
 
static int my_open(struct inode *i, struct file *f)
{
    return 0;
}
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
    query_arg_t q;
 
    switch (cmd)
    {
        case QUERY_GET_VARIABLES:
            q.teller = teller;
            if (copy_to_user((query_arg_t *)arg, &q, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            break;
        case QUERY_CLR_VARIABLES:
            teller = 0;
            break;
        case QUERY_SET_VARIABLES:
            if (copy_from_user(&q, (query_arg_t *)arg, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            time = q.time;
            break;
        default:
            return -EINVAL;
    }
 
    return 0;
}
 
static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};
 
static int __init query_ioctl_init(void)
{
    int ret;
    struct device *dev_ret;
 
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "query_ioctl")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &query_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "query")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
    InitLeds_Buttons();
    return 0;
}
 
static void __exit query_ioctl_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
    ExitLeds_Buttons();
}
 
module_init(query_ioctl_init);
module_exit(query_ioctl_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email_at_sarika-pugs_dot_com>");
MODULE_DESCRIPTION("Query ioctl() Char Driver");
