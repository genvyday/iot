#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <os_type.h>
#include <stdlib.h>
#include <c_types.h>
typedef struct{
	uint8_t* p_o;				/**< Original pointer */
	uint8_t* volatile p_r;		/**< Read pointer */
	uint8_t* volatile p_w;		/**< Write pointer */
	volatile int32_t cnt;	    /**< Number of filled slots */
	int32_t size;				/**< Buffer size */
}ringbuf_t;

//api
#define rbptr(r) ((r)->p_o+0)
#define rbsiz(r) ((r)->size+0)
#define rblen(r) ((r)->cnt+0)   //return data length
#define rbdat(r) ((r)->p_r+0)
#define rbgap(r) ((r)->size-(r)->cnt)
int16_t ICACHE_FLASH_ATTR rbinit(ringbuf_t *r, void* buf, int32_t size);
int32_t ICACHE_FLASH_ATTR rbdrain(ringbuf_t *r);
int32_t ICACHE_FLASH_ATTR rbputc(ringbuf_t *r, uint8_t c);
bool    ICACHE_FLASH_ATTR rbgetc(ringbuf_t *r, void* p);
int32_t ICACHE_FLASH_ATTR rbget(ringbuf_t *r, void* p,int32_t len);
int32_t ICACHE_FLASH_ATTR rbput(ringbuf_t *r, const void* p,int32_t len);
int32_t ICACHE_FLASH_ATTR rbctnlen(ringbuf_t* r); //return continues data length
int32_t ICACHE_FLASH_ATTR rbfwd(ringbuf_t* r,int32_t len); //return continues data length
uint8_t ICACHE_FLASH_ATTR rbchar(ringbuf_t *r,uint8_t def);
ringbuf_t* ICACHE_FLASH_ATTR rbnew(int32_t size);
void       ICACHE_FLASH_ATTR rbdel(ringbuf_t* r);
#endif
