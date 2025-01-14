/*
 * leds-aw22xxx.c   aw22xxx led module
 *
 * Version: v1.1.3
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include <linux/leds.h>
#include "leds-aw22xxx.h"
#include "leds-aw22xxx-reg.h"




/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW22XXX_I2C_NAME "aw22xxx_led"

#define AW22XXX_VERSION "v1.1.3"

#define AW22_LOG(fmt, args...) printk(KERN_ERR "[aw22xx] [%s: %d] "fmt,  __func__, __LINE__, ##args)
#ifdef NUBIA_MULTICOLOR_LED
#define AW22XXX_PINCTRL_STATE_ACTIVE "aw22xxx_led_default"
#define AW22XXX_PINCTRL_STATE_SUSPEND "aw22xxx_led_sleep"
#endif

#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 1
#define AW_READ_CHIPID_RETRIES 2
#define AW_READ_CHIPID_RETRY_DELAY 1
//add by jones 659S
int multicolor_led = 0;

/******************************************************
 *
 * aw22xxx led parameter
 *
 ******************************************************/
#define AW22XXX_CFG_NAME_MAX        64
#define AW22XXX_CFG_CUSTOM_MAX      0x20
#define AW22XXX_CFG_NUM_MAX         255

#define LED_EFFECT_NOINIT_STR       "noinit"

#define FW_NAME_DEFAULT             "aw22xxx_fw.bin"
#define FW_NAME_NX669J_V1E          "aw22xxx_fw_nx669j_v1e.bin"
static char aw22xxx_fw_name[AW22XXX_CFG_NAME_MAX] = {0};

static char aw22xxx_cfg_name[AW22XXX_CFG_NUM_MAX][AW22XXX_CFG_NAME_MAX] = {
	{"m_led_off.bin"},
	{"fan_led_on.bin"},
	{"fan_led_off.bin"},
	{"factory_test.bin"},
	{"left_red_on_noinit.bin"},
	{"left_red_off_noinit.bin"},
	{"right_blue_on_noinit.bin"},
	{"right_blue_off_noinit.bin"},
    /*0x50-0x58:aw_cfg_a0.bin-aw_cfg_a8.bin*/
	/*0x60-0x68:aw_cfg_b0.bin-aw_cfg_b8.bin*/
	/*0x70-0x78:aw_cfg_c0.bin-aw_cfg_c8.bin*/
	/*0x80-0x88:aw_cfg_d0.bin-aw_cfg_d8.bin*/
	/*0x90-0x98:aw_cfg_e0.bin-aw_cfg_e8.bin*/
	/*0xa0-0xa8:aw_cfg_f0.bin-aw_cfg_f8.bin*/
};

#define AW22XXX_IMAX_NAME_MAX       32
static char aw22xxx_imax_name[][AW22XXX_IMAX_NAME_MAX] = {
    {"AW22XXX_IMAX_2mA"},
    {"AW22XXX_IMAX_3mA"},
    {"AW22XXX_IMAX_4mA"},
    {"AW22XXX_IMAX_6mA"},
    {"AW22XXX_IMAX_9mA"},
    {"AW22XXX_IMAX_10mA"},
    {"AW22XXX_IMAX_15mA"},
    {"AW22XXX_IMAX_20mA"},
    {"AW22XXX_IMAX_30mA"},
    {"AW22XXX_IMAX_40mA"},
    {"AW22XXX_IMAX_45mA"},
    {"AW22XXX_IMAX_60mA"},
    {"AW22XXX_IMAX_75mA"},
};
static char aw22xxx_imax_code[] = {
    AW22XXX_IMAX_2mA,
    AW22XXX_IMAX_3mA,
    AW22XXX_IMAX_4mA,
    AW22XXX_IMAX_6mA,
    AW22XXX_IMAX_9mA,
    AW22XXX_IMAX_10mA,
    AW22XXX_IMAX_15mA,
    AW22XXX_IMAX_20mA,
    AW22XXX_IMAX_30mA,
    AW22XXX_IMAX_40mA,
    AW22XXX_IMAX_45mA,
    AW22XXX_IMAX_60mA,
    AW22XXX_IMAX_75mA,
};

#define CMD_LINE_NUMS	1000
#define CMD_LINE_LEN	25
static unsigned  char user_para_data[CMD_LINE_NUMS*CMD_LINE_LEN] = {0};
static unsigned int write_idx = 0,read_idx = 0;
static unsigned char funcmp_flag = 0;
struct task_struct *cfg_update_kthread;
static unsigned char kthread_status = 0;
static unsigned char duration = 0;
static unsigned char lamp_init[] = {
	0xFF,0x00,
	0x02,0x01,
	0x0C,0x00,
	0x0B,0x05,
	0x05,0x01,
	0x04,0x01,
	0x09,0x01,
	0x04,0x03,
	0x05,0x41,
};

static unsigned char aw22xxx_blink_cfg[] = {
0xFF,0x00,
0x0C,0x00,

0x05,0xA0,
0x06,0xFF,
0x04,0x07,

0x05,0xA1,
0x06,0x00,
0x04,0x07,

0x05,0xA2,
0x06,0x00,
0x04,0x07,

0x05,0xA3,
0x06,0x00,
0x04,0x07,

0x05,0xA4,
0x06,0xFF,
0x04,0x07,

0x05,0xA5,
0x06,0x00,
0x04,0x07,

0x05,0xA6,
0x06,0x00,
0x04,0x07,

0x05,0xA7,
0x06,0x00,
0x04,0x07,

0x05,0xA8,
0x06,0xFF,
0x04,0x07,

0x05,0xA9,
0x06,0xFF,
0x04,0x07,

0x05,0xAA,
0x06,0xFF,
0x04,0x07,

0x05,0xAB,
0x06,0xFF,
0x04,0x07,

0x05,0xC0,
0x06,0xff,
0x04,0x07,

0x05,0xC1,
0x06,0xff,
0x04,0x07,

0x05,0xC2,
0x06,0xff,
0x04,0x07,

0x05,0xC3,
0x06,0xff,
0x04,0x07,

0x05,0xC4,
0x06,0xff,
0x04,0x07,

0x05,0xC5,
0x06,0xff,
0x04,0x07,

0x05,0xC6,
0x06,0xff,
0x04,0x07,

0x05,0xC7,
0x06,0xff,
0x04,0x07,

0x05,0xC8,
0x06,0xff,
0x04,0x07,

0x05,0xC9,
0x06,0xff,
0x04,0x07,

0x05,0xCA,
0x06,0xff,
0x04,0x07,

0x05,0xCB,
0x06,0xff,
0x04,0x07,
};

static unsigned char aw22xxx_led_off_cfg[] = {
	0xff,0x00,//page0
	0x05,0x00,//task0
	0x04,0x01,//mcuctr
	0x02,0x00,//gcr
};
/******************************************************
 *
 * aw22xxx i2c write/read
 *
 ******************************************************/
static int aw22xxx_i2c_write(struct aw22xxx *aw22xxx,
         unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;
	unsigned char data[2];

	data[0] = reg_addr;
	data[1] = reg_data;
    while(cnt < AW_I2C_RETRIES) {
		ret = i2c_master_send(aw22xxx->i2c, data, 2);
        //ret = i2c_smbus_write_byte_data(aw22xxx->i2c, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw22xxx_i2c_read(struct aw22xxx *aw22xxx,
        unsigned char reg_addr, unsigned char *reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
		ret = i2c_master_send(aw22xxx->i2c, &reg_addr, 1);
		if(ret < 0){
			pr_err("%s: i2c_read send cmd error=%d\n", __func__, ret);
		}
		ret = i2c_master_recv(aw22xxx->i2c, reg_data, 1);
		if(ret < 0){
			*reg_data = ret;
			pr_err("%s: i2c_read recive data error=%d\n", __func__, ret);
		}else{
			break;
		}
        //ret = i2c_smbus_read_byte_data(aw22xxx->i2c, reg_addr);
        /*if(ret < 0) {
            pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            *reg_data = ret;
            break;
        }*/
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw22xxx_i2c_write_bits(struct aw22xxx *aw22xxx,
         unsigned char reg_addr, unsigned char mask, unsigned char reg_data)
{
    unsigned char reg_val;

    aw22xxx_i2c_read(aw22xxx, reg_addr, &reg_val);
    reg_val &= mask;
    reg_val |= reg_data;
    aw22xxx_i2c_write(aw22xxx, reg_addr, reg_val);

    return 0;
}

#ifdef AW22XXX_FLASH_I2C_WRITES
static int aw22xxx_i2c_writes(struct aw22xxx *aw22xxx,
        unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
    int ret = -1;
    unsigned char *data;

    data = kmalloc(len+1, GFP_KERNEL);
    if (data == NULL) {
        pr_err("%s: can not allocate memory\n", __func__);
        return  -ENOMEM;
    }

    data[0] = reg_addr;
    memcpy(&data[1], buf, len);

    ret = i2c_master_send(aw22xxx->i2c, data, len+1);
    if (ret < 0) {
        pr_err("%s: i2c master send error\n", __func__);
    }

    kfree(data);

    return ret;
}
#endif

/*****************************************************
 *
 * aw22xxx led cfg
 *
 *****************************************************/
static int aw22xxx_reg_page_cfg(struct aw22xxx *aw22xxx, unsigned char page)
{
    aw22xxx_i2c_write(aw22xxx, REG_PAGE, page);
    return 0;
}

static int aw22xxx_sw_reset(struct aw22xxx *aw22xxx)
{
    aw22xxx_i2c_write(aw22xxx, REG_SRST, AW22XXX_SRSTW);
    msleep(2);
    return 0;
}

static int aw22xxx_chip_enable(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_GCR,
                BIT_GCR_CHIPEN_MASK, BIT_GCR_CHIPEN_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_GCR,
                BIT_GCR_CHIPEN_MASK, BIT_GCR_CHIPEN_DISABLE);
    }
    msleep(2);
    return 0;
}


/***********************************add by wyl 20201202****************************/
static void aw22xxx_update_cfg_array(struct aw22xxx *aw22xxx, unsigned char *p_cfg_data, unsigned int cfg_size)
{
    unsigned int i = 0;

    for(i=0; i<cfg_size; i+=2) {
        aw22xxx_i2c_write(aw22xxx, p_cfg_data[i], p_cfg_data[i+1]);
    }
}

static int  aw22xxx_blink_cfg_update_array(struct aw22xxx *aw22xxx)
{
    //pr_info("enter--%s---%d--\n",__func__,__LINE__);

    aw22xxx_update_cfg_array(aw22xxx, aw22xxx_blink_cfg, sizeof(aw22xxx_blink_cfg)/sizeof(unsigned char));
   // pr_info("exit--%s---%d--\n",__func__,__LINE__);
    return 0;
}
static int  aw22xxx_init_cfg_update_array(struct aw22xxx *aw22xxx)
{
    pr_info("enter--%s---%d--\n",__func__,__LINE__);
    aw22xxx_update_cfg_array(aw22xxx, lamp_init, sizeof(lamp_init)/sizeof(unsigned char));
   // pr_info("exit--%s---%d--\n",__func__,__LINE__);
    return 0;
}
static int  aw22xxx_cfg_led_off(struct aw22xxx *aw22xxx)
{
    //pr_info("--%s---%d--\n",__func__,__LINE__);

    aw22xxx_update_cfg_array(aw22xxx, aw22xxx_led_off_cfg, sizeof(aw22xxx_led_off_cfg)/sizeof(unsigned char));
    return 0;
}
static ssize_t aw22xxx_para_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int *databuf = NULL;
	//unsigned char rgb_idx = 0;
	unsigned char i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    (void)aw22xxx;
    pr_info("%s enter\n", __func__);
    databuf = kmalloc(CMD_LINE_LEN*sizeof(unsigned int), GFP_KERNEL);
    if (!databuf) {
        pr_info("Fail to alloc memory");
        return -EFAULT;
    }

    if(CMD_LINE_LEN == sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n", &databuf[0], &databuf[1],
		&databuf[2],&databuf[3],&databuf[4],&databuf[5],&databuf[6],&databuf[7],&databuf[8],&databuf[9],&databuf[10],&databuf[11], &databuf[12],
		&databuf[13],&databuf[14],&databuf[15],&databuf[16],&databuf[17],&databuf[18],&databuf[19],&databuf[20],&databuf[21],&databuf[22],&databuf[23],&databuf[24])) {
		pr_info("%s len = %d\n", __func__,CMD_LINE_LEN);
		if(write_idx < CMD_LINE_NUMS){
			for(i = 0;i < CMD_LINE_LEN;i++){
				user_para_data[(write_idx)*CMD_LINE_LEN + i] = (unsigned char)databuf[i];
			}
			write_idx ++;
			pr_info("%s write_idx = %d \n", __func__,write_idx);
		}else {
			pr_info("%s not enough space\n", __func__);
		}
	}
	kfree(databuf);
    return len;
}
static ssize_t aw22xxx_para_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    ssize_t len = 0;
    unsigned int i = 0;
    (void)led_cdev;
    (void)aw22xxx;
	if(write_idx > 0){
		for(i = 0; i < (write_idx*CMD_LINE_LEN); i ++) {
			if(i%CMD_LINE_LEN == 0){
				len += snprintf(buf+len, PAGE_SIZE-len, "\r\n");
				len += snprintf(buf+len, PAGE_SIZE-len, "%d  ",(int)user_para_data[i]);

			}else {
				len += snprintf(buf+len, PAGE_SIZE-len, "%d  ",(int)user_para_data[i]);
				}
			}
		len += snprintf(buf+len, PAGE_SIZE-len, "\r\n");
	}

    return len;
}
void aw22xxx_set_breath_data(struct aw22xxx *aw22xxx,unsigned char * databuf)
{
	aw22xxx_blink_cfg[3*2 + 1] = (unsigned char)databuf[8];//iled0
	aw22xxx_blink_cfg[6*2 + 1] = (unsigned char)databuf[9];//iled1
	aw22xxx_blink_cfg[9*2 + 1] = (unsigned char)databuf[10];//iled2

	aw22xxx_blink_cfg[12*2 + 1] = (unsigned char)databuf[11];//iled0
	aw22xxx_blink_cfg[15*2 + 1] = (unsigned char)databuf[12];//iled1
	aw22xxx_blink_cfg[18*2 + 1] = (unsigned char)databuf[13];//iled2

	aw22xxx_blink_cfg[21*2 + 1] = (unsigned char)databuf[14];//iled0
	aw22xxx_blink_cfg[24*2 + 1] = (unsigned char)databuf[15];//iled1
	aw22xxx_blink_cfg[27*2 + 1] = (unsigned char)databuf[16];//iled2

	aw22xxx_blink_cfg[30*2 + 1] = (unsigned char)databuf[17];//iled3
	aw22xxx_blink_cfg[33*2 + 1] = (unsigned char)databuf[18];//iled4
	aw22xxx_blink_cfg[36*2 + 1] = (unsigned char)databuf[19];//iled5

	aw22xxx_blink_cfg[39*2 + 1] = (unsigned char)databuf[20];//iled3
	aw22xxx_blink_cfg[42*2 + 1] = (unsigned char)databuf[20];//iled4
	aw22xxx_blink_cfg[45*2 + 1] = (unsigned char)databuf[20];//iled5

	aw22xxx_blink_cfg[48*2 + 1] = (unsigned char)databuf[21];//iled3
	aw22xxx_blink_cfg[51*2 + 1] = (unsigned char)databuf[21];//iled4
	aw22xxx_blink_cfg[54*2 + 1] = (unsigned char)databuf[21];//iled5

	aw22xxx_blink_cfg[57*2 + 1] = (unsigned char)databuf[22];//iled3
	aw22xxx_blink_cfg[60*2 + 1] = (unsigned char)databuf[22];//iled4
	aw22xxx_blink_cfg[63*2 + 1] = (unsigned char)databuf[22];//iled5

	aw22xxx_blink_cfg[66*2 + 1] = (unsigned char)databuf[23];//iled3
	aw22xxx_blink_cfg[69*2 + 1] = (unsigned char)databuf[23];//iled4
	aw22xxx_blink_cfg[72*2 + 1] = (unsigned char)databuf[23];//iled5
	duration = (unsigned char)databuf[24];
	aw22xxx_blink_cfg_update_array(aw22xxx);
}
int aw22xxx_play(void * data)
{
	struct aw22xxx *aw22xxx = (struct aw22xxx *)data;
    aw22xxx_init_cfg_update_array((struct aw22xxx *)data);
	aw22xxx_set_breath_data((struct aw22xxx *)data,&user_para_data[0]);
	while(1){
		msleep(duration/30);
			if(read_idx == write_idx){
				kthread_status = 0;
				read_idx = 0;
			    write_idx = 0;
				//aw22xxx_cfg_led_off((struct aw22xxx *)data);
				schedule_work(&aw22xxx->cfg_work);
				//pr_info("%s (!!!while 888)kthread exit\n",__func__);
				do_exit(0);

		}
		if(kthread_status == 0){
			aw22xxx_cfg_led_off((struct aw22xxx *)data);
			pr_info("%s kthread exit\n",__func__);
			do_exit(1);
		}
		aw22xxx_set_breath_data((struct aw22xxx *)data,&user_para_data[read_idx*CMD_LINE_LEN]);
		read_idx++;

	}
	return 1;
}
static ssize_t aw22xxx_multi_breath_pattern_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
	unsigned int databuf = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
	//pr_info("%s fzc kthread enter \n",__func__);

	if(1 == sscanf(buf, "%x", &databuf)) {
		//pr_info("%s fzc kthread enter  %d  pattern_status=%d\n",__func__,databuf,aw22xxx->pattern_status);
		aw22xxx->pattern_status = databuf;
		if(aw22xxx->pattern_status == 1){
			read_idx = 0;
			if(kthread_status == 0 && write_idx > 0){
				cfg_update_kthread = kthread_run(aw22xxx_play, (void *)aw22xxx, "%s", "aw22xxx_play");
				if (!IS_ERR(cfg_update_kthread))
				{
					kthread_status = 1;
					printk("kthread_create done\n");
				}
				else
				{
					kthread_status = 0;
					printk("kthread_create error\n");
				}
			}
		}else if(aw22xxx->pattern_status == 0){
			//printk("kthread_create  aw22xxx->pattern_status = 0 %d\n",kthread_status);
			read_idx = 0;
			write_idx = 0;
			if(kthread_status == 1)
				kthread_status = 0;
				//kthread_stop(cfg_update_kthread);
			aw22xxx_cfg_led_off(aw22xxx);
		}
	}
	return len;
}
static ssize_t aw22xxx_multi_breath_pattern_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, " pattern_status = %d\n", aw22xxx->pattern_status);
    return len;
}


/***********************************end********************************************/
/*
static int aw22xxx_mcu_wakeup(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_WAKEUP_MASK, BIT_MCUCTR_MCU_WAKEUP_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_WAKEUP_MASK, BIT_MCUCTR_MCU_WAKEUP_DISABLE);
    }
    return 0;
}
*/

static int aw22xxx_mcu_reset(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_RESET_MASK, BIT_MCUCTR_MCU_RESET_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_RESET_MASK, BIT_MCUCTR_MCU_RESET_DISABLE);
    }
    return 0;
}

static int aw22xxx_mcu_enable(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_WORK_MASK, BIT_MCUCTR_MCU_WORK_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_WORK_MASK, BIT_MCUCTR_MCU_WORK_DISABLE);
    }
    return 0;
}

static int aw22xxx_led_task0_cfg(struct aw22xxx *aw22xxx, unsigned char task)
{
    aw22xxx_i2c_write(aw22xxx, REG_TASK0, task);
    return 0;
}

static int aw22xxx_led_task1_cfg(struct aw22xxx *aw22xxx, unsigned char task)
{
    aw22xxx_i2c_write(aw22xxx, REG_TASK1, task);
    return 0;
}

/*
static int aw22xxx_get_pst(struct aw22xxx *aw22xxx, unsigned char *pst)
{
    int ret = -1;
    ret = aw22xxx_i2c_read(aw22xxx, REG_PST, pst);
    if(ret < 0) {
        pr_err("%s: error=%d\n", __func__, ret);
    }
    return ret;
}
*/

static int aw22xxx_imax_cfg(struct aw22xxx *aw22xxx, unsigned char imax)
{
    if(imax > 0x0f) {
        imax = 0x0f;
    }
    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_i2c_write(aw22xxx, REG_IMAX, imax);

    return 0;
}

/*
static int aw22xxx_audio_enable(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
                BIT_AUDCTR_AUDEN_MASK, BIT_AUDCTR_AUDEN_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
                BIT_AUDCTR_AUDEN_MASK, BIT_AUDCTR_AUDEN_DISABLE);
    }
    return 0;
}

static int aw22xxx_agc_enable(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
                BIT_AUDCTR_AGCEN_MASK, BIT_AUDCTR_AGCEN_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
                BIT_AUDCTR_AGCEN_MASK, BIT_AUDCTR_AGCEN_DISABLE);
    }
    return 0;
}

static int aw22xxx_agc_igain_cfg(struct aw22xxx *aw22xxx, unsigned char igain)
{
    if(igain > 0x3f) {
        igain = 0x3f;
    }
    aw22xxx_i2c_write(aw22xxx, REG_IGAIN, igain);

    return 0;
}

static int aw22xxx_get_agc_gain(struct aw22xxx *aw22xxx, unsigned char *gain)
{
    int ret = -1;
    ret = aw22xxx_i2c_read(aw22xxx, REG_GAIN, gain);
    if(ret < 0) {
        pr_err("%s: error=%d\n", __func__, ret);
    }
    return ret;
}
*/

static int aw22xxx_dbgctr_cfg(struct aw22xxx *aw22xxx, unsigned char cfg)
{
    if(cfg >= (AW22XXX_DBGCTR_MAX-1)) {
        cfg = AW22XXX_DBGCTR_NORMAL;
    }
    aw22xxx_i2c_write(aw22xxx, REG_DBGCTR, cfg);

    return 0;
}

static int aw22xxx_addr_cfg(struct aw22xxx *aw22xxx, unsigned int addr)
{
    aw22xxx_i2c_write(aw22xxx, REG_ADDR1, (unsigned char)((addr>>0)&0xff));
    aw22xxx_i2c_write(aw22xxx, REG_ADDR2, (unsigned char)((addr>>8)&0xff));

    return 0;
}

static int aw22xxx_data_cfg(struct aw22xxx *aw22xxx, unsigned int data)
{
    aw22xxx_i2c_write(aw22xxx, REG_DATA, data);

    return 0;
}
/*
static int aw22xxx_get_fw_version(struct aw22xxx *aw22xxx)
{
    unsigned char reg_val;
    unsigned char i;

    pr_info("%s: enter\n", __func__);

    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);
    aw22xxx_led_task0_cfg(aw22xxx, 0xff);
    aw22xxx_mcu_reset(aw22xxx, false);
    msleep(2);
    aw22xxx_mcu_reset(aw22xxx, true);

    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE2);
    aw22xxx->fw_version = 0;
    for(i=0; i<4; i++) {
        aw22xxx_i2c_read(aw22xxx, 0xe9+i, &reg_val);
        aw22xxx->fw_version |= (reg_val<<((3-i)*8));
    }
    pr_info("%s: flash fw version: 0x%04x\n", __func__, aw22xxx->fw_version);

    aw22xxx_i2c_read(aw22xxx, 0xd0, &reg_val);
    switch(reg_val) {
        case AW22118_CHIPID:
            aw22xxx->chipid= AW22118;
            pr_info("%s: flash chipid: aw22118\n", __func__);
            break;
        case AW22127_CHIPID:
            aw22xxx->chipid= AW22127;
            pr_info("%s: flash chipid: aw22127\n", __func__);
            break;
        default:
            pr_err("%s: unknown id=0x%02x\n", __func__, reg_val);
    }

    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);

    return 0;
}
*/

static int aw22xxx_led_display(struct aw22xxx *aw22xxx)
{
    aw22xxx_addr_cfg(aw22xxx, 0x00e1);
    aw22xxx_dbgctr_cfg(aw22xxx, AW22XXX_DBGCTR_SFR);
    aw22xxx_data_cfg(aw22xxx, 0x3d);
    aw22xxx_dbgctr_cfg(aw22xxx, AW22XXX_DBGCTR_NORMAL);
    return 0;
}

static int aw22xxx_led_off(struct aw22xxx *aw22xxx)
{
    aw22xxx_led_task0_cfg(aw22xxx, 0xff);
    aw22xxx_mcu_reset(aw22xxx, true);
    return 0;
}

static void aw22xxx_brightness_work(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx,
          brightness_work);

    pr_info("%s: enter\n", __func__);

    aw22xxx_led_off(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, false);
    if(aw22xxx->cdev.brightness) {
        aw22xxx_chip_enable(aw22xxx, true);
        aw22xxx_mcu_enable(aw22xxx, true);

        aw22xxx_imax_cfg(aw22xxx, (unsigned char)aw22xxx->imax);
        aw22xxx_led_display(aw22xxx);

        aw22xxx_led_task0_cfg(aw22xxx, 0x82);
        aw22xxx_mcu_reset(aw22xxx, false);
    }
}

static void aw22xxx_set_brightness(struct led_classdev *cdev,
           enum led_brightness brightness)
{
    struct aw22xxx *aw22xxx = container_of(cdev, struct aw22xxx, cdev);

    aw22xxx->cdev.brightness = brightness;

    schedule_work(&aw22xxx->brightness_work);
}

static void aw22xxx_task_work(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx,
          task_work);

    pr_info("%s: enter\n", __func__);

    aw22xxx_led_off(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, false);
    if(aw22xxx->task0) {
        aw22xxx_chip_enable(aw22xxx, true);
        aw22xxx_mcu_enable(aw22xxx, true);

        aw22xxx_imax_cfg(aw22xxx, (unsigned char)aw22xxx->imax);
        aw22xxx_led_display(aw22xxx);

        aw22xxx_led_task0_cfg(aw22xxx, aw22xxx->task0);
        aw22xxx_led_task1_cfg(aw22xxx, aw22xxx->task1);
        aw22xxx_mcu_reset(aw22xxx, false);
    }
}

static int aw22xxx_led_init(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_imax_cfg(aw22xxx, aw22xxx_imax_code[aw22xxx->imax]);
    aw22xxx_chip_enable(aw22xxx, false);

    pr_info("%s: exit\n", __func__);

    return 0;
}

void aw22xxx_led_imax_cfg(struct aw22xxx * aw22xxx)
{
      u8 imax_id = 0;

      /*if (aw22xxx->effect == 0x3) {
          imax_id = AW22XXX_IMAX_45mA;
      } else if (aw22xxx->effect == 0x37) {
          imax_id = AW22XXX_IMAX_75mA;
      } else if (aw22xxx->effect == 0x2 || aw22xxx->effect == 0x8) {
          imax_id = AW22XXX_IMAX_10mA;
      } else {
          imax_id = AW22XXX_IMAX_30mA;
      }*/

	  imax_id = AW22XXX_IMAX_75mA;	//set the current to max for debug,it will be adjusted after system_test
      aw22xxx_imax_cfg(aw22xxx, imax_id);

      aw22xxx->imax = imax_id;
      pr_info("%s: nubia set %s imax_level=%d.\n", __func__, aw22xxx_cfg_name[aw22xxx->effect], imax_id);
}

/*****************************************************
 *
 * firmware/cfg update
 *
 *****************************************************/
static void aw22xxx_cfg_loaded(const struct firmware *cont, void *context)
{
    struct aw22xxx *aw22xxx = context;
    int i = 0;
    unsigned char page = 0;
    unsigned char reg_addr = 0;
    unsigned char reg_val = 0;

    pr_info("%s: enter\n", __func__);

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
        release_firmware(cont);

        return;
    }

    pr_info("%s: loaded %s - size: %zu\n", __func__, aw22xxx_cfg_name[aw22xxx->effect],
                    cont ? cont->size : 0);
/*
    for(i=0; i<cont->size; i++) {
        pr_info("%s: addr:0x%04x, data:0x%02x\n", __func__, i, *(cont->data+i));
    }
*/
    for(i=0; i<cont->size; i+=2) {
        if(*(cont->data+i) == 0xff) {
            page = *(cont->data+i+1);
        }
        if(aw22xxx->cfg == 1) {
            aw22xxx_i2c_write(aw22xxx, *(cont->data+i), *(cont->data+i+1));
            pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
        } else {
            if(page == AW22XXX_REG_PAGE1) {
                reg_addr = *(cont->data+i);
                if((reg_addr<0x2b) && (reg_addr>0x0f)) {
                    reg_addr -= 0x10;
                    reg_val = (unsigned char)(((aw22xxx->rgb[reg_addr/3])>>(8*(2-reg_addr%3)))&0xff);
                    aw22xxx_i2c_write(aw22xxx, *(cont->data+i), reg_val);
                    pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), reg_val);
                } else {
                    aw22xxx_i2c_write(aw22xxx, *(cont->data+i), *(cont->data+i+1));
                    pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
                }
            } else {
                aw22xxx_i2c_write(aw22xxx, *(cont->data+i), *(cont->data+i+1));
                pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
            }
        }
        if(page == AW22XXX_REG_PAGE0) {
            reg_addr = *(cont->data+i);
            reg_val = *(cont->data+i+1);
            /* gcr chip enable delay */
            if((reg_addr == REG_GCR) &&
                    ((reg_val&BIT_GCR_CHIPEN_ENABLE) == BIT_GCR_CHIPEN_ENABLE)) {
                msleep(2);
            }
        }
    }

    release_firmware(cont);
//add by nubia zhouruituan start
	//aw22xxx_led_imax_cfg(aw22xxx);
    aw22xxx_imax_cfg(aw22xxx,aw22xxx_imax_code[9]);
//add by nubia zhouruituan end
    pr_info("%s: cfg update complete\n", __func__);
}
static bool aw22xxx_is_need_init(struct aw22xxx *aw22xxx)
{
	static bool init_flg = false;

	if(aw22xxx->effect==1){
		aw22xxx->fan_led_stat = true;
	}
	if(aw22xxx->effect==2){
		aw22xxx->fan_led_stat = false;
	}

	if(init_flg == false){
		init_flg = true;
		return true;
	}
	if(aw22xxx->effect == 3){
		return true;
	}

	if(aw22xxx->effect>=AW22XXX_CFG_CUSTOM_MAX && aw22xxx->fan_led_stat==false){
		return true;
	}

	return 0;
}
static int aw22xxx_cfg_update(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    if(aw22xxx->effect < (sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX)) {
        pr_info("%s: cfg name=%s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
    } else {
        pr_err("%s: effect 0x%02x over max value \n", __func__, aw22xxx->effect);
        return -1;
    }
#define EFFECT_NAME_MIN_LEN  5
	if(strlen(aw22xxx_cfg_name[aw22xxx->effect]) < EFFECT_NAME_MIN_LEN){
		pr_err("%s: effect 0x%02x name is NULL, stop to load it!\n", __func__, aw22xxx->effect);
		return -1;
	}

    if(aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
        pr_err("%s: fw update error: not compelte \n", __func__);
        return -2;
    }

	if(aw22xxx_is_need_init(aw22xxx))
		aw22xxx_init_cfg_update_array(aw22xxx);

    return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
                aw22xxx_cfg_name[aw22xxx->effect], aw22xxx->dev, GFP_KERNEL,
                aw22xxx, aw22xxx_cfg_loaded);
}

static int aw22xxx_cfg_update_wait(struct aw22xxx *aw22xxx)
{
	const struct firmware *firmware = NULL;
	int ret = 0;
    pr_info("%s: enter\n", __func__);

    if(aw22xxx->effect < (sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX)) {
        pr_info("%s: cfg name=%s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
    } else {
        pr_err("%s: effect 0x%02x over max value \n", __func__, aw22xxx->effect);
        return -1;
    }
#define EFFECT_NAME_MIN_LEN  5
	if(strlen(aw22xxx_cfg_name[aw22xxx->effect]) < EFFECT_NAME_MIN_LEN){
		pr_err("%s: effect 0x%02x name is NULL, stop to load it!\n", __func__, aw22xxx->effect);
		return -1;
	}

    if(aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
        pr_err("%s: fw update error: not compelte \n", __func__);
        return -2;
    }

	if(aw22xxx_is_need_init(aw22xxx))
		aw22xxx_init_cfg_update_array(aw22xxx);

	ret = request_firmware(&firmware, aw22xxx_cfg_name[aw22xxx->effect], aw22xxx->dev);
	if(ret != 0){
		pr_err("%s: firmware request fail!\n", __func__);
		return ret;
	}

	aw22xxx_cfg_loaded(firmware, aw22xxx);

	msleep(20);	//wait for bin exec

    return ret;
}

static int aw22xxx_container_update(struct aw22xxx *aw22xxx,
        struct aw22xxx_container *aw22xxx_fw)
{
    unsigned int i;
    unsigned char reg_val;
    unsigned int tmp_bist;
#ifdef AW22XXX_FLASH_I2C_WRITES
    unsigned int tmp_len;
#endif

    /* chip enable*/
    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);

    /* flash cfg */
    aw22xxx_i2c_write(aw22xxx, 0x80, 0xec);
    aw22xxx_i2c_write(aw22xxx, 0x35, 0x29);
    //aw22xxx_i2c_write(aw22xxx, 0x37, 0xba);
    aw22xxx_i2c_write(aw22xxx, 0x38, aw22xxx_fw->key);

    /* flash erase*/
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x03);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x03);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(40);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x40);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x02);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(6);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x42);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x02);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(6);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x44);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x02);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(6);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x00);

#ifdef AW22XXX_FLASH_I2C_WRITES
    /* flash writes */
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x03);
    for(i=0; i<aw22xxx_fw->len; i+=tmp_len) {
        aw22xxx_i2c_write(aw22xxx, 0x22, ((i>>8)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x21, ((i>>0)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x11, 0x01);
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x04);
        if((aw22xxx_fw->len - i) < MAX_FLASH_WRITE_BYTE_SIZE) {
            tmp_len = aw22xxx_fw->len - i;
        } else {
            tmp_len = MAX_FLASH_WRITE_BYTE_SIZE;
        }
        aw22xxx_i2c_writes(aw22xxx, 0x23, &aw22xxx_fw->data[i], tmp_len);
        aw22xxx_i2c_write(aw22xxx, 0x11, 0x00);
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    }
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x00);
#else
    /* flash write */
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x03);
    for(i=0; i<aw22xxx_fw->len; i++) {
        aw22xxx_i2c_write(aw22xxx, 0x22, ((i>>8)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x21, ((i>>0)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x04);
        aw22xxx_i2c_write(aw22xxx, 0x23, aw22xxx_fw->data[i]);
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    }
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x00);
#endif

    /* bist check */
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);
    aw22xxx_i2c_write(aw22xxx, 0x22, (((aw22xxx_fw->len-1)>>8)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x21, (((aw22xxx_fw->len-1)>>0)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x07);
    msleep(5);
    aw22xxx_i2c_read(aw22xxx, 0x24, &reg_val);
    if(reg_val == 0x05) {
        aw22xxx_i2c_read(aw22xxx, 0x25, &reg_val);
        tmp_bist = reg_val;
        aw22xxx_i2c_read(aw22xxx, 0x26, &reg_val);
        tmp_bist |= (reg_val<<8);
        if(tmp_bist == aw22xxx_fw->bist) {
            pr_info("%s: bist check pass, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
        } else {
            pr_err("%s: bist check fail, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
            pr_err("%s: fw update failed, please reset phone\n", __func__);
            return -1;
        }
    } else {
        pr_err("%s: bist check is running, reg0x24=0x%02x\n", __func__, reg_val);
    }
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x00);

    return 0;
}

static void aw22xxx_fw_loaded(const struct firmware *cont, void *context)
{
    struct aw22xxx *aw22xxx = context;
    struct aw22xxx_container *aw22xxx_fw;
    int i = 0;
    int ret = -1;
    char tmp_buf[32] = {0};
    unsigned int shift = 0;
    unsigned short check_sum = 0;
    unsigned char reg_val = 0;
    unsigned int tmp_bist = 0;

    pr_info("%s: enter\n", __func__);

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw22xxx_fw_name);
        release_firmware(cont);
        return;
    }

    pr_info("%s: loaded %s - size: %zu\n", __func__, aw22xxx_fw_name,
                    cont ? cont->size : 0);
/*
    for(i=0; i<cont->size; i++) {
        pr_info("%s: addr:0x%04x, data:0x%02x\n", __func__, i, *(cont->data+i));
    }
*/
    /* check sum */
    for(i=2; i<cont->size; i++) {
        check_sum += cont->data[i];
    }
    if(check_sum != (unsigned short)((cont->data[0]<<8)|(cont->data[1]))) {
        pr_err("%s: check sum err: check_sum=0x%04x\n", __func__, check_sum);
        release_firmware(cont);
        return;
    } else {
        pr_info("%s: check sum pass : 0x%04x\n", __func__, check_sum);
    }

    /* get fw info */
    aw22xxx_fw = kzalloc(cont->size + 4*sizeof(unsigned int), GFP_KERNEL);
    if (!aw22xxx_fw) {
        release_firmware(cont);
        pr_err("%s: Error allocating memory\n", __func__);
        return;
    }
    shift += 2;

    pr_info("%s: fw chip_id : 0x%02x\n", __func__, cont->data[0+shift]);
    shift += 1;

    memcpy(tmp_buf, &cont->data[0+shift], 16);
    pr_info("%s: fw customer: %s\n", __func__, tmp_buf);
    shift += 16;

    memcpy(tmp_buf, &cont->data[0+shift], 8);
    pr_info("%s: fw project: %s\n", __func__, tmp_buf);
    shift += 8;

    aw22xxx_fw->version = (cont->data[0+shift]<<24) | (cont->data[1+shift]<<16) |
            (cont->data[2+shift]<<8) | (cont->data[3+shift]<<0);
    pr_info("%s: fw version : 0x%04x\n", __func__, aw22xxx_fw->version);
    shift += 4;

    // reserved
    shift += 3;

    aw22xxx_fw->bist = (cont->data[0+shift]<<8) | (cont->data[1+shift]<<0);
    pr_info("%s: fw bist : 0x%04x\n", __func__, aw22xxx_fw->bist);
    shift += 2;

    aw22xxx_fw->key = cont->data[0+shift];
    pr_info("%s: fw key : 0x%04x\n", __func__, aw22xxx_fw->key);
    shift += 1;

    // reserved
    shift += 1;

    aw22xxx_fw->len = (cont->data[0+shift]<<8) | (cont->data[1+shift]<<0);
    pr_info("%s: fw len : 0x%04x\n", __func__, aw22xxx_fw->len);
    shift += 2;

    memcpy(aw22xxx_fw->data, &cont->data[shift], aw22xxx_fw->len);
    release_firmware(cont);

    /* check version */
    //aw22xxx_get_fw_version(aw22xxx);

    /* bist check */
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);
    aw22xxx_i2c_write(aw22xxx, 0x22, (((aw22xxx_fw->len-1)>>8)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x21, (((aw22xxx_fw->len-1)>>0)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x07);
    msleep(5);
    aw22xxx_i2c_read(aw22xxx, 0x24, &reg_val);
    if(reg_val == 0x05) {
        aw22xxx_i2c_read(aw22xxx, 0x25, &reg_val);
        tmp_bist = reg_val;
        aw22xxx_i2c_read(aw22xxx, 0x26, &reg_val);
        tmp_bist |= (reg_val<<8);
        if(tmp_bist == aw22xxx_fw->bist) {
            pr_info("%s: bist check pass, bist=0x%04x\n",
                    __func__, aw22xxx_fw->bist);
            if(aw22xxx->fw_update == 0) {
                kfree(aw22xxx_fw);
                aw22xxx_i2c_write(aw22xxx, 0x24, 0x00);
                aw22xxx_led_init(aw22xxx);
                aw22xxx->fw_flags = AW22XXX_FLAG_FW_OK;
                return;
            } else {
                pr_info("%s: fw version: 0x%04x, force update fw\n",
                    __func__, aw22xxx_fw->version);
            }
        } else {
            pr_info("%s: bist check fail, fw bist=0x%04x, flash bist=0x%04x\n",
                    __func__, aw22xxx_fw->bist, tmp_bist);
            pr_info("%s: find new fw: 0x%04x, need update\n",
                    __func__, aw22xxx_fw->version);
        }
    } else {
        pr_err("%s: bist check is running, reg0x24=0x%02x\n", __func__, reg_val);
        pr_info("%s: fw need update\n", __func__);
    }
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x00);

    /* fw update */
    ret = aw22xxx_container_update(aw22xxx, aw22xxx_fw);
    if(ret) {
        aw22xxx->fw_flags = AW22XXX_FLAG_FW_FAIL;
    } else {
        aw22xxx->fw_flags = AW22XXX_FLAG_FW_OK;
    }
    kfree(aw22xxx_fw);

    aw22xxx->fw_update = 0;

    aw22xxx_led_init(aw22xxx);

    pr_info("%s: exit\n", __func__);
}

static int aw22xxx_fw_update(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    aw22xxx->fw_flags = AW22XXX_FLAG_FW_UPDATE;

    return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
                aw22xxx_fw_name, aw22xxx->dev, GFP_KERNEL,
                aw22xxx, aw22xxx_fw_loaded);
}

#ifdef AWINIC_FW_UPDATE_DELAY
static enum hrtimer_restart aw22xxx_fw_timer_func(struct hrtimer *timer)
{
    struct aw22xxx *aw22xxx = container_of(timer, struct aw22xxx, fw_timer);

    pr_info("%s: enter\n", __func__);

    schedule_work(&aw22xxx->fw_work);

    return HRTIMER_NORESTART;
}
#endif

static void aw22xxx_fw_work_routine(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx, fw_work);

    pr_info("%s: enter\n", __func__);

    aw22xxx_fw_update(aw22xxx);

}

static void aw22xxx_cfg_work_routine(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx, cfg_work);

    pr_info("%s: enter\n", __func__);
    aw22xxx_cfg_update(aw22xxx);

}

static int aw22xxx_load_nubia_fw_name(struct aw22xxx *aw22xxx)
{
    unsigned int gpio_value = 0;

    memset(aw22xxx_fw_name, 0, AW22XXX_CFG_NAME_MAX);
    strcpy(aw22xxx_fw_name, FW_NAME_DEFAULT);

    // judge the fw name only for nx669j-s
#if 0
    if (gpio_is_valid(aw22xxx->nubia_ver_gpio)) {
        gpio_value = gpio_get_value(aw22xxx->nubia_ver_gpio);
        if (gpio_value == 1) {
            memset(aw22xxx_fw_name, 0, AW22XXX_CFG_NAME_MAX);
            strcpy(aw22xxx_fw_name, FW_NAME_NX669J_V1E);
        }
    }
#endif
    AW22_LOG("use default fw, nubia_ver_gpio:%d, gpio_value:%d, fw name :%s", aw22xxx->nubia_ver_gpio, gpio_value, aw22xxx_fw_name);
    return 0;
}

static int aw22xxx_fw_init(struct aw22xxx *aw22xxx)
{
#ifdef AWINIC_FW_UPDATE_DELAY
    int fw_timer_val = 1000;

    hrtimer_init(&aw22xxx->fw_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    aw22xxx->fw_timer.function = aw22xxx_fw_timer_func;
    INIT_WORK(&aw22xxx->fw_work, aw22xxx_fw_work_routine);
    INIT_WORK(&aw22xxx->cfg_work, aw22xxx_cfg_work_routine);
    hrtimer_start(&aw22xxx->fw_timer,
            ktime_set(fw_timer_val/1000, (fw_timer_val%1000)*1000000),
            HRTIMER_MODE_REL);
#else
    INIT_WORK(&aw22xxx->fw_work, aw22xxx_fw_work_routine);
    INIT_WORK(&aw22xxx->cfg_work, aw22xxx_cfg_work_routine);
    schedule_work(&aw22xxx->fw_work);
#endif
    return 0;
}


/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw22xxx_interrupt_clear(struct aw22xxx *aw22xxx)
{
    unsigned char reg_val;

    pr_info("%s: enter\n", __func__);

    aw22xxx_i2c_read(aw22xxx, REG_INTST, &reg_val);
    pr_info("%s: reg INTST=0x%x\n", __func__, reg_val);
}

static void aw22xxx_interrupt_setup(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    aw22xxx_interrupt_clear(aw22xxx);

    aw22xxx_i2c_write_bits(aw22xxx, REG_INTEN,
            BIT_INTEN_FUNCMPE_MASK, BIT_INTEN_FUNCMPE_ENABLE);
}

static irqreturn_t aw22xxx_irq(int irq, void *data)
{
    struct aw22xxx *aw22xxx = data;
    unsigned char reg_val;

    pr_info("%s: enter\n", __func__);

    aw22xxx_i2c_read(aw22xxx, REG_INTST, &reg_val);
    pr_info("%s: reg INTST=0x%x\n", __func__, reg_val);

    if(reg_val & BIT_INTST_FUNCMPE) {
        pr_info("%s: functions compelte!\n", __func__);
		if(kthread_status == 0 ){
			funcmp_flag = 0;
			aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
			aw22xxx_mcu_reset(aw22xxx, true);
			aw22xxx_mcu_enable(aw22xxx, false);
			aw22xxx_chip_enable(aw22xxx, false);
		}else if(kthread_status == 1){
			funcmp_flag = 1;
		}
        pr_info("%s: enter standby mode!\n", __func__);
    }
    pr_info("%s exit\n", __func__);

    return IRQ_HANDLED;
}

#ifdef NUBIA_MULTICOLOR_LED
static int aw22xxx_pinctrl_init(struct device *dev,
		struct aw22xxx *aw22xxx_data)
{
	aw22xxx_data->pinctrl_info.pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(aw22xxx_data->pinctrl_info.pinctrl)) {
		pr_err("%s: aw22xxx get pinctrl info error.\n", __func__);
		return -EINVAL;
	}

	aw22xxx_data->pinctrl_info.pin_active = pinctrl_lookup_state(
					aw22xxx_data->pinctrl_info.pinctrl,
					AW22XXX_PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(aw22xxx_data->pinctrl_info.pin_active)) {
		pr_err("%s: aw22xxx get pin_active info error.\n",__func__);
		return -EINVAL;
	}

	aw22xxx_data->pinctrl_info.pin_suspend = pinctrl_lookup_state(
					aw22xxx_data->pinctrl_info.pinctrl,
					AW22XXX_PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(aw22xxx_data->pinctrl_info.pin_suspend)) {
		pr_err("%s: aw22xxx get pin_suspend info error.\n",__func__);
		return -EINVAL;
	}

	return 0;
}

static int aw22xxx_pinctrl_set_state(
		struct aw22xxx *aw22xxx_data,
		bool active)
{
	int ret = -1;

	if (!aw22xxx_data->pinctrl_info.pinctrl ||
			!aw22xxx_data->pinctrl_info.pin_active ||
			!aw22xxx_data->pinctrl_info.pin_suspend) {
		pr_err("%s: pinctrl is invalid, skip.\n",__func__);
		return ret;
	}
	if (active) {
		ret = pinctrl_select_state(aw22xxx_data->pinctrl_info.pinctrl,
				aw22xxx_data->pinctrl_info.pin_active);
	} else {
		ret = pinctrl_select_state(aw22xxx_data->pinctrl_info.pinctrl,
				aw22xxx_data->pinctrl_info.pin_suspend);
	}
	pr_debug("%s: set pinctrl to [%s], ret = %d.\n", __func__,
			active ? "active" : "suspend", ret);

	return ret;
}
#endif

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw22xxx_parse_dt(struct device *dev, struct aw22xxx *aw22xxx,
        struct device_node *np)
{
    aw22xxx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
    if (aw22xxx->reset_gpio < 0) {
        dev_err(dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
        return -1;
    } else {
        dev_info(dev, "%s: reset gpio provided ok, aw22xxx->reset_gpio=%d\n", __func__, aw22xxx->reset_gpio);
    }
    aw22xxx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
    if (aw22xxx->irq_gpio < 0) {
        dev_err(dev, "%s: no irq gpio provided, will not suppport intterupt\n", __func__);
        return -1;
    } else {
        dev_info(dev, "%s: irq gpio provided ok, aw22xxx->irq_gpio=%d\n", __func__, aw22xxx->irq_gpio);
    }

    aw22xxx->nubia_ver_gpio = of_get_named_gpio(np, "nubia_ver-gpio", 0);
    if (!gpio_is_valid(aw22xxx->nubia_ver_gpio)) {
        dev_info(dev, "%s: not suppport nubia_ver_gpio, regard as normal version\n", __func__);
    } else {
        dev_info(dev, "%s: nubia_ver_gpio provided ok, aw22xxx->nubia_ver_gpio=%d\n", __func__, aw22xxx->nubia_ver_gpio);
    }
    return 0;
}

static int aw22xxx_hw_reset(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    if (aw22xxx && gpio_is_valid(aw22xxx->reset_gpio)) {
        gpio_set_value_cansleep(aw22xxx->reset_gpio, 0);
        msleep(1);
        gpio_set_value_cansleep(aw22xxx->reset_gpio, 1);
        msleep(1);
    } else {
        dev_err(aw22xxx->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

static int aw22xxx_hw_off(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    if (aw22xxx && gpio_is_valid(aw22xxx->reset_gpio)) {
        gpio_set_value_cansleep(aw22xxx->reset_gpio, 0);
        msleep(1);
    } else {
        dev_err(aw22xxx->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw22xxx_read_chipid(struct aw22xxx *aw22xxx)
{
    int ret = -1;
    unsigned char cnt = 0;
    unsigned char reg_val = 0;

    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_sw_reset(aw22xxx);

    while(cnt < AW_READ_CHIPID_RETRIES) {
        ret = aw22xxx_i2c_read(aw22xxx, REG_SRST, &reg_val);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                "%s: failed to read register AW22XXX_REG_ID: %d\n",
                __func__, ret);
            return -EIO;
        }
        switch (reg_val) {
            case AW22XXX_SRSTR:
                pr_info("%s aw22xxx detected\n", __func__);
                //aw22xxx->flags |= AW22XXX_FLAG_SKIP_INTERRUPTS;
                aw22xxx_i2c_read(aw22xxx, REG_CHIPID, &reg_val);
                switch(reg_val) {
                    case AW22118_CHIPID:
                        aw22xxx->chipid= AW22118;
                        pr_info("%s: chipid: aw22118\n", __func__);
                        break;
                    case AW22127_CHIPID:
                        aw22xxx->chipid= AW22127;
                        pr_info("%s: chipid: aw22127\n", __func__);
                        break;
                    default:
                        pr_err("%s: unknown id=0x%02x\n", __func__, reg_val);
                        break;
                }
                return 0;
            default:
                pr_info("%s unsupported device revision (0x%x)\n",
                    __func__, reg_val );
                break;
        }
        cnt ++;

        msleep(AW_READ_CHIPID_RETRY_DELAY);
    }

    return -EINVAL;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw22xxx_reg_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[2] = {0, 0};

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw22xxx_i2c_write(aw22xxx, databuf[0], databuf[1]);
    }

    return count;
}

static ssize_t aw22xxx_reg_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    ssize_t len = 0;
    unsigned int i = 0;
    unsigned char reg_val = 0;
    unsigned char reg_page = 0;
    aw22xxx_i2c_read(aw22xxx, REG_PAGE, &reg_page);
    for(i = 0; i < AW22XXX_REG_MAX; i ++) {
        if(!reg_page) {
            if(!(aw22xxx_reg_access[i]&REG_RD_ACCESS))
               continue;
        }
        aw22xxx_i2c_read(aw22xxx, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x \n", i, reg_val);
    }
    return len;
}

static ssize_t aw22xxx_hwen_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(1 == databuf[0]) {
            aw22xxx_hw_reset(aw22xxx);
        } else {
            aw22xxx_hw_off(aw22xxx);
        }
    }

    return count;
}

static ssize_t aw22xxx_hwen_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n",
            gpio_get_value(aw22xxx->reset_gpio));

    return len;
}

static ssize_t aw22xxx_fw_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        aw22xxx->fw_update = databuf[0];
        if(1 == databuf[0]) {
            schedule_work(&aw22xxx->fw_work);
        }
    }

    return count;
}

static ssize_t aw22xxx_fw_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "firmware name = %s\n", aw22xxx_fw_name);

    return len;
}

static ssize_t aw22xxx_cfg_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[1] = {0};
	unsigned char non_block = 0;

    mutex_lock(&aw22xxx->cfg_lock);
    if(1 == sscanf(buf, "%x", &databuf[0])) {
        AW22_LOG("cfg=%02x\n", databuf[0]);
        aw22xxx->cfg = databuf[0] & 0x0f;
        non_block = databuf[0] & 0xf0;
        if(aw22xxx->cfg) {
            if(non_block)
                schedule_work(&aw22xxx->cfg_work);
            else
                aw22xxx_cfg_update_wait(aw22xxx);
        }
    }
	if(aw22xxx->effect == 0)	//wait for close finish
		msleep(100);
    mutex_unlock(&aw22xxx->cfg_lock);

    return count;
}

static ssize_t aw22xxx_cfg_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX; i++) {
		if(!strcmp(aw22xxx_cfg_name[i], "null"))
			continue;
        len += snprintf(buf+len, PAGE_SIZE-len, "cfg[%x] = %s\n", i, aw22xxx_cfg_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current cfg = %s\n", aw22xxx_cfg_name[aw22xxx->effect]);

    return len;
}

static ssize_t aw22xxx_effect_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    mutex_lock(&aw22xxx->cfg_lock);
    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->effect = databuf[0];
    mutex_unlock(&aw22xxx->cfg_lock);

	AW22_LOG("effect=%x\n", aw22xxx->effect);

    return len;
}

static ssize_t aw22xxx_effect_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    len += snprintf(buf+len, PAGE_SIZE-len, "effect = 0x%02x\n", aw22xxx->effect);

    return len;
}

static ssize_t aw22xxx_imax_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->imax = databuf[0];
    aw22xxx_imax_cfg(aw22xxx, aw22xxx_imax_code[aw22xxx->imax]);

    return len;
}

static ssize_t aw22xxx_imax_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<sizeof(aw22xxx_imax_name)/AW22XXX_IMAX_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "imax[%x] = %s\n", i, aw22xxx_imax_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current id = 0x%02x, imax = %s\n",
        aw22xxx->imax, aw22xxx_imax_name[aw22xxx->imax]);

    return len;
}

static ssize_t aw22xxx_rgb_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[2];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x %x",&databuf[0], &databuf[1]);
    aw22xxx->rgb[databuf[0]] = databuf[1];

	AW22_LOG("rgb=%d\n", aw22xxx->rgb);

    return len;
}

static ssize_t aw22xxx_rgb_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<AW22XXX_RGB_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "rgb[%d] = 0x%06x\n", i, aw22xxx->rgb[i]);
    }
    return len;
}

static ssize_t aw22xxx_task0_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->task0 = databuf[0];
    schedule_work(&aw22xxx->task_work);

    return len;
}

static ssize_t aw22xxx_task0_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    len += snprintf(buf+len, PAGE_SIZE-len, "task0 = 0x%02x\n", aw22xxx->task0);

    return len;
}

static ssize_t aw22xxx_task1_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->task1 = databuf[0];

    return len;
}

static ssize_t aw22xxx_task1_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    len += snprintf(buf+len, PAGE_SIZE-len, "task1 = 0x%02x\n", aw22xxx->task1);

    return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw22xxx_reg_show, aw22xxx_reg_store);
static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw22xxx_hwen_show, aw22xxx_hwen_store);
static DEVICE_ATTR(fw, S_IWUSR | S_IRUGO, aw22xxx_fw_show, aw22xxx_fw_store);
static DEVICE_ATTR(cfg, S_IWUSR | S_IRUGO, aw22xxx_cfg_show, aw22xxx_cfg_store);
static DEVICE_ATTR(effect, S_IWUSR | S_IRUGO, aw22xxx_effect_show, aw22xxx_effect_store);
static DEVICE_ATTR(imax, S_IWUSR | S_IRUGO, aw22xxx_imax_show, aw22xxx_imax_store);
static DEVICE_ATTR(rgb, S_IWUSR | S_IRUGO, aw22xxx_rgb_show, aw22xxx_rgb_store);
static DEVICE_ATTR(task0, S_IWUSR | S_IRUGO, aw22xxx_task0_show, aw22xxx_task0_store);
static DEVICE_ATTR(task1, S_IWUSR | S_IRUGO, aw22xxx_task1_show, aw22xxx_task1_store);

static DEVICE_ATTR(para, S_IWUSR | S_IRUGO, aw22xxx_para_show, aw22xxx_para_store);
static DEVICE_ATTR(pattern, S_IWUSR | S_IRUGO, aw22xxx_multi_breath_pattern_show, aw22xxx_multi_breath_pattern_store);

static struct attribute *aw22xxx_attributes[] = {
    &dev_attr_reg.attr,
    &dev_attr_hwen.attr,
    &dev_attr_fw.attr,
    &dev_attr_cfg.attr,
    &dev_attr_effect.attr,
    &dev_attr_imax.attr,
    &dev_attr_rgb.attr,
    &dev_attr_task0.attr,
    &dev_attr_task1.attr,
    &dev_attr_para.attr,
    &dev_attr_pattern.attr,
    NULL
};

static struct attribute_group aw22xxx_attribute_group = {
    .attrs = aw22xxx_attributes
};


/******************************************************
 *
 * led class dev
 *
 ******************************************************/
static int aw22xxx_parse_led_cdev(struct aw22xxx *aw22xxx,
        struct device_node *np)
{
    struct device_node *temp;
    int ret = -1;

    pr_info("%s: enter\n", __func__);

    for_each_child_of_node(np, temp) {
        ret = of_property_read_string(temp, "aw22xxx,name",
            &aw22xxx->cdev.name);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                "Failure reading led name, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw22xxx,imax",
            &aw22xxx->imax);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                "Failure reading imax, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw22xxx,brightness",
            &aw22xxx->cdev.brightness);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                "Failure reading brightness, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw22xxx,max_brightness",
            &aw22xxx->cdev.max_brightness);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                "Failure reading max brightness, ret = %d\n", ret);
            goto free_pdata;
        }
    }

    INIT_WORK(&aw22xxx->brightness_work, aw22xxx_brightness_work);
    INIT_WORK(&aw22xxx->task_work, aw22xxx_task_work);

    aw22xxx->cdev.brightness_set = aw22xxx_set_brightness;
    ret = led_classdev_register(aw22xxx->dev, &aw22xxx->cdev);
    if (ret) {
        dev_err(aw22xxx->dev,
            "unable to register led ret=%d\n", ret);
        goto free_pdata;
    }

    ret = sysfs_create_group(&aw22xxx->cdev.dev->kobj,
            &aw22xxx_attribute_group);
    if (ret) {
        dev_err(aw22xxx->dev, "led sysfs ret: %d\n", ret);
        goto free_class;
    }

    return 0;

free_class:
    led_classdev_unregister(&aw22xxx->cdev);
free_pdata:
    return ret;
}

void aw22xxx_build_leds_name(void)
{
	int i;

	for(i = AW22XXX_CFG_CUSTOM_MAX; i < AW22XXX_CFG_NUM_MAX; i++)
	{
		if (i <= 0xaf)
			sprintf(aw22xxx_cfg_name[i], "aw_cfg_%x.bin", i + 0x50);
		else
			strcpy(aw22xxx_cfg_name[i], "null");
	}
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw22xxx_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    struct aw22xxx *aw22xxx;
    struct device_node *np = i2c->dev.of_node;
    int ret;
    int irq_flags;
	int gpio_request_flags = 0;

    AW22_LOG("enter.\n");


    if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
        dev_err(&i2c->dev, "check_functionality failed\n");
        return -EIO;
    }

    aw22xxx = devm_kzalloc(&i2c->dev, sizeof(struct aw22xxx), GFP_KERNEL);
    if (aw22xxx == NULL)
        return -ENOMEM;

    aw22xxx->dev = &i2c->dev;
    aw22xxx->i2c = i2c;

    i2c_set_clientdata(i2c, aw22xxx);

    mutex_init(&aw22xxx->cfg_lock);
#ifdef NUBIA_MULTICOLOR_LED
	aw22xxx->cfg = 1;
	ret = aw22xxx_pinctrl_init(&i2c->dev, aw22xxx);
	if (ret < 0) {
		pr_err("%s: aw22xxx pinctrl init failed.\n", __func__);
		goto pinctrl_init_failed;
	}
	ret = aw22xxx_pinctrl_set_state(aw22xxx, true);
	if (ret) {
		pr_err("%s: aw22xxx pinctrl set state failed.\n", __func__);
		goto pinctrl_set_failed;
	}
	pr_debug("%s: aw22xxx pinctrl inited, set state to active successfully.\n", __func__);
#endif

    /* aw22xxx rst & int */
    if (np) {
        ret = aw22xxx_parse_dt(&i2c->dev, aw22xxx, np);
        if (ret) {
            dev_err(&i2c->dev, "%s: failed to parse device tree node\n", __func__);
            goto err_parse_dt;
        }
    } else {
        aw22xxx->reset_gpio = -1;
        aw22xxx->irq_gpio = -1;
    }

    if (gpio_is_valid(aw22xxx->reset_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw22xxx->reset_gpio,
            GPIOF_OUT_INIT_LOW, "aw22xxx_rst");
        if (ret){
            dev_err(&i2c->dev, "%s: rst request failed\n", __func__);
            goto err_gpio_request;
        }
    }

    if (gpio_is_valid(aw22xxx->irq_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw22xxx->irq_gpio,
            GPIOF_DIR_IN, "aw22xxx_int");
        if (ret){
            dev_err(&i2c->dev, "%s: int request failed\n", __func__);
            gpio_request_flags = 1;
        }
    }

    /* hardware reset */
    aw22xxx_hw_reset(aw22xxx);
	//add by jones 659S
    AW22_LOG("enter multicolor_led = %d\n",multicolor_led );

    if (multicolor_led == 2 || multicolor_led == 1)
	    goto err_led;
    /* aw22xxx chip id */
    ret = aw22xxx_read_chipid(aw22xxx);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s: aw22xxx_read_chipid failed ret=%d\n", __func__, ret);
        goto err_id;
    }

    /* aw22xxx irq */
    if (gpio_is_valid(aw22xxx->irq_gpio) &&
        !(aw22xxx->flags & AW22XXX_FLAG_SKIP_INTERRUPTS) && !gpio_request_flags) {
        /* register irq handler */
        aw22xxx_interrupt_setup(aw22xxx);
        irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        ret = devm_request_threaded_irq(&i2c->dev,
                    gpio_to_irq(aw22xxx->irq_gpio),
                    NULL, aw22xxx_irq, irq_flags,
                    "aw22xxx", aw22xxx);
        if (ret != 0) {
            dev_err(&i2c->dev, "%s: failed to request IRQ %d: %d\n",
                    __func__, gpio_to_irq(aw22xxx->irq_gpio), ret);
            goto err_irq;
        }
    } else {
        dev_info(&i2c->dev, "%s skipping IRQ registration gpio_request_flags = %d\n", __func__,gpio_request_flags);
        /* disable feature support if gpio was invalid */
        aw22xxx->flags |= AW22XXX_FLAG_SKIP_INTERRUPTS;
    }

    dev_set_drvdata(&i2c->dev, aw22xxx);

    aw22xxx_parse_led_cdev(aw22xxx, np);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s error creating led class dev\n", __func__);
        goto err_sysfs;
    }

    aw22xxx_load_nubia_fw_name(aw22xxx);
    aw22xxx_fw_init(aw22xxx);

	aw22xxx_build_leds_name();

    AW22_LOG("probe completed successfully!\n");

    return 0;

err_sysfs:
    devm_free_irq(&i2c->dev, gpio_to_irq(aw22xxx->irq_gpio), aw22xxx);
err_irq:
err_id:
#ifdef NUBIA_MULTICOLOR_LED
pinctrl_set_failed:
	devm_pinctrl_put(aw22xxx->pinctrl_info.pinctrl);
pinctrl_init_failed:
	aw22xxx->pinctrl_info.pinctrl = NULL;
#endif
#ifdef CONFIG_NUBIA_LED_AW22XXX_CTS
    devm_gpio_free(&i2c->dev, aw22xxx->reset_gpio);
    devm_gpio_free(&i2c->dev, aw22xxx->irq_gpio);
#endif
err_gpio_request:
err_parse_dt:
    devm_kfree(&i2c->dev, aw22xxx);
    aw22xxx = NULL;
    return ret;
//add by jones 659S
err_led:
     gpio_set_value_cansleep(aw22xxx->reset_gpio, 0);
     devm_gpio_request_one(&i2c->dev, aw22xxx->irq_gpio,GPIOF_DIR_OUT, "aw22xxx_int");
#ifdef CONFIG_NUBIA_LED_AW22XXX_CTS
     devm_gpio_free(&i2c->dev, aw22xxx->reset_gpio);
     devm_gpio_free(&i2c->dev, aw22xxx->irq_gpio);
#endif
     return multicolor_led = 0;
}

static int aw22xxx_i2c_remove(struct i2c_client *i2c)
{
    struct aw22xxx *aw22xxx = i2c_get_clientdata(i2c);

    pr_info("%s: enter\n", __func__);
    sysfs_remove_group(&aw22xxx->cdev.dev->kobj,
            &aw22xxx_attribute_group);
    led_classdev_unregister(&aw22xxx->cdev);

    devm_free_irq(&i2c->dev, gpio_to_irq(aw22xxx->irq_gpio), aw22xxx);
#ifdef CONFIG_NUBIA_LED_AW22XXX_CTS
    if (gpio_is_valid(aw22xxx->reset_gpio))
        devm_gpio_free(&i2c->dev, aw22xxx->reset_gpio);
    if (gpio_is_valid(aw22xxx->irq_gpio))
        devm_gpio_free(&i2c->dev, aw22xxx->irq_gpio);
#endif
    devm_kfree(&i2c->dev, aw22xxx);
    aw22xxx = NULL;

    return 0;
}

static const struct i2c_device_id aw22xxx_i2c_id[] = {
    { AW22XXX_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, aw22xxx_i2c_id);

static struct of_device_id aw22xxx_dt_match[] = {
    { .compatible = "awinic,aw22xxx_led" },
    { },
};

static struct i2c_driver aw22xxx_i2c_driver = {
    .driver = {
        .name = AW22XXX_I2C_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(aw22xxx_dt_match),
    },
    .probe = aw22xxx_i2c_probe,
    .remove = aw22xxx_i2c_remove,
    .id_table = aw22xxx_i2c_id,
};


static int __init aw22xxx_i2c_init(void)
{
    int ret = 0;

    pr_info("aw22xxx driver version %s\n", AW22XXX_VERSION);

    ret = i2c_add_driver(&aw22xxx_i2c_driver);
    if(ret){
        pr_err("fail to add aw22xxx device into i2c\n");
        return ret;
    }

    return 0;
}
module_init(aw22xxx_i2c_init);


static void __exit aw22xxx_i2c_exit(void)
{
    i2c_del_driver(&aw22xxx_i2c_driver);
}
module_exit(aw22xxx_i2c_exit);


MODULE_DESCRIPTION("AW22XXX LED Driver");
MODULE_LICENSE("GPL v2");
