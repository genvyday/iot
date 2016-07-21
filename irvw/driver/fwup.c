/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_esp_platform.c
 *
 * Description: The client mode configration.
 *              Check your hardware connection with the host while use this mode.
 *
 * Modification history:
 *     2014/5/09, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
//#include "user_esp_platform.h"
#include "upgrade.h"



#define ESP_DEBUG

#ifdef ESP_DEBUG
#define ESP_DBG os_printf
#else
#define ESP_DBG
#endif


#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate,sdch\r\n\
Accept-Language: zh-CN,zh;q=0.8\r\n\r\n"
os_timer_t fwup_timer;
uint8 upgrading;
/******************************************************************************
 * FunctionName : fwup_rsp
 * Description  : Processing the downloaded data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR timer_func(os_timer_func_t f,int ms);
LOCAL void ICACHE_FLASH_ATTR fwup_rsp(void *arg)
{
    struct upgrade_server_info *server = arg;
	if(server->upgrade_flag==true)
	{
    	ESP_DBG("upgrade finished,will reboot now\n");
		os_timer_disarm(&fwup_timer);
		os_timer_setfn(&fwup_timer, (os_timer_func_t*)system_upgrade_reboot , NULL);
		os_timer_arm(&fwup_timer,2000,1);
	}
	else
	{
        ESP_DBG("upgrade failed\n");
    }
	upgrading=0;
    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;
}
void ICACHE_FLASH_ATTR fwup_timer_cb(struct upgrade_server_info *server)
{
	os_timer_disarm(&fwup_timer);
	upgrading=1;
	system_upgrade_start(server);
}
uint16 ICACHE_FLASH_ATTR str2ip(uint8* ip,const char* s)
{
	int i=0;
	uint16 port=80;
	ip[0]=ip[1]=ip[2]=ip[3]=0;
	while(*s!=0&&*s!=':')
	{
		if(*s!='.') ip[i]=ip[i]*10+*s-'0';
		else i++;
		++s;
	}
	if(*s==':')
	{
		++s;
		port=0;
		while(*s!=0) port=port*10+*s++-'0';
	}
	return port;
}
/******************************************************************************
 * FunctionName : fwup_begin
 * Description  : Processing the received data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 *                server -- upgrade param
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR fwup_begin(struct espconn *pespconn,const char* ip)
{
	uint8 user_bin[30] = {0};
	struct upgrade_server_info *server = NULL;
	server = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));
	server->pespconn = pespconn;
	server->port = str2ip(server->ip,ip);
	server->check_cb = fwup_rsp;
	server->check_times = 120000;

	if (server->url == NULL) 
	{
		server->url = (uint8 *)os_zalloc(512);
	}
	if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1)
	{
		os_strcpy(user_bin, "user2.bin");
	}
	else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
	{
		os_strcpy(user_bin, "user1.bin");
	}
	os_sprintf(server->url, "GET /espup/%s HTTP/1.0\r\nHost: %s\r\n"pheadbuffer"",user_bin, ip);
	ESP_DBG("%s\n",user_bin);
	os_timer_disarm(&fwup_timer);
	os_timer_setfn(&fwup_timer, (os_timer_func_t*)fwup_timer_cb,server);
	os_timer_arm(&fwup_timer,200,1);
	//if (system_upgrade_start(server) == false) 
	//{
	//	ESP_DBG("upgrade is already started\n");
	//}
}
bool ICACHE_FLASH_ATTR fwup_upgrading()
{
	return upgrading!=0;
}
