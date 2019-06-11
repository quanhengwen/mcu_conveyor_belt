/*
 *  Author: Kaka Xie
 *  brief: sanwei rfid
 */

#include "sanwei_rfid.h"
#include "usart.h"
#include <string.h>

//static uint8_t pre_send_data_buf[PRE_SEND_DATA_LEN] = {0x02,0x04,0x05,0x02,0x03,0x10,0x03};
static uint8_t pre_send_data_buf[PRE_SEND_DATA_LEN] = {0};
static uint8_t send_data_buf[SEND_DATA_LEN] = {0};


#define PROTOCOL_HEAD           0x02
#define PROTOCOL_TAIL           0x03
#define INSERT_ESCAPE           0x10


OS_MEM *sw_rfid_uart_rcv_mem_handle;

void sanwei_rfid_init(void)
{
    uart2_dma_init(19200);
}

static sw_rfid_uart_rcv_buf_t *rcv_buf_head = NULL;

int sw_rfid_uart_rcv_buf_head_init(void)
{
    uint8_t err = 0;
    rcv_buf_head = (sw_rfid_uart_rcv_buf_t *)OSMemGet(sw_rfid_uart_rcv_mem_handle, &err);
    if(rcv_buf_head != NULL)
    {
        rcv_buf_head->next = NULL;
        rcv_buf_head->rcv_len = 0;
        memset(rcv_buf_head->rcv_buf, 0, SW_RFID_UART_RCV_SIZE);
    }
    else
    {
        /*
        todo: err process
        */
        return -1;
    }
    return 0;
}

static uint16_t get_used_buf_size(void)
{
    uint16_t cnt = 0;
    sw_rfid_uart_rcv_buf_t *ptr = NULL;
    ptr = rcv_buf_head;
    while(ptr->next != NULL)
    {
        ptr = ptr->next;
        cnt++;
    }
    return cnt;
}

int put_sw_rfid_uart_rcv_buf(uint8_t *buf, uint16_t len)
{
    sw_rfid_uart_rcv_buf_t *p = NULL;
    sw_rfid_uart_rcv_buf_t *node = NULL;
    uint8_t err = 0;
    if(get_used_buf_size() < SW_RFID_UART_RCV_BUF_NUM)
    {
        node = (sw_rfid_uart_rcv_buf_t *)OSMemGet(sw_rfid_uart_rcv_mem_handle, &err);
        if(node == NULL)
        {
            /*
            todo: err process
            */
            return -1;
        }
        node->next = NULL;
        node->rcv_len = len;
        memcpy(node->rcv_buf, buf, len);
    }
    else
    {
        /*
        todo: err process
        */
        return -1;
    }
    p = rcv_buf_head;
    while(p->next != NULL)  //链表头不保存数据
    {
        p = p->next;
    }
    p->next = node;
    return 0;
}

int free_one_rcv_buf(sw_rfid_uart_rcv_buf_t *buf)
{
    sw_rfid_uart_rcv_buf_t *p = NULL;
    sw_rfid_uart_rcv_buf_t *pre_p = NULL;
    p = rcv_buf_head;
    pre_p = rcv_buf_head;
    do
    {
        if(p != buf)
        {
            pre_p = p;
            p = p->next;
        }
        else
        {
            break;
        }
    }
    while(p != NULL);

    if(p == NULL)
    {
        return -1;  //can not find such buf
    }

    OSMemPut(sw_rfid_uart_rcv_mem_handle, p);
    if(p->next == NULL)
    {
        pre_p->next = NULL;
    }
    else
    {
        pre_p->next = p->next;
    }
    return 0;
}

sw_rfid_uart_rcv_buf_t *get_latest_buf(void)
{
    sw_rfid_uart_rcv_buf_t *p = NULL;
    sw_rfid_uart_rcv_buf_t *node = NULL;
    p = rcv_buf_head;
    if(p->next != NULL)
    {
        node = p->next;
    }
    return node;
}



static bool is_escape(uint8_t c)
{
    if((c == PROTOCOL_HEAD) || (c == PROTOCOL_TAIL) || (c == INSERT_ESCAPE))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static uint8_t cal_check_value(uint8_t *data, uint8_t len)
{
    uint8_t sum = 0;
    for(uint8_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return sum;
}

static uint8_t pre_proc_data(uint8_t *in, uint8_t in_len, uint8_t *out)
{
    uint8_t out_len = 0;
    uint8_t cnt = 1;
    uint8_t i = 1;
    if((in[0] == PROTOCOL_HEAD) && (in[in_len - 1] == PROTOCOL_TAIL))
    {
        out[0] = in[0];
        out_len = in_len;
        for(i = 1; i < in_len - 1; i++, cnt++)
        {
            if(is_escape(in[i]))
            {
                out[cnt++] = INSERT_ESCAPE;
                out[cnt] = in[i];
            }
            else
            {
                out[cnt] = in[i];
            }
        }
        out[cnt] = in[i];
        out_len = cnt + 1;
        return out_len;
    }
    else    //输入参数有问题
    {
        return 0;
    }

}

void test_sanwei_rfid_send_data(void)
{
    uint8_t len = pre_proc_data(pre_send_data_buf, 7, send_data_buf);
    uart2_send(send_data_buf, len);
}



/******************** uart send protocol *****************/
uint8_t set_rfid_work_mode(uint8_t mode)    //02 00 00 04 3a 41 7f 03           ack: 02 00 00 10 03 3A 00 3D 03 
{
    uint8_t check_value = 0;
    uint8_t len = 0;
    pre_send_data_buf[0] = PROTOCOL_HEAD;
    pre_send_data_buf[1] = 0;       //模块地址
    pre_send_data_buf[2] = 0;       //模块地址
    pre_send_data_buf[3] = 4;       //length:从长度字到校验字的字节数
    pre_send_data_buf[4] = 0x3a;
    /* data */
    pre_send_data_buf[5] = 0x41;    //'A'

    check_value = cal_check_value(&pre_send_data_buf[1], 5);    //从模块地址到数据域结束
    pre_send_data_buf[6] = check_value;         //校验字
    pre_send_data_buf[7] = PROTOCOL_TAIL;

    len = pre_proc_data(pre_send_data_buf, 8, send_data_buf);
    if(len > 0)
    {
        uart2_send(send_data_buf, len);
    }
    return 0;
}


uint8_t search_card(void)      //02 00 00 04 46 52 9c 03       ack:02 00 00 05 46 00 04 00 4F 03 
{
    uint8_t check_value = 0;
    uint8_t len = 0;
    pre_send_data_buf[0] = PROTOCOL_HEAD;
    pre_send_data_buf[1] = 0;       //模块地址
    pre_send_data_buf[2] = 0;       //模块地址
    pre_send_data_buf[3] = 4;       //length:从长度字到校验字的字节数
    pre_send_data_buf[4] = 0x46;
    /* data */
    pre_send_data_buf[5] = 0x52;

    check_value = cal_check_value(&pre_send_data_buf[1], 5);    //从模块地址到数据域结束
    pre_send_data_buf[6] = check_value;         //校验字
    pre_send_data_buf[7] = PROTOCOL_TAIL;

    len = pre_proc_data(pre_send_data_buf, 8, send_data_buf);
    if(len > 0)
    {
        uart2_send(send_data_buf, len);
    }
    return 0;
}

uint8_t prevent_conflict(void)      //02 00 00 04 47 04 4f 03    ack:02 00 00 07 47 00 12 3C 56 56 48 03 
{
    uint8_t check_value = 0;
    uint8_t len = 0;
    pre_send_data_buf[0] = PROTOCOL_HEAD;
    pre_send_data_buf[1] = 0;       //模块地址
    pre_send_data_buf[2] = 0;       //模块地址
    pre_send_data_buf[3] = 4;       //length:从长度字到校验字的字节数
    pre_send_data_buf[4] = 0x47;
    /* data */
    pre_send_data_buf[5] = 0x04;

    check_value = cal_check_value(&pre_send_data_buf[1], 5);    //从模块地址到数据域结束
    pre_send_data_buf[6] = check_value;         //校验字
    pre_send_data_buf[7] = PROTOCOL_TAIL;

    len = pre_proc_data(pre_send_data_buf, 8, send_data_buf);
    if(len > 0)
    {
        uart2_send(send_data_buf, len);
    }
    return 0;
}


uint8_t select_card(uint32_t id)        //02 00 00 07 48 12 3c 56 56 46 03          ack:02 00 00 04 48 00 08 54 03 
{
    uint8_t check_value = 0;
    uint8_t len = 0;
    pre_send_data_buf[0] = PROTOCOL_HEAD;
    pre_send_data_buf[1] = 0;       //模块地址
    pre_send_data_buf[2] = 0;       //模块地址
    pre_send_data_buf[3] = 4;       //length:从长度字到校验字的字节数
    pre_send_data_buf[4] = 0x48;
    /* data */
    pre_send_data_buf[5] = 0x12;
    pre_send_data_buf[6] = 0x3c;
    pre_send_data_buf[7] = 0x56;
    pre_send_data_buf[8] = 0x56;

    check_value = cal_check_value(&pre_send_data_buf[1], 8);    //从模块地址到数据域结束
    pre_send_data_buf[9] = check_value;         //校验字
    pre_send_data_buf[10] = PROTOCOL_TAIL;

    len = pre_proc_data(pre_send_data_buf, 11, send_data_buf);
    if(len > 0)
    {
        uart2_send(send_data_buf, len);
    }
    return 0;
}

uint8_t verify_secret_key(uint8_t *key, uint8_t key_len)        //02 00 00 0b 4A 60 34 ff ff ff ff ff ff e3 03          ack:02 00 00 10 03 4A 00 4D 03 
{
    uint8_t check_value = 0;
    uint8_t len = 0;
    pre_send_data_buf[0] = PROTOCOL_HEAD;
    pre_send_data_buf[1] = 0;       //模块地址
    pre_send_data_buf[2] = 0;       //模块地址
    pre_send_data_buf[3] = 11;       //length:从长度字到校验字的字节数
    pre_send_data_buf[4] = 0x4A;
    /* data */
    pre_send_data_buf[5] = 0x60;    //验证A密钥
    pre_send_data_buf[6] = 0x34;    //块 52
    pre_send_data_buf[7] = 0xff;
    pre_send_data_buf[8] = 0xff;
    pre_send_data_buf[9] = 0xff;
    pre_send_data_buf[10] = 0xff;
    pre_send_data_buf[11] = 0xff;
    pre_send_data_buf[12] = 0xff;

    check_value = cal_check_value(&pre_send_data_buf[1], 12);    //从模块地址到数据域结束
    pre_send_data_buf[13] = check_value;         //校验字
    pre_send_data_buf[14] = PROTOCOL_TAIL;

    len = pre_proc_data(pre_send_data_buf, 11, send_data_buf);
    if(len > 0)
    {
        uart2_send(send_data_buf, len);
    }
    return 0;
}


uint8_t read_rfid(uint8_t absolute_block_num)   // e.g: 02 00 00 04 4b 34 83 03           ack: 02 00 00 13 4B 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 5E 03 
{
    uint8_t check_value = 0;
    uint8_t len = 0;
    pre_send_data_buf[0] = PROTOCOL_HEAD;
    pre_send_data_buf[1] = 0;       //模块地址
    pre_send_data_buf[2] = 0;       //模块地址
    pre_send_data_buf[3] = 4;       //length:从长度字到校验字的字节数
    pre_send_data_buf[4] = 0x4b;    //cmd: 读块
    /* data */
    pre_send_data_buf[5] = absolute_block_num;  //绝对块号

    check_value = cal_check_value(&pre_send_data_buf[1], 5);    //从模块地址到数据域结束
    pre_send_data_buf[6] = check_value;         //校验字
    pre_send_data_buf[7] = PROTOCOL_TAIL;

    len = pre_proc_data(pre_send_data_buf, 8, send_data_buf);
    if(len > 0)
    {
        uart2_send(send_data_buf, len);
    }
    return 0;
}



/******************** uart receive protocol *****************/

static bool is_protocol_cmd(uint8_t cmd)
{
    return TRUE;
}

static bool is_frame_valid(uint8_t *data, uint8_t len)
{
    if((data != NULL) && (len > 8))
    {
        if((data[0] == PROTOCOL_HEAD) && (data[len - 1] == PROTOCOL_TAIL))
        {
            return TRUE;
        }
    }
    return FALSE;
}

uint8_t sanwei_rfid_rcv_proccess(uint8_t *data, uint8_t len)
{
    uint16_t module_addr = 0;
    uint8_t cmd = 0;
    uint8_t result = 0xff;
    if(is_frame_valid(data, len))
    {
        if(is_protocol_cmd(cmd))
        {
            module_addr = data[1] | (data[2] << 8);
            cmd = data[4];
            result = data[5];
            switch(cmd)
            {
                case SW_RFID_PROTOCOL_CMD_SEARCH_CARD:
                    break;
                default: break;
            }
        }
    }
    return FALSE;
}




