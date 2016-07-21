/**
* \file
*		Ring Buffer library
*/

#include "driver/ringbuf.h"

#include"osapi.h"
#include"mem.h"
ringbuf_t* ICACHE_FLASH_ATTR rbnew(int32_t size)
{
	ringbuf_t* r;
	uint8_t* p=(uint8_t*)os_malloc(size+sizeof(ringbuf_t));
	r=(ringbuf_t*)p;
	rbinit(r,p+sizeof(ringbuf_t),size);
	return r;
}
void ICACHE_FLASH_ATTR rbdel(ringbuf_t* r)
{
	os_free(r);
}
/**
* \brief init a ringbuf_t object
* \param r pointer to a ringbuf_t object
* \param buf pointer to a byte array
* \param size size of buf
* \return 0 if successfull, otherwise failed
*/
int16_t ICACHE_FLASH_ATTR rbinit(ringbuf_t *r, void* p, int32_t size)
{
	if(r == NULL || p == NULL || size < 2) return -1;
	
	r->p_r = r->p_w = r->p_o =(uint8_t*)p;
	r->cnt = 0;
	r->size = size;
	
	return 0;
}
int32_t ICACHE_FLASH_ATTR rbdrain(ringbuf_t *r)
{
	int32_t ret=r->cnt;
	r->p_r = r->p_w = r->p_o;
	r->cnt = 0;
	return ret;
}
/**
* \brief put a character into ring buffer
* \param r pointer to a ringbuf object
* \param c character to be put
* \return 0 if successfull, otherwise failed
*/
int32_t ICACHE_FLASH_ATTR rbputc(ringbuf_t *r, uint8_t c)
{
	if(r->cnt>=r->size)
	{
		os_printf("BUF FULL\n");
		return 0;
	}
	r->cnt++;		// increase filled slots count, this should be atomic operation
	*r->p_w++ = c;	// put character into buffer
	
	if(r->p_w >= r->p_o + r->size)			// rollback if write pointer go pass
		r->p_w = r->p_o;					// the physical boundary	
	return 1;
}
int32_t ICACHE_FLASH_ATTR rbput(ringbuf_t *r,const void* p,int32_t len)
{
	int32_t l,t;
	uint8_t* c=(uint8_t*)p;
	t=r->size-r->cnt;
	if(len>t) len=t;
	l=r->p_o+r->size-r->p_w;
	r->cnt+=len;
	if(len<l)
	{
		os_memcpy(r->p_w,c,len);
		r->p_w+=len;
	}
	else if(len==l)
	{
		os_memcpy(r->p_w,c,len);
		r->p_w=r->p_o;
	}
	else if(len>l)
	{
		os_memcpy(r->p_w,c,l);
		os_memcpy(r->p_o,c+l,len-l);
		r->p_w=r->p_o+len-l;
	}
	return len;
}
int32_t ICACHE_FLASH_ATTR rbctnlen(ringbuf_t* r)
{
	if(r->p_r>r->p_w) return r->p_o+r->size-r->p_r;
	else return r->cnt;
}
int32_t ICACHE_FLASH_ATTR rbfwd(ringbuf_t* r,int32_t len)
{
	if(len>r->cnt) len=r->cnt;
	int32_t ret,d=r->p_o+r->size-r->p_r-len;
	r->cnt-=len;
	ret=r->cnt;
	if(d<=0)
	{
		r->p_r=r->p_o-d;
	}
	else if(d<r->cnt)
	{
		r->p_r+=len;
		ret=d;
	}
	else
	{
		r->p_r+=len;
	}
	return ret;
}
/**
* \brief get a character from ring buffer
* \param r pointer to a ringbuf object
* \param c read character
* \return 0 if successfull, otherwise failed
*/
int32_t ICACHE_FLASH_ATTR rbget(ringbuf_t *r, void* p,int32_t len)
{
	if(len>r->cnt) len=r->cnt;
	int32_t d=r->p_o+r->size-r->p_r-len;
	r->cnt-=len;
	if(d<=0)
	{
		if(p)
		{
			os_memcpy(p,r->p_r,d+len);
			if(d<0) os_memcpy(p,r->p_r,-d);
		}
		r->p_r=r->p_o-d;
	}
	else
	{
		if(p) os_memcpy(p,r->p_r,len);
		r->p_r+=len;		
	}
	return len;
}
uint8_t ICACHE_FLASH_ATTR rbchar(ringbuf_t *r,uint8_t def)
{
	uint8_t ret=def;
	if(r!=0&&r->cnt>0)
	{
		r->cnt--;
		ret=*r->p_r++;
		if(r->p_r >= r->p_o + r->size) r->p_r = r->p_o;		
	}
	return ret;
}
bool ICACHE_FLASH_ATTR rbgetc(ringbuf_t *r,void* p)
{
	uint8_t* c=(uint8_t*)p;
	if(r!=0&&r->cnt>0)
	{
		if(c) *c=*r->p_r;
		r->cnt--;
		r->p_r++;
		if(r->p_r >= r->p_o+r->size) r->p_r = r->p_o;
		return true;
	}
	return false;
}
