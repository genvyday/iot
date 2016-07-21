#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "at_custom.h"
#include "driver/ringbuf.h"

#define AE(a1,a2,i) ((a1)[i]==(a2)[i])
#define IPEQ(ip1,ip2) (AE(ip1,ip2,0)&&AE(ip1,ip2,1)&&AE(ip1,ip2,2)&&AE(ip1,ip2,3))
#define CE(a1,a2,i) (a1)[i]=(a2)[i]
#define IPCP(ip1,ip2) CE(ip1,ip2,0);CE(ip1,ip2,1);CE(ip1,ip2,2);CE(ip1,ip2,3)
#define RADDR(c) (c)->proto.tcp->remote_ip
#define RPORT(c) (c)->proto.tcp->remote_port
#define SAMECON(c1,c2) (RPORT(c1)==RPORT(c2)&&IPEQ(RADDR(c1),RADDR(c2)))
#define cmdsendstr(con,s) net_send((con), s, os_strlen(s))
#define CFLGGET(c,i) (((c)->flags&(1<<(i)))>>i)
#define CFLGSET(c,i) ((c)->flags=((c)->flags|(1<<(i))))
#define CFLGCLR(c,i) ((c)->flags=((c)->flags&~(1<<(i))))
#define CMDNUM 20
#define ARGNUM 20
#define BUFLEN 1023
void ets_write_char(char c);
void ICACHE_FLASH_ATTR cmdsvr_write_char(char c);
typedef int (*cmdfunc_t)(int argc,char**argv);
typedef unsigned short ushort;
typedef struct cmdata
{
	struct espconn* con;
	ushort flags;
	ushort lnidx;
	ushort datlen;
	ushort buflen;
	char   rcvdat[BUFLEN+1];
}cmdata;
typedef struct cmdrcd
{
	char* cmdkey;
	cmdfunc_t cmdfnc;
	uint32 flags;
}cmdrcd;

LOCAL int ICACHE_FLASH_ATTR stridx(char* s,char c)
{
	int i=0;
	while(s[i]!=0)
	{
		if(s[i]==c) return i;
		++i;
	}
	return -1;
}
LOCAL int ICACHE_FLASH_ATTR strcmpz(char* s1,char* s2,int len)
{
	int i=0;
	while(s1[i]==s2[i]&&s1[i]!=0&&(i<len||len<0)) ++i;
	return len==i?0:(s1[i]-s2[i]);
}
int32 ICACHE_FLASH_ATTR str2int(char* s)
{
	int32 r=0;
	int neg=0;
	if(*s=='-')
	{
		neg=1;
		++s;
	}
	while(*s!=0)
	{
		r=r*10+*s-'0';
	}
	if(neg) r=-r;
	return r;
}
#define ISBLANK(c) ((c)==' '||(c)=='\r'||(c)=='\n')
#define ISEND(c) ((c)==0)
LOCAL int ICACHE_FLASH_ATTR str2argv(char* s,char** argv,int max)
{
	char* p=s;
	int c=0;
	while(!ISEND(*s)&&c<max)
	{
		while(ISBLANK(*s)) ++s;
		argv[c]=s;
		p=s;
		++c;
		while(!ISEND(*s))
		{
			if(ISBLANK(*s))
			{
				*p=0;
				++s;
				break;
			}
			else if(*s=='\\')
			{
				++s;
				if(*s=='r') *s='\r';
				else if(*s=='n') *s='\n';
				else if(*s=='\\') *s='\\';
				else if(*s==' ') *s=' ';
				else --s;
			}
			*p=*s;
			p++;
			s++;
		}
	}
	*p=0;
	if(c>0&&argv[c-1][0]==0) --c;
	return c;
}
typedef struct
{
	ringbuf_t rb;
	char sndata[BUFLEN];
	u8 sending;
	u8 closing;
}netsnd_t;
LOCAL netsnd_t netsnd;
int ICACHE_FLASH_ATTR net_try_tx(struct espconn* con,netsnd_t* ns)
{
	int ret=0;
	int len=0;
	if(ns->sending==1) return ESPCONN_MAXNUM;
	len=rbctnlen(&ns->rb);
	if(len>0)
	{
		ret=espconn_send(con,rbdat(&ns->rb),len);
		//os_printf("esp send ret %d %d\n",len,ret);
		if(ret==0)
		{
			rbfwd(&ns->rb,len);
			ns->sending=1;
		}
	}
	return ret;
}
int ICACHE_FLASH_ATTR net_fin_tx(struct espconn* con,netsnd_t* ns)
{
	ns->sending=0;
	if(rblen(&ns->rb)>0) net_try_tx(con,ns);
	return 0;
}
int ICACHE_FLASH_ATTR net_clox(struct espconn* con,netsnd_t* ns)
{
	ns->closing=1;
}
int ICACHE_FLASH_ATTR net_send(struct espconn* con,void* pdt,int len)
{
	int ret=rbput(&netsnd.rb,pdt,len);
	//os_printf("ring buf len=%d\n",rblen(&netsnd.rb));
	if(ret==-1) return -1;
	if(netsnd.sending==0) net_try_tx(con,&netsnd);
	return 0;
}
LOCAL struct espconn esp_conn;
LOCAL esp_tcp esptcp;
LOCAL cmdata cmdat;
LOCAL cmdata* cmdt=&cmdat;
void ICACHE_FLASH_ATTR fwup_begin(struct espconn *pespconn,const char* ip);
#define _INTSIZEOF(n) ((sizeof(n)+sizeof(char*)-1)&~(sizeof(char*)-1)) 
void ICACHE_FLASH_ATTR term_sndstr(char*fmt,...)
{
	char buf[1024];
	if(cmdt->con==0||CFLGGET(cmdt,0)==0) return ;
	ets_vsnprintf(buf,1023,fmt,((char*)&fmt)-_INTSIZEOF(fmt));
	buf[1023]=0;
	cmdsendstr(cmdt->con,buf);
}
LOCAL int ICACHE_FLASH_ATTR cmdsvr_embed(int argc,char** argv)
{
	int i;
	if(strcmpz(argv[0],"echo",-1)==0)
	{
		for(i=1;i<argc;i++)
		{
			if(i!=1) term_sndstr(" ");
			term_sndstr(argv[i]);
		}
		term_sndstr(" test %s %s","hello","world");
	}
	else if(strcmpz(argv[0],"exit",-1)==0)
	{
		net_clox(cmdt->con,&netsnd);
		cmdsendstr(cmdt->con,"ok");
	}
	else if(strcmpz(argv[0],"stdout",-1)==0)
	{
		if(argc>1&&strcmpz(argv[1],"off",-1)==0) os_install_putc1(0);
		else os_install_putc1((void *)cmdsvr_write_char);
		cmdsendstr(cmdt->con,"stdout redirect ok ");
		os_printf("%s",">>this is print out");
	}
	else if(strcmpz(argv[0],"upgrade",-1)==0)
	{
		char* ip="192.168.1.50:5980";
		if(argc>1) ip=argv[1];
		cmdsendstr(cmdt->con,"start to upgrade");
		fwup_begin(0,ip);
	}
	return 0;
}
void ICACHE_FLASH_ATTR cmdsvr_attx(const uint8 *data,uint32 length)
{
#ifdef EN_AT_FAKE
	if(CFLGGET(cmdt,1)&&CFLGGET(cmdt,0)) espconn_sent(&cmdt->con,(void*)data,length);
	else at_fake_uart_enable(false,0);
#endif
}
LOCAL int ICACHE_FLASH_ATTR cmdsvr_atfake(int argc,char** argv)
{
#ifdef EN_AT_FAKE
	int en=0;
	if(argc<1) en=1;
	else if(strcmpz(argv[1],"on",-1)) en=1;
	if(en==1)
	{
		CFLGSET(cmdt,1);
		at_fake_uart_enable(true,cmdsvr_attx);
	}
	else CFLGCLR(cmdt,1);
#endif
	return 0;
}
//os_install_putc1((void *)uart1_write_char);
//os_install_putc1(uart0_write_char);
void ICACHE_FLASH_ATTR cmdsvr_write_char(char c)
{
	if(CFLGGET(cmdt,0)==1&&cmdt->con!=0)
	{
		net_send(cmdt->con,&c,1);
	}
	else
	{
		os_install_putc1(0);
		os_printf("%c",c);
	}
}
/******************************************************************************
 * FunctionName : cmdsvr_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
cmdsvr_sent_cb(void *arg)
{
	//data sent successfully
	//os_printf("tcp sent cb \r\n");
	net_fin_tx(cmdt->con,&netsnd);
}
LOCAL cmdrcd cmdreg[]=
{
	{"echo",cmdsvr_embed,0},
	{"exit",cmdsvr_embed,0},
	{"stdout",cmdsvr_embed,0},
	{"upgrade",cmdsvr_embed,0},
#ifdef EN_AT_FAKE
	{"atfack",cmdsvr_atfake,0},
#endif
	{"",0,0}
};
void cmdsvr_execmd(struct espconn *con,cmdata* cmdt)
{
	int argc=0,i=0;
	char* argv[ARGNUM];
	cmdt->con=con;
	argc=str2argv(cmdt->rcvdat,argv,ARGNUM);
	if(argc!=0)
	{
		//os_printf("exec cmd %s on %08x\r\n",argv[0],con);
		while(cmdreg[i].cmdfnc!=0)
		{
			if(strcmpz(cmdreg[i].cmdkey,argv[0],-1)==0)
			{
				(*cmdreg[i].cmdfnc)(argc,argv);
				break;
			}
			++i;
		}
	}
	if(cmdreg[i].cmdfnc==0) cmdsendstr(con,"unknow command");
	cmdsendstr(con,"\r\n>");
}
/******************************************************************************
 * FunctionName : cmdsvr_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
cmdsvr_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	//received some data from tcp connection
	struct espconn *con = arg;
	int tmp=-1;
	ushort len;
	//if(!SAMECON(con,cmdt->con))
	//{
		//os_memcpy(&cmdt->con,con,sizeof(cmdt->con));
		//cmdt->datlen=0;
		//cmdt->lnidx=0;
	//}
	//os_printf("tcp recv : %s \r\n", pusrdata);
	len=cmdt->buflen-cmdt->datlen;
	if(length>len)
	{
		cmdsendstr(con,"command too long\n");
		cmdt->datlen=0;
		cmdt->lnidx=0;
	}
	else
	{
		//cmdsendstr(con,"data recv\n");
		os_memcpy(cmdt->rcvdat+cmdt->datlen,pusrdata,length);
		cmdt->datlen+=length;
		cmdt->rcvdat[cmdt->datlen]=0;
		tmp=stridx(cmdt->rcvdat+cmdt->lnidx,'\n');
		if(tmp!=-1) cmdt->lnidx+=tmp; else cmdt->lnidx=cmdt->datlen;
	}
	if(tmp!=-1)
	{
		cmdt->rcvdat[cmdt->lnidx]=0;
		if(CFLGGET(cmdt,1)&&strcmpz(cmdt->rcvdat,"AT+",3)==0)
		{
			#ifdef EN_AT_FAKE
			at_fake_uart_rx((uint8*)cmdt->rcvdat,cmdt->lnidx);
			#endif
		}
		else
		{
			cmdsvr_execmd(con,cmdt);
		}
		cmdt->datlen=cmdt->datlen-cmdt->lnidx-1;
		cmdt->lnidx=0;
		os_memcpy(cmdt->rcvdat,cmdt->rcvdat+cmdt->lnidx+1,cmdt->datlen+1);
	}
}

/******************************************************************************
 * FunctionName : cmdsvr_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
cmdsvr_discon_cb(void *arg)
{
	//tcp disconnect successfully
	CFLGCLR(cmdt,0);
    os_printf("tcp disconnect succeed !!! \r\n");
}

/******************************************************************************
 * FunctionName : cmdsvr_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
cmdsvr_recon_cb(void *arg, sint8 err)
{
	//error occured , tcp connection broke.
	CFLGCLR(cmdt,0);
	os_printf("reconnect callback, error code %d !!! \r\n",err);
}

LOCAL void cmdsvr_multi_send(void)
{
	struct espconn *con = &esp_conn;
	remot_info *premot = NULL;
	uint8 count = 0;
	sint8 value = ESPCONN_OK;
	if (espconn_get_connection_info(con,&premot,0) == ESPCONN_OK)
	{
		char *pbuf = "cmdsvr_multi_send\n";
		for (count = 0; count < con->link_cnt; count ++)
		{
			con->proto.tcp->remote_port = premot[count].remote_port;
			IPCP(con->proto.tcp->remote_ip,premot[count].remote_ip);
			espconn_sent(con, pbuf, os_strlen(pbuf));
		}
   }
}

/******************************************************************************
 * FunctionName : cmdsvr_listen
 * Description  : CMD server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
cmdsvr_listen(void *arg)
{
    struct espconn *con = arg;
    os_printf("cmdsvr_listen %08x!!! \r\n",con);
	espconn_set_opt(con,ESPCONN_KEEPALIVE);
	
	cmdt->datlen=0;
	cmdt->lnidx=0;
	cmdt->buflen=BUFLEN;
	cmdt->con=con;
	CFLGSET(cmdt,0);
	//os_install_putc1((void *)cmdsvr_write_char);
    espconn_regist_recvcb(con, cmdsvr_recv_cb);
    espconn_regist_reconcb(con, cmdsvr_recon_cb);
    espconn_regist_disconcb(con, cmdsvr_discon_cb);
    espconn_regist_sentcb(con, cmdsvr_sent_cb);
	uint8 ub=system_upgrade_userbin_check();
	char* bin=(ub==0?" bin1":(ub==1?" bin2":" non"));
	char buf[128];
	cmdsendstr(cmdt->con,"Build Time: "__DATE__" "__TIME__);
	os_sprintf(buf,"%s %d %d\r\n>",bin,sizeof(int),sizeof(void*));
	cmdsendstr(cmdt->con,buf);
	//cmdsendstr(cmdt->con,"\r\n>");
}

/******************************************************************************
 * FunctionName : cmdsvr_init
 * Description  : parameter initialize as a CMD server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR cmdsvr_init(uint32 port)
{
	rbinit(&netsnd.rb,(uint8_t*)netsnd.sndata,BUFLEN);
    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&esp_conn, cmdsvr_listen);

    sint8 ret = espconn_accept(&esp_conn);
    
    os_printf("espconn_accept [%d] !!! \r\n", ret);

}
/*************************************end**************************************/
void ICACHE_FLASH_ATTR cmdsvr_shutdown()
{
	espconn_accept(&esp_conn);
	espconn_delete(&esp_conn);
}
