
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "driver/ringbuf.h"
#include "driver/ir_tx_rx.h"
#include "upgrade.h"
os_timer_t main_timer;
void wifi_checkstatus();
int wifistat=0;
uint32 upts;
uint32 upus;
#define SERVER_LOCAL_PORT   1112
#define STNSSID  "stx"
#define STNPSWD  "start10wlan26"
void wifi_event(System_Event_t *evt)
{
	switch (evt->event)
	{
		case EVENT_STAMODE_CONNECTED:
			break;
		case EVENT_STAMODE_DISCONNECTED:
			lpm_enter();
			break;
		case EVENT_STAMODE_AUTHMODE_CHANGE:
			break;
		case EVENT_STAMODE_GOT_IP:
			break;
		case EVENT_SOFTAPMODE_STACONNECTED:
			break;
		case EVENT_SOFTAPMODE_STADISCONNECTED:
			break;
	}
}
void ICACHE_FLASH_ATTR wifi_init()
{
	wifi_set_event_handler_cb(wifi_event);
	wifi_set_opmode(STATION_MODE); //STATION_MODE
	//wifi_set_sleep_type(LIGHT_SLEEP_T); //MODEM_SLEEP_T
	//return;
   // Wifi configuration 
   char ssid[32] = STNSSID; 
   char password[64] = STNPSWD;
   struct station_config stationConf;
   
   os_memset(stationConf.ssid, 0, 32);
   os_memset(stationConf.password, 0, 64);
   //need not mac address
   stationConf.bssid_set = 0; 
   
   //Set ap settings 
   os_strcpy(&stationConf.ssid, ssid); 
   os_strcpy(&stationConf.password, password); 
   wifi_station_set_config(&stationConf);
}
void wifi_checkstatus()
{
	uint8 status; 
	//struct ip_info ipconfig;
	//wifi_get_ip_info(STATION_IF, &ipconfig); //&& ipconfig.ip.addr != 0
	status=wifi_station_get_connect_status();
	if (status == STATION_GOT_IP )
	{
		os_printf("got ip !!! \r\n");
		cmdsvr_init(SERVER_LOCAL_PORT);
		wifistat=1;
	}
	else 
	{
		if ((status == STATION_WRONG_PASSWORD ||
		status == STATION_NO_AP_FOUND ||
		status == STATION_CONNECT_FAIL))
		{
			wifistat=-1;
			os_printf("connect fail !!! \r\n");
		}
	}
}
void ICACHE_FLASH_ATTR user_timer()
{
	upus=system_get_time()-upts;
	if(lpm_status()==3)
	{
		os_timer_disarm(&main_timer);
		return ;
	}
	if(upus>60*1000000&&!fwup_upgrading()&&lpm_status()==0)
	{
		os_printf("entering light sleep1\r\n");
		cmdsvr_shutdown();
		lpm_prepare();
	}
	if(wifistat==0&&lpm_status()==0) wifi_checkstatus();
	if(ir_tx_idle()) ir_tx_start();
}

void ICACHE_FLASH_ATTR user_main()
{
    os_printf("SDK version:%s %d\n", system_get_sdk_version(),upts);
	//task_init(30);
	wifi_set_event_handler_cb(wifi_event);
	//wifi_set_sleep_type(LIGHT_SLEEP_T); //MODEM_SLEEP_T
	ir_init();
	//send IR every n ms
    os_timer_disarm(&main_timer);
    os_timer_setfn(&main_timer, user_timer , NULL);
    os_timer_arm(&main_timer,IR_TX_INTER_MS,1);
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
	upts=system_get_time();
    uart_div_modify(0, 80000000 / 115200);
	os_printf("\r\n\r\n");
	wifi_init();
	system_init_done_cb(user_main);
}
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

