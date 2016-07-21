#ifndef UTILS_H_
#define UTILS_H_
#include "ets_sys.h"
#include "osapi.h"
#include <mem.h>
typedef void (*lpm_cb_t)(void);
typedef void (*task_func_t)(void*);
uint32 lpm_status();
uint32 ICACHE_FLASH_ATTR lpm_enter();
uint32 ICACHE_FLASH_ATTR lpm_leave();
int8 lpm_sleep(lpm_cb_t func,uint32 us);
bool ICACHE_FLASH_ATTR fwup_upgrading();
void ICACHE_FLASH_ATTR task_init(int qlen);
bool ICACHE_FLASH_ATTR task_post(void* f,void* p);
void ICACHE_FLASH_ATTR uart1_write_char(char c);
void ICACHE_FLASH_ATTR uart0_write_char(char c);
#endif //UTILS_H_
