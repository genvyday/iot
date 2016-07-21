#include "driver/utils.h"
#include <user_interface.h>
uint32 lpmstatus=0;
lpm_cb_t lpmusrcb;
void ICACHE_FLASH_ATTR lpm_cb()
{
	//wifi_fpm_close();
	if(lpmusrcb!=0) (*lpmusrcb)();
}
void lpm_init()
{
	lpmusrcb=0;
	lpmstatus=0;
}
uint32 lpm_status()
{
	return lpmstatus;
}
uint32 lpm_prepare()
{
	if(lpmstatus!=0) return lpmstatus;
	lpmstatus=1;
	os_printf("discon %d\r\n",wifi_station_disconnect());
	return lpmstatus;
}

uint32 ICACHE_FLASH_ATTR lpm_enter()
{
	uint32 r=lpmstatus;
	lpmstatus=2;
	return r;
}
uint32 ICACHE_FLASH_ATTR lpm_leave()
{
	uint32 r=lpmstatus;
	lpmstatus=0;
	if(r)
	{
		wifi_set_opmode(STATION_MODE);
		wifi_station_connect();
	}
	return r;
}
int8 lpm_sleep(lpm_cb_t func,uint32 us)
{
	if(lpmstatus==2)
	{
		lpmstatus=3;
		os_printf("begin set wifi to null mod\r\n");
		os_printf("set wifi mod ret=%d\r\n",wifi_set_opmode(NULL_MODE));
		wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
		wifi_fpm_open();
		wifi_fpm_set_wakeup_cb(lpm_cb);
		system_set_os_print(0);
		os_printf("turn off system log\r\n");
	}
	lpmusrcb=func;
	wifi_fpm_do_sleep(us);
	return 0;
}

void task_exec(os_event_t *e)
{
	task_func_t f=(task_func_t)e->sig;
	(*f)((void*)e->par);
}
void ICACHE_FLASH_ATTR task_init(int qlen)
{
	os_event_t * taskq=(os_event_t *)os_malloc(sizeof(os_event_t)*qlen);
	system_os_task(task_exec,USER_TASK_PRIO_0,taskq,qlen);
}
bool ICACHE_FLASH_ATTR task_post(void* f,void* p)
{
	return system_os_post(USER_TASK_PRIO_0,(os_signal_t)f,(os_param_t)p);
}
#define UART0   0
#define UART1   1
#include"driver/uartdef.h"
/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description  : Internal used function
 *                Use uart1 interface to transfer one char
 * Parameters   : uint8 c - character to tx
 * Returns      : OK
 *******************************************************************************/
void uart_tx_one_char(uint8 uart, uint8 c)
{
    for(;;) {
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);

        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
            break;
        }
    }
    WRITE_PERI_REG(UART_FIFO(uart) , c);
    return ;
}
LOCAL void ICACHE_FLASH_ATTR uart_write_char(uint8 uart,char c)
{
    if (c == '\n')
	{
		(void)uart_tx_one_char(UART1, '\r');
		(void)uart_tx_one_char(UART1, '\n');
    }
	else if (c == '\r')
	{
    }
	else
	{
        (void)uart_tx_one_char(UART1, (uint8)c);
    }
}
void ICACHE_FLASH_ATTR uart1_write_char(char c)
{
	uart_write_char(UART1,c);
}
void ICACHE_FLASH_ATTR uart0_write_char(char c)
{
	uart_write_char(UART0,c);
}
