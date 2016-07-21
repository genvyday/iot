/******************************************************************************
 * Copyright 2013-2014 Espressif Systems
 *
 * FileName: uart.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/9/24, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "driver/utils.h"
#include "espconn.h"
#include "driver/ringbuf.h"
#define FUNC_U1TXD_BK                   2
#define FUNC_U0CTS                             4
extern UartDevice    UartDev;
static ringbuf_t* pTxBuf;
static ringbuf_t* pRxBuf;
void   rx_buff_enq(ringbuf_t* r);

LOCAL void uart0_rx_intr_handler(void *para);
LOCAL task_func_t rx_func;
LOCAL task_func_t tx_cmpl;
void uart1_send_no_wait(const char *str);
void tx_start_uart_buffer(uint8 uart_no);
void uart_rx_intr_disable(uint8 uart_no);
void uart_rx_intr_enable(uint8 uart_no);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
    if (uart_no == UART1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    } else {
        /* rcv_buff size if 0x100 */
        ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0CTS);
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTCK_U);
    }

    uart_div_modify(uart_no, (uint16)(UART_CLK_FREQ / (uint32)(UartDev.baut_rate)));

    WRITE_PERI_REG(UART_CONF0(uart_no), (uint32)UartDev.exist_parity
            | (uint32)UartDev.parity
            | (((uint8)UartDev.stop_bits) << UART_STOP_BIT_NUM_S)
            | (((uint8)UartDev.data_bits) << UART_BIT_NUM_S));

    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

    if (uart_no == UART0) {
        //set rx fifo trigger
        WRITE_PERI_REG(UART_CONF1(uart_no),
                ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                ((110 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
                UART_RX_FLOW_EN |
                (0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
                UART_RX_TOUT_EN |

                ((0x10 & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S)); //wjl
                //SET_PERI_REG_MASK( UART_CONF0(uart_no),UART_TX_FLOW_EN);  //add this sentense to add a tx flow control via MTCK( CTS )

        SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_TOUT_INT_ENA |
                UART_FRM_ERR_INT_ENA);
    } else {
        WRITE_PERI_REG(UART_CONF1(uart_no),
                ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));
    }

    //clear all interrupt
    WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
    //enable rx_interrupt
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);
}

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
 *******************************************************************************/
LOCAL void uart0_rx_intr_handler(void *para)
{
    uint8 uart_no = UART0;
    if (UART_FRM_ERR_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_FRM_ERR_INT_ST)) {
        uart1_send_no_wait("FRM_ERR\r\n");
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
    }

    if (UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST)) {
        uart_rx_intr_disable(uart_no);
        rx_buff_enq(pRxBuf);
    } else if (UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_TOUT_INT_ST)) {
        uart_rx_intr_disable(uart_no);
        rx_buff_enq(pRxBuf);
    } else if (UART_TXFIFO_EMPTY_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_TXFIFO_EMPTY_INT_ST)) {
        CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_TXFIFO_EMPTY_INT_ENA);
        tx_start_uart_buffer(uart_no);
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
   } else if (UART_RXFIFO_OVF_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_OVF_INT_ST)) {
        uart1_send_no_wait("RX OVF!\r\n");
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_OVF_INT_CLR);
    }
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
 *******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br)
{
    UartDev.baut_rate = uart0_br;
    uart_config(UART0);
    UartDev.baut_rate = BIT_RATE_115200;
    uart_config(UART1);
    ETS_UART_INTR_ENABLE();

    os_install_putc1(uart1_write_char);

    pTxBuf = rbnew(UART_TX_BUFFER_SIZE);
    pRxBuf = rbnew(UART_RX_BUFFER_SIZE);
}

/******************************************************************************
 * FunctionName : uart_tx_one_char_no_wait
 * Description  : uart tx a single char without waiting for fifo
 * Parameters   : uint8 uart - uart port
 *                uint8 TxChar - char to tx
 * Returns      : STATUS
 *******************************************************************************/
STATUS
uart_tx_one_char_no_wait(uint8 uart, uint8 TxChar)
{
    uint8 fifo_cnt = ((READ_PERI_REG(UART_STATUS(uart)) >> UART_TXFIFO_CNT_S)& UART_TXFIFO_CNT);

    if (fifo_cnt < 126) {
        WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    }

    return OK;
}

/******************************************************************************
 * FunctionName : uart1_send_no_wait
 * Description  : uart tx a string without waiting for every char, used for print debug info which can be lost
 * Parameters   : const char *str - string to be sent
 * Returns      : NONE
 *******************************************************************************/
void
uart1_send_no_wait(const char *str)
{
    if(str == NULL) {
        return;
    }

    while (*str) {
        (void)uart_tx_one_char_no_wait(UART1, (uint8)*str);
        str++;
    }
}



void rx_buff_enq(ringbuf_t* r)
{
    uint8 fifo_len = 0, buf_idx = 0,loop;
    ETSParam par = 0;
    uint8 c;
    fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;

    while (fifo_len--)
	{
        c = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF; // discard data
		rbputc(r,c);
    }

    if(rx_func!=0&&task_post(rx_func,r) != TRUE) {
        os_printf("post fail!!!\n\r");
    }
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
    uart_rx_intr_enable(UART0);
}

void ICACHE_FLASH_ATTR tx_buff_enq(const char *pdata, int32_t dtlen, bool force)
{
	int32_t lw=0;
    CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
    //DBG2("len:%d\n\r",dtlen);
	while(force&&rbgap(pTxBuf)<dtlen-lw)
	{
		lw+=rbput(pTxBuf,pdata+lw,dtlen-lw);
		tx_start_uart_buffer(UART0);
		CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
		ets_delay_us(70);
		WRITE_PERI_REG(0X60000914, 0x73);
	}
	if(rbgap(pTxBuf)<dtlen-lw) rbput(pTxBuf,pdata+lw,dtlen-lw);
	//if(rbgap(pTxBuf)) URAT_TX_LOWER_SIZE
    SET_PERI_REG_MASK(UART_CONF1(UART0), (UART_TX_EMPTY_THRESH_VAL & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S);
    SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
}

//--------------------------------
LOCAL void tx_fifo_insert(ringbuf_t* r,int32_t len,uint8 uartno)
{
    int32_t i;
    for (i = 0;i<len;i++) WRITE_PERI_REG(UART_FIFO(uartno),rbchar(r,0));
}

/******************************************************************************
 * FunctionName : tx_start_uart_buffer
 * Description  : get data from the tx buffer and fill the uart tx fifo, co-work with the uart fifo empty interrupt
 * Parameters   : uint8 uart_no - uart port num
 * Returns      : NONE
 *******************************************************************************/
void tx_start_uart_buffer(uint8 uart_no)
{
    uint8 tx_fifo_len = (READ_PERI_REG(UART_STATUS(uart_no)) >> UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT;
    uint8 fifo_remain = UART_FIFO_LEN - tx_fifo_len ;
    int32_t len_tmp;
    uint32 data_len;

    if (pTxBuf) {
        data_len = rblen(pTxBuf);

        if (data_len > fifo_remain) {
            len_tmp = fifo_remain;
            tx_fifo_insert(pTxBuf, len_tmp, uart_no);
            SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
        } else {
            len_tmp = (uint8)data_len; // THIS OK
            tx_fifo_insert(pTxBuf, len_tmp, uart_no);

        }
        if (rbgap(pTxBuf) >= 0)
		{
            task_post(tx_cmpl,pTxBuf);
        }
    }
}

void uart_rx_intr_disable(uint8 uart_no)
{
    CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
}

void uart_rx_intr_enable(uint8 uart_no)
{
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
}

/*
void ICACHE_FLASH_ATTR
set_tcp_block(void)
{
    struct espconn * trans_conn = (struct espconn *)get_trans_conn();
    if(trans_conn == NULL) {

    } else if (trans_conn->type == ESPCONN_TCP) {
        DBG1("TCP BLOCK\n\r");
        (void)espconn_recv_hold(trans_conn);
        DBG2("b space: %d\n\r", pTxBuffer->Space);
    } else {

    }
}

void ICACHE_FLASH_ATTR
clr_tcp_block(void)
{
    struct espconn * trans_conn = (struct espconn *)get_trans_conn();
    if(trans_conn == NULL) {

    } else if (trans_conn->type == ESPCONN_TCP) {
        DBG1("TCP recover\n\r");
        (void)espconn_recv_unhold(trans_conn);
        DBG2("r space: %d\n\r", pTxBuffer->Space);
    } else {

    }

}
*/
//========================================================
