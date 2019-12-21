/*
 * Basic kernel module using a timer and GPIOs to flash a LED.
 *
 * Author:
 * 	Stefan Wendler (devnull@kaltpost.de)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>	
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h> 

//#define LED1	4
static int teller=0;
static struct timer_list blink_timer;
static long data=0;

/*
 * The module commandline arguments ...
 */

static short int 	 time 		= 1;
static int 			 Input	=  27;
static int 			 OutArray[2] 	= {4,17};
static int 			 arr_argc 		= 0;

static struct gpio buttons[] = {
		{ 17, GPIOF_IN, "BUTTON 1" },	// turns LED on
};

/* Later on, the assigned IRQ numbers for the buttons are stored here */
static int button_irqs[] = { -1, -1 };

static irqreturn_t button_isr(int irq, void *data)
{
	if(irq == button_irqs[0]) {
			teller++;
	}
	else if(irq == button_irqs[1]) {
			//gpio_set_value(leds[0].gpio, 0);
	}
    printk(KERN_ERR "Aantal flanken: %d\n", teller);
	return IRQ_HANDLED;
}

module_param(time, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(time, "time toggle (sec)");
MODULE_PARM_DESC(Input, "Input");
module_param_array(OutArray, int, &arr_argc, 0000);
MODULE_PARM_DESC(OutArray, "IO numbers");

/*
 * Module init function
 */

/*
 * Timer function called periodically
 */
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

/*
 * Module init function
 */
static int __init gpiomod_init(void)
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
fail3:
	free_irq(button_irqs[0], NULL);

fail2: 
	gpio_free_array(buttons, ARRAY_SIZE(OutArray));

fail1:
	gpio_free(OutArray[0]);

	return ret;	
}

/*
 * Module exit function
 */
static void __exit gpiomod_exit(void)
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Callens");
MODULE_DESCRIPTION("Basic kernel module using a timer and GPIOs to flash a LED.");

module_init(gpiomod_init);
module_exit(gpiomod_exit);
