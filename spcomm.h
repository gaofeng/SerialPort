
#ifndef _SPCOMM_H
#define _SPCOMM_H

#define u8 unsigned char
#define u32 unsigned int
#define Handle void*

Handle VUARTCreate(u8* commname);

void VUARTSetBaudrate(Handle h, u32 baudrate);

u32 VUARTSend(Handle h, u8* buf, u32 len);
u32 VUARTRecv(Handle h, u8* buf, u32 len, bool clear);

u32 VUARTDel(Handle h, u32 len);

#endif /*_UART_H*/