#include "eagle_soc.h"
#include "c_types.h"
#include "driver/ir_tx_rx.h"
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "driver/i2s.h"
#include "driver/sigma_delta.h"
#include "driver/hw_timer.h"

#define IR_NEC_TX_IO_MUX        IR_GPIO_OUT_MUX
#define IR_NEC_TX_GPIO_NUM      IR_GPIO_OUT_NUM

LOCAL os_timer_t ir_tx_timer;
void ir_tx_handler();

typedef enum{
     TX_BIT_CARRIER,
     TX_BIT_LOW,
}TX_BIT_STA;

#define IR_TX_OUTPUT_LOW(ir_out_gpio_num)  \
    gpio_output_set(0, 1<<ir_out_gpio_num, 1<<ir_out_gpio_num, 0)
#define IR_TX_OUTPUT_HIGH(ir_out_gpio_num) \
    gpio_output_set(1<<ir_out_gpio_num, 0, 1<<ir_out_gpio_num, 0)

#define IR_TX_SET_INACTIVE(io_num)   IR_TX_OUTPUT_HIGH(io_num)

#if GEN_IR_CLK_FROM_IIS
    /* MODE 1 : GENERATE IR CLK FROM IIS CLOCK */

    //#if GEN_IR_CLK_FROM_IIS
    #define TIMER_INTERVAL_US  63
    #define I2C_BASE                   0x60000D00
    #define I2S_BCK_DIV_NUM         0x0000003F
    #define I2S_BCK_DIV_NUM_S       22
    #define I2S_CLKM_DIV_NUM        0x0000003F
    #define I2S_CLKM_DIV_NUM_S      16
    #define I2S_BITS_MOD            0x0000000F
    #define I2S_BITS_MOD_S          12
    #define I2SCONF                 (DR_REG_I2S_BASE + 0x0008)
    #define DR_REG_I2S_BASE         (0x60000e00)
    
    #define U32 uint32
    #define i2c_bbpll                               0x67
    #define i2c_bbpll_en_audio_clock_out            4
    #define i2c_bbpll_en_audio_clock_out_msb        7
    #define i2c_bbpll_en_audio_clock_out_lsb        7
    #define i2c_bbpll_hostid                        4
    
    /******************************************************************************
     * FunctionName : ir_tx_carrier_clr
     * Description  : stop 38khz carrier clk and output low
     * Parameters   : NONE
     * Returns      : NONE
    *******************************************************************************/
    void  ir_tx_carrier_clr()
    {
        PIN_FUNC_SELECT(IR_GPIO_OUT_MUX, IR_GPIO_OUT_FUNC); 
        IR_TX_SET_INACTIVE(IR_GPIO_OUT_NUM);
        WRITE_PERI_REG(0x60000e08, READ_PERI_REG(0x60000e08) & 0xfffffdff | (0x0<<8) ) ; //i2s clk stop
    }
    
    /******************************************************************************
     * FunctionName : gen_carrier_clk
     * Description  : gen 38khz carrier clk
     * Parameters   : NONE
     * Returns      : NONE
    *******************************************************************************/
    void gen_carrier_clk()
    {
        
        //CONFIG AS I2S 
    #if (IR_NEC_TX_IO_MUX==PERIPHS_IO_MUX_GPIO2_U)
        //set i2s clk freq 
        WRITE_PERI_REG(I2SCONF,  READ_PERI_REG(I2SCONF) & 0xf0000fff|
                            ( (( 62&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|
                            ((2&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S)|
                            ((1&I2S_BITS_MOD  )   <<  I2S_BITS_MOD_S )  )  );
    
        WRITE_PERI_REG(IR_NEC_TX_IO_MUX, (READ_PERI_REG(IR_NEC_TX_IO_MUX)&0xfffffe0f)| (0x1<<4) );
        WRITE_PERI_REG(0x60000e08, READ_PERI_REG(0x60000e08) & 0xfffffdff | (0x1<<8) ); //i2s tx  start
    #endif

    #if (IR_NEC_TX_IO_MUX==PERIPHS_IO_MUX_MTMS_U)
        //set i2s clk freq 
        WRITE_PERI_REG(I2SCONF,  READ_PERI_REG(I2SCONF) & 0xf0000fff|
                            ( (( 62&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|  
                            ((2&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S)|   
                            ((1&I2S_BITS_MOD  )   <<  I2S_BITS_MOD_S )  )  );
    
        WRITE_PERI_REG(IR_NEC_TX_IO_MUX, (READ_PERI_REG(IR_NEC_TX_IO_MUX)&0xfffffe0f)| (0x1<<4) );
        WRITE_PERI_REG(0x60000e08, READ_PERI_REG(0x60000e08) & 0xfffffdff | (0x2<<8) ) ;//i2s rx  start
    #endif
    #if (IR_NEC_TX_IO_MUX==PERIPHS_IO_MUX_MTCK_U)
        //set i2s clk freq 
        WRITE_PERI_REG(I2SCONF,  READ_PERI_REG(I2SCONF) & 0xf0000fff|
                            ( (( 63&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|
                            ((63&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S)| 
                            ((1&I2S_BITS_MOD  )   <<  I2S_BITS_MOD_S )  )  );
    
        WRITE_PERI_REG(IR_NEC_TX_IO_MUX, (READ_PERI_REG(IR_NEC_TX_IO_MUX)&0xfffffe0f)| (0x1<<4) );
        WRITE_PERI_REG(0x60000e08, READ_PERI_REG(0x60000e08) & 0xfffffdff | (0x2<<8) ) ;//i2s rx  start
    #endif
    #if (IR_NEC_TX_IO_MUX==PERIPHS_IO_MUX_MTDO_U)
        //set i2s clk freq 
        WRITE_PERI_REG(I2SCONF,  READ_PERI_REG(I2SCONF) & 0xf0000fff|
                            ( (( 63&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|
                            ((63&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S)|
                            ((1&I2S_BITS_MOD  )   <<  I2S_BITS_MOD_S )));
    
        WRITE_PERI_REG(IR_NEC_TX_IO_MUX, (READ_PERI_REG(IR_NEC_TX_IO_MUX)&0xfffffe0f)| (0x1<<4) );
        WRITE_PERI_REG(0x60000e08, READ_PERI_REG(0x60000e08) & 0xfffffdff | (0x1<<8) ); //i2s tx  start
    #endif
    }
    
#elif GEN_IR_CLK_FROM_DMA_IIS
    /* MODE 2 : GENERATE IR CLK FROM I2S data line */

    /******************************************************************************
     * FunctionName : gen_carrier_clk
     * Description  : gen 38khz carrier clk
     * Parameters   : NONE
     * Returns      : NONE
    *******************************************************************************/
    void gen_carrier_clk()
    {
        
        #if (IR_NEC_TX_IO_MUX == PERIPHS_IO_MUX_U0RXD_U)
        //CONFIG I2S TX PIN FUNC
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);
        i2s_start();  //START
        #else
        os_printf("Please select right GPIO for IIS TX data\r\n");
        #endif
    }
    
    /******************************************************************************
     * FunctionName : ir_tx_carrier_clr
     * Description  : stop 38khz carrier clk and output low
     * Parameters   : NONE
     * Returns      : NONE
    *******************************************************************************/
    void ir_tx_carrier_clr(void)
    {
        PIN_FUNC_SELECT(IR_GPIO_OUT_MUX, IR_GPIO_OUT_FUNC); 
        IR_TX_SET_INACTIVE(IR_GPIO_OUT_NUM);
        
        i2s_stop(); //stop
    }
    
#elif GEN_IR_CLK_FROM_GPIO_SIGMA_DELAT
    /* MODE 3 : GENERATE IR CLK FROM SIGMA-DELTA MODULE */
    extern void sigma_delta_close(uint32 GPIO_NUM);
    extern void sigma_delta_gen_38k(uint32 GPIO_MUX,uint32 GPIO_NUM,uint32 GPIO_FUNC);
    void  ir_tx_carrier_clr()
    {
        sigma_delta_close(IR_NEC_TX_GPIO_NUM);
        IR_TX_SET_INACTIVE(IR_GPIO_OUT_NUM);
    }
    
    void gen_carrier_clk()
    {
        sigma_delta_gen_38k(IR_NEC_TX_IO_MUX,IR_NEC_TX_GPIO_NUM,IR_GPIO_OUT_FUNC);
    }
#endif

#define BBE(txb) (((txb)->clkdef[0]&0x01)==1)
#define BBEVAL(txb) ((txb->bytdat>>(txb->bitidx%8))&0x01)
#define BLEVAL(txb) ((txb->bytdat<<(txb->bitidx%8)>>7)&0x01)
#define CALC_DATIDX(txb) (txb->datidx+2+(txb->bitcnt+7)/8)
#define CALC_DEFBEG(txb) (txb->txdata[txb->datidx])
#define CALC_BITCNT(txb) (txb->txdata[txb->datidx+1])
#define CALC_BYTDAT(txb) (txb->txdata[txb->datidx+2+txb->bitidx/8])
#define CALC_BITDAT(txb) (BBE(txb)?BBEVAL(txb):BLEVAL(txb))
#define CALC_CLKOFF(txb) (txb->bitdat==0?0:1+txb->clkdef[txb->defbeg])
#define CALC_CLKCNT(txb) (txb->clkdef[txb->defbeg+txb->clkoff])
#define CALC_CLKDAT(txb) (txb->clkdef[txb->defbeg+1+txb->clkidx+txb->clkoff])

LOCAL void ICACHE_FLASH_ATTR tx_blk_init(tx_blk* txb,u8* data,int32* def)
{
	txb->clkdef=def;
	txb->txdata=data;
	txb->datidx=1;
	txb->bitidx=0;
	txb->clkidx=0;
	txb->defbeg=CALC_DEFBEG(txb);
	txb->bitcnt=CALC_BITCNT(txb);
	txb->bytdat=CALC_BYTDAT(txb);
	txb->bitdat=CALC_BITDAT(txb);
	txb->clkoff=CALC_CLKOFF(txb);
	txb->clkcnt=CALC_CLKCNT(txb);
	txb->clkdat=CALC_CLKDAT(txb);
	//txb->datidx=0;
}
LOCAL void ICACHE_FLASH_ATTR tx_blk_shift(tx_blk* txb)
{
	u8 txstop=0;
	txb->clkidx++;
	if(txb->clkidx>=txb->clkcnt)
	{
		txb->clkidx=0;
		txb->bitidx++;
		if(txb->bitidx>=txb->bitcnt)
		{
			txb->bitidx=0;
			txb->datidx=CALC_DATIDX(txb);
			if(txb->datidx>=txb->txdata[0])
			{
				txb->datidx=1;
				txstop=1;
			}
			txb->defbeg=CALC_DEFBEG(txb);
			txb->bitcnt=CALC_BITCNT(txb);
		}
		txb->bytdat=CALC_BYTDAT(txb);
		txb->bitdat=CALC_BITDAT(txb);
		txb->clkoff=CALC_CLKOFF(txb);
		txb->clkcnt=CALC_CLKCNT(txb);
	}
	txb->clkdat=CALC_CLKDAT(txb);
	if(txstop==1) txb->datidx=0;
}
void ICACHE_FLASH_ATTR tx_seqdat_build(int32* pout,u8* data,int32* def)
{
	int32 i=1;
	tx_blk txb;
	tx_blk_init(&txb,data,def);
	while(txb.datidx!=0)
	{
		pout[i]=txb.clkdat;
		tx_blk_shift(&txb);
		++i;
	}
	pout[0]=i;
}

LOCAL seq_dat seqdt;
void ICACHE_FLASH_ATTR ir_tx_setup(int32* pseqdt)
{
	seqdt.data=pseqdt;
	seqdt.idx=seqdt.data[0];
}
u8 ICACHE_FLASH_ATTR ir_tx_idle()
{
	if(seqdt.idx<seqdt.data[0]) return 0;
	return 1;
}
void ICACHE_FLASH_ATTR ir_tx_start()
{
	seqdt.idx=1;
	ir_tx_handler();
}
LOCAL uint32 cntx=0;
void ir_tx_handler()
{
	int32 clkdat=0;
    #if IR_TX_STATUS_MACHINE_HW_TIMER
        hw_timer_stop();
    #else
        os_timer_disarm(&ir_tx_timer);
    #endif
	//os_printf("tx handler %d\n",seqdt.idx);
	if(seqdt.idx>=seqdt.data[0])
	{
		cntx++;
		//if(cntx%10==1) os_printf("tx ir count: %d\n",cntx);
		if(lpm_status()>1) lpm_sleep(ir_tx_start,IR_TX_INTER_MS*1000);
		return ;
	}
	clkdat=seqdt.data[seqdt.idx];
	if(clkdat>0)
	{
		//os_printf("send high %d\n",clkdat);
		gen_carrier_clk();
	}
	else
	{
		//os_printf("send low %d\n",-clkdat);
		ir_tx_carrier_clr();
		clkdat=-clkdat;
	}
	#if IR_TX_STATUS_MACHINE_HW_TIMER
	    hw_timer_arm(clkdat);
	    hw_timer_start();
	#else
	    os_timer_arm_us(&ir_tx_timer,clkdat,0);
	#endif
	seqdt.idx++;
}

void ICACHE_FLASH_ATTR ir_tx_init()
{
    /*NOTE: NEC IR code tx function need a us-accurate timer to generate the tx logic waveform*/
    /* So we call system_timer_reinit() to reset the os_timer API's clock*/
    /* Also , Remember to #define USE_US_TIMER to enable  os_timer_arm_us function*/
	
    rom_i2c_writeReg_Mask(i2c_bbpll, i2c_bbpll_hostid, i2c_bbpll_en_audio_clock_out, i2c_bbpll_en_audio_clock_out_msb, i2c_bbpll_en_audio_clock_out_lsb, 1);
#if IR_TX_STATUS_MACHINE_HW_TIMER
     hw_timer_init(0,0);//0:FRC1 INTR LEVEL 0:NOT AUTOLOAD
     hw_timer_set_func(ir_tx_handler);
     hw_timer_stop();
     os_printf("Ir tx status machine Clk is hw_timer\n");
#else
    system_timer_reinit();
    os_timer_disarm(&ir_tx_timer);
    os_timer_setfn(&ir_tx_timer, (os_timer_func_t *)ir_tx_handler, NULL);
    os_printf("Ir tx status machine Clk is soft_timer\n");
#endif  
    //init code for mode 2;
    #if GEN_IR_CLK_FROM_DMA_IIS
    i2s_carrier_duty_set(0xfff);//0xfff:50% : THE DATA WIDTH IS SET 24BIT, SO IF WE SET 0XFFF,THAT IS A HALF-DUTY.
    i2s_dma_mode_init();
    #endif
}
LOCAL int32 seqdat[18]; //发送的电平数据
void ICACHE_FLASH_ATTR ir_init()
{
	//格式:标识,bit电平个数,电平时长,电平时长....bit电平个数,电平时长...
	int32 clk_def[]={0,1,-4000,2,1000,-3000,2,3000,-1000};
	//格式:组数,bit电平定义开始索引,bit数,byte1,byte2,...,bit电平定义,bit数,byte1,byte2,...
	u8 tx_data[]={7,3,8,0xA2,1,1,0};
    os_printf("ir init\r\n");
	tx_seqdat_build(seqdat,tx_data,clk_def);
    ir_tx_setup(seqdat);
	ir_tx_init();
	//ir rx code
    //ir_rx_init();
}
