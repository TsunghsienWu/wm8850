/*++
	linux/arch/arm/mach-wmt/gpio_cfg.c

	Some descriptions of such software. Copyright (c) 2008  WonderMedia Technologies, Inc.

	This program is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software Foundation,
	either version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
	PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <http://www.gnu.org/licenses/>.

	WonderMedia Technologies, Inc.
	10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <mach/gpio-cs.h>

#undef DEBUG
//#define DEBUG

#ifdef DEBUG
#define DBG(x...)	printk(KERN_ALERT x)
#else
#define DBG(x...)
#endif

#define perr(fmt,args...) printk(KERN_ERR"[%s]"fmt,__func__,##args)

/*
* gpio_enable()
*
* \brief	setting GPIO to input mode or output mode
* \retval zero is successfull,others is failed
*/
/*!<; mode: 0 = input, 1 = output */
int gpio_enable(enum GPIO_T gpio_idx, int bit, enum GPIO_MODE_T mode)
{
	if ( gpio_idx >= GP_MAX || bit >7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -EIO;
	}
	
	REG8_VAL(GPIO_EN_BASE + gpio_idx) |= 1<<bit;//enable gpio
	REG8_VAL(GPIO_PULL_EN_BASE + gpio_idx) &= ~(1<<bit);//disable default pull down
	
	if(mode == INPUT)
		REG8_VAL(GPIO_OC_BASE + gpio_idx) &= ~(1<<bit);//set as input
	else
		REG8_VAL(GPIO_OC_BASE + gpio_idx) |= 1<<bit;//set as output

	return 0;
}
EXPORT_SYMBOL(gpio_enable);



int gpio_disable(enum GPIO_T gpio_idx, int bit)
{
	if ( gpio_idx >= GP_MAX || bit >7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -EIO;
	}
	
	REG8_VAL(GPIO_EN_BASE + gpio_idx) &= ~(1<<bit);//disable gpio

	return 0 ;
}
EXPORT_SYMBOL(gpio_disable);


/*
* gpio_set_value()
* \brief	setting GPIO at output high level or output low level
* \retval zero is successful,othres is failed
*/
/*!<; // gpio function name, set to output high or output low: 1 = output high, 0 = output low  */
int gpio_set_value(enum GPIO_T gpio_idx, int bit,enum GPIO_OUTPUT_T value)
{
	if ( gpio_idx >= GP_MAX || bit >7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -EIO;
	}

	/* set GPIO Output Data Register */
	if (value == HIGH)
		REG8_VAL(GPIO_OD_BASE+gpio_idx) |= 1<<bit;
	else
		REG8_VAL(GPIO_OD_BASE+gpio_idx) &= ~(1<<bit);

	return 0;

}
EXPORT_SYMBOL(gpio_set_value);



int gpio_get_value(enum GPIO_T gpio_idx, int bit)
{
	if ( gpio_idx >= GP_MAX || bit >7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -EIO;
	}

	/* read GPIO Input Data Register */
	if (REG8_VAL(GPIO_ID_BASE+gpio_idx) & (1<<bit))
		return HIGH;
	else 
		return LOW;

}
EXPORT_SYMBOL(gpio_get_value);


static int set_irq_type(int reg, int shift, int type)
{
	switch(type){
		case IRQ_LOW:
			REG32_VAL(reg) &= ~(1<<(shift*8));
			REG32_VAL(reg) &= ~(1<<(shift*8+1));
			REG32_VAL(reg) &= ~(1<<(shift*8+2));
			break;
		case IRQ_HIGH:
			REG32_VAL(reg) |= (1<<(shift*8));
			REG32_VAL(reg) &= ~(1<<(shift*8+1));
			REG32_VAL(reg) &= ~(1<<(shift*8+2));
			break;
		case IRQ_FALLING:
			REG32_VAL(reg) &= ~(1<<(shift*8));
			REG32_VAL(reg) |= (1<<(shift*8+1));
			REG32_VAL(reg) &= ~(1<<(shift*8+2));
			break;
		case IRQ_RISING:
			REG32_VAL(reg) |= (1<<(shift*8));
			REG32_VAL(reg) |= (1<<(shift*8+1));
			REG32_VAL(reg) &= ~(1<<(shift*8+2));
			break;
		case IRQ_BOTHEDGE:
			REG32_VAL(reg) &= ~(1<<(shift*8));
			REG32_VAL(reg) &= ~(1<<(shift*8+1));
			REG32_VAL(reg) |= (1<<(shift*8+2));
			break;
		default:
			perr("GPIO IRQ TYPE ERROR\n");
			break;
	}
	REG32_VAL(reg) |= (1<<(shift*8+7));//ENABLE IRQ

	return 0;
}

static int set_irq_disable(int reg, int shift)
{
	REG32_VAL(reg) &= ~(1<<(shift*8+7));//DISABLE IRQ
	return 0;
}

static int set_irq_enable(int reg, int shift)
{
	REG32_VAL(reg) |= 1<<(shift*8+7);//ENABLE IRQ
	return 0;
}

static int check_irq_enable(int reg, int shift)
{
	return REG32_VAL(reg) & (1<<(shift*8+7));
}

static int GP0_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift=0;
	
	if(num > 7 || num < 0){
		perr("Invalid bit.\n");
		return -1;
	}

	//set gpio irq triger type
	if(num < 4){
		shift = num;
		reg = GPIO_IRQ_REQ_BASE;
	}else{
		shift = num-4;
		reg = GPIO_IRQ_REQ_BASE + 0x04;
	}

	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);

	return 0;
}

static int GP1_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift=0;
	
	if(num > 4 || num < 0){
		perr("Invalid bit.\n");
		return -1;
	}

	shift = num;
	reg = GPIO_IRQ_REQ_BASE + 0x08;

	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);

	return 0;
}


//I2S
static int GP10_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int tmp;
	int reg,shift;

	if(num > 7 || num < 0){
		perr("Invalid bit.\n");
		return -1;
	}

	tmp = REG32_VAL(__GPIO_BASE + 0x200);

	if (num == 4) {
		if(tmp&(BIT21|BIT22)){
			perr("GP10 bit4 set irq failed, for pin sharing err.\n");
			return -1;
		}		
	} else if (num == 3) {
		if(tmp&BIT20){
			perr("GP10 bit3 set irq failed, for pin sharing err.\n");
			return -1;
		}
	} else if (num == 2) {
		if(tmp&(BIT19|BIT18)){
			perr("GP10 bit2 set irq failed, for pin sharing err.\n");
			return -1;
		}
	} else if (num == 1) {
		if(tmp&(BIT17|BIT16)){
			perr("GP10 bit1 set irq failed, for pin sharing err.\n");
			return -1;
		}		
	}
	
	if(num < 4){
		shift = num;
		reg = GPIO_IRQ_REQ_BASE + 0x20;
	} else {
		shift = num-4;
		reg = GPIO_IRQ_REQ_BASE + 0x24;
	}
	
	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);
	
	return 0;
}

//SPI
static int GP11_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift=0;	
	
	if(num > 4 || num < 0){
		perr("Invalid bit.\n");
		return -1;
	}
	
	reg = GPIO_IRQ_REQ_BASE + 0x28;
	
	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);
	
	return 0;
}



//UART1 & UART0
static int GP22_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift = 0;

	if(num > 7 || num < 4){
		perr("Invalid bit.\n");
		return -1;
	}

	if(num == 4 || num == 6){
		if(REG32_VAL(__GPIO_BASE + 0x200) & BIT14){//No irq for uart0
			perr("GP22 bit4/6 set irq failed, for pin sharing to UART0 now.\n");
			return -1;	
		}
	}
	
	reg = GPIO_IRQ_REQ_BASE + 0x14;

	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);
	
	return 0;
}

//UART3 & UART2
static int GP23_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift = 0;

	if(num > 7 || num < 0){
		perr("Invalid bit.\n");
		return -1;
	}
		
	if(REG32_VAL(__GPIO_BASE + 0x200)& BIT8){//Sharing ping is for SPI1
		perr("GP23 set irq failed,for pin sharing to SPI1 now.\n");
		return -1;
	}

	if(REG32_VAL(__GPIO_BASE + 0x200)& BIT9){//select uart2
		reg = GPIO_IRQ_REQ_BASE + 0x18;
		
		if( num == 7)
			shift = 1;
		else if ( num == 5)
			shift = 0;
		else if ( num == 3)
			shift = 3;
		else if ( num == 1)
			shift = 2;
		else
			return -1;

	} else {//select for uart3
		reg = GPIO_IRQ_REQ_BASE + 0x1c;
		
		if( num == 7)
			shift = 3;
		else if ( num == 5)
			shift = 1;
		else
			return -1;
	}

	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);
	
	return 0;
}

//KPAD
static int GP26_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift=0;

	if(num > 7 || num < 0){
		perr("Invalid bit.\n");
		return -1;
	}

	if(num < 4){
		shift = num;
		reg = GPIO_IRQ_REQ_BASE + 0x0C;
	} else {
		if( num == 7){
			if( REG32_VAL(__GPIO_BASE + 0x200)&(BIT6|BIT24) ){
				perr("GP26 bit7 set irq failed, for pin sharing to SD2/EBM\n");
				return -1;
			}
		}
		shift = num - 4;
		reg = GPIO_IRQ_REQ_BASE + 0x10;
	}
	
	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);

	return 0;
}

//PWM
static int GP31_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift=0;

	if(num > 3 || num < 2){
		perr("Invalid gpio bit.\n");
		return -1;
	}
		
	if(num == 2){
		if((REG32_VAL(__GPIO_BASE + 0x200)&BIT4) == 0){
			perr("GP31 bit2 set irq failed, for pin sharing error.\n");
			return -1;
		}
	}

	reg = GPIO_IRQ_REQ_BASE + 0x2c;
	
	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);
	
	return 0;
}




static int GP49_setup_irq( int num, enum GPIO_IRQ_T type,enum GPIO_SET_T op) 
{
	int reg,shift=0;

	if(num > 3 || num < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
		


	shift = num;

	reg = GPIO_IRQ_REQ_BASE + 0x18;
	
	if(op == OP_SET)
		set_irq_type(reg, shift, type);
	else if(op == OP_EN)
		set_irq_enable(reg, shift);
	else if(op == OP_DIS)
		set_irq_disable(reg, shift);
	else if(op == OP_CHK)
		return check_irq_enable(reg, shift);
	
	return 0;
}



int gpio_setup_irq(enum GPIO_T gpio_idx, int bit,enum GPIO_IRQ_T type)
{		
	if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
	switch (gpio_idx){
		case GP0:
			return GP0_setup_irq(bit, type,OP_SET);
			
		case GP1:
			return GP1_setup_irq(bit, type,OP_SET);
			
		case GP10:
			return GP10_setup_irq(bit, type,OP_SET);
			
		case GP11:
			return GP11_setup_irq(bit, type,OP_SET);
			
		case GP22:
			return GP22_setup_irq(bit, type,OP_SET);
			
		case GP23:
			return GP23_setup_irq(bit, type,OP_SET);
			
		case GP26:
			return GP26_setup_irq(bit, type,OP_SET);
			
		case GP31:
			return GP31_setup_irq(bit, type,OP_SET);
		case GP49:
			return GP49_setup_irq(bit, type,OP_SET);			
		default:
			break;
	
	}

	return 0;
}
EXPORT_SYMBOL(gpio_setup_irq);

int gpio_enable_irq(enum GPIO_T gpio_idx, int bit)
{		
	if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
	
	switch (gpio_idx){
		case GP0:
			return GP0_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP1:
			return GP1_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP10:
			return GP10_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP11:
			return GP11_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP22:
			return GP22_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP23:
			return GP23_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP26:
			return GP26_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP31:
			return GP31_setup_irq(bit, IRQ_NULL, OP_EN);
			
		case GP49:
			return GP49_setup_irq(bit, IRQ_NULL, OP_EN);			
		default:
			break;
	}

	return 0;
}
EXPORT_SYMBOL(gpio_enable_irq);

gpio26_setup_irq()
{
	int reg;
	reg = GPIO_IRQ_REQ_BASE + 0x1a;
//	REG8_VAL(reg) |= (1<<7);
	REG8_VAL(reg) &= ~0x07;
	printk("gpio26 interrupter register value:%x,reg:%x\n",REG8_VAL(reg),reg);
}
gpio26_enable_irq(int enable)
{
	int reg;
	reg = GPIO_IRQ_REQ_BASE + 0x1a;
	if(enable){
			REG8_VAL(reg) |= (1<<7);
			printk("enable gpi026 int\n");
	}else{
			printk("disable gpio26 int\n");
			REG8_VAL(reg) &= ~(1<<7);
	}	
	printk("REG8_VAL(reg):%x\n",REG8_VAL(reg));
}
int is_irq_enable_gpio26()
{
	int reg;
	reg = GPIO_IRQ_REQ_BASE + 0x1a;
	return REG8_VAL(reg)&(1<<7);
}
int gpio26_irq_state()
{
	int reg;
	reg = GPIO_IRQ_REQ_BASE + 0x63;	
	return REG8_VAL(reg)&(1<<2);
}
void clear_gpio26_irq_state()
{
	int reg;
	reg = GPIO_IRQ_REQ_BASE + 0x63;	
	REG8_VAL(reg) |= (1<<2);		
}
EXPORT_SYMBOL(gpio26_setup_irq);
EXPORT_SYMBOL(gpio26_enable_irq);
EXPORT_SYMBOL(is_irq_enable_gpio26);
EXPORT_SYMBOL(gpio26_irq_state);
EXPORT_SYMBOL(clear_gpio26_irq_state);


int gpio_disable_irq(enum GPIO_T gpio_idx, int bit)
{	
	if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
	
	gpio_irq_clean(gpio_idx,bit);//clean irq
	switch (gpio_idx){
		case GP0:
			return GP0_setup_irq(bit, IRQ_NULL, OP_DIS);
		
		case GP1:
			return GP1_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP10:
			return GP10_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP11:
			return GP11_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP22:
			return GP22_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP23:
			return GP23_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP26:
			return GP26_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP31:
			return GP31_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		case GP49:
			return GP49_setup_irq(bit, IRQ_NULL, OP_DIS);
			
		default:
			break;
	}

	return 0;
}
EXPORT_SYMBOL(gpio_disable_irq);

int gpio_irq_isenable(enum GPIO_T gpio_idx, int bit)
{		
	if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
	
	switch (gpio_idx){
		case GP0:
			return GP0_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP1:
			return GP1_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP10:
			return GP10_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP11:
			return GP11_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP22:
			return GP22_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP23:
			return GP23_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP26:
			return GP26_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP31:
			return GP31_setup_irq(bit, IRQ_NULL, OP_CHK);
			
		case GP49:
			return GP49_setup_irq(bit, IRQ_NULL, OP_CHK);	
			
		default:
			break;
	}

	return 0;
}
EXPORT_SYMBOL(gpio_irq_isenable);

int gpio_irq_state(enum GPIO_T gpio_idx, int bit)
{
	if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
	
	switch (gpio_idx){
		case GP0: //GPIO[7:0]
			return REG32_VAL(GPIO_IRQ_STS_BASE) & (1<<bit) ;//BIT[7:0]

		case GP1://GPIO[11:8]
			return (REG32_VAL(GPIO_IRQ_STS_BASE) & (1<<(bit+8)));//BIT[11:8]
			
		case GP10://I2S[7:0]
			return (REG32_VAL(GPIO_IRQ_STS_BASE + 0x04) & (1<<bit));//BIT[7:0]
			
		case GP11://SPI[3:0]
			return (REG32_VAL(GPIO_IRQ_STS_BASE + 0x04) & (1<<(bit+8)));//BIT[11:8]
			
		case GP22://UART1[7:4]
			return (REG32_VAL(GPIO_IRQ_STS_BASE) & (1<<(bit+16)));//BIT[23:20]
			
		case GP23://UART3[7][5]/UART2[7][5][3][1]??
			return (REG32_VAL(GPIO_IRQ_STS_BASE) & (1<<(bit+24)));
			
		case GP26://KPAD COL[3:0] ROW[3:0]
			return (REG32_VAL(GPIO_IRQ_STS_BASE) & (1<<(bit+12)));//BIT[19:12]
			
		case GP31://PWM[3:2]
			return (REG32_VAL(GPIO_IRQ_STS_BASE + 0x04) & (1<<(bit+10)));//BIT[13:12]

		case GP49:
			return (REG32_VAL(GPIO_IRQ_STS_BASE) & (1<<(bit+24)));

			
		default:
			break;
	
	}

	return 0;
}
EXPORT_SYMBOL(gpio_irq_state);

int gpio_irq_clean(enum GPIO_T gpio_idx, int bit)
{
	if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
	
	switch (gpio_idx){
		case GP0: //GPIO[7:0]
			REG32_VAL(GPIO_IRQ_STS_BASE) = 1<<bit ;//BIT[7:0]
			break;
			
		case GP1://GPIO[11:8]
			REG32_VAL(GPIO_IRQ_STS_BASE) = 1<<(bit+8);//BIT[11:8]
			break;
			
		case GP10://I2S[7:0]
			REG32_VAL(GPIO_IRQ_STS_BASE + 0x04) = 1<<bit;//BIT[7:0]
			break;
			
		case GP11://SPI[3:0]
			REG32_VAL(GPIO_IRQ_STS_BASE + 0x04) = 1<<(bit+8);//BIT[11:8]
			break;
			
		case GP22://UART1[7:4]
			REG32_VAL(GPIO_IRQ_STS_BASE) = 1<<(bit+16);//BIT[23:20]
			break;
			
		case GP23://UART3[7][5]/UART2[7][5][3][1]??
			REG32_VAL(GPIO_IRQ_STS_BASE) = 1<<(bit+24);
			break;
			
		case GP26://KPAD COL[3:0] ROW[3:0]
			REG32_VAL(GPIO_IRQ_STS_BASE) = 1<<(bit+12);//BIT[19:12]
			break;
			
		case GP31://PWM[3:2]
			REG32_VAL(GPIO_IRQ_STS_BASE + 0x04) = 1<<(bit+10);//BIT[13:12]
			break;


		case GP49:
			REG32_VAL(GPIO_IRQ_STS_BASE) = 1<<(bit+24);
			break;
			
		default:
			break;
	
	}
	
	return 0;
}
EXPORT_SYMBOL(gpio_irq_clean);


/*
 * mode =0 pull down, mode = 1 pull up
 */
int gpio_pull_enable(enum GPIO_T gpio_idx, int bit,enum GPIO_PULL_T mode)
{
    if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
    
	REG8_VAL(GPIO_PULL_EN_BASE+gpio_idx) = 1<<bit;

	if(mode == PULL_UP)
		REG8_VAL(GPIO_PULL_CTRL_BASE+gpio_idx) |= 1<<bit;
	else
		REG8_VAL(GPIO_PULL_CTRL_BASE+gpio_idx) &= ~(1<<bit);
	
	return 0;
}
EXPORT_SYMBOL(gpio_pull_enable);


int gpio_pull_disable(enum GPIO_T gpio_idx, int bit)
{
    if(gpio_idx >= GP_MAX || bit > 7 || bit < 0){
		perr("Invalid gpio bit.\n");
		return -1;
	}
    
	REG8_VAL(GPIO_PULL_EN_BASE+gpio_idx) &= ~(1<<bit);
    
	return 0;
}
EXPORT_SYMBOL(gpio_pull_disable);


