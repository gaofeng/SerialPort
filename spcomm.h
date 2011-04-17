
#ifndef _SPCOMM_H
#define _SPCOMM_H

/*VUart设备的信息*/
typedef struct VUART_INFO_st
{
    HANDLE hCommFile;
    const char* ComName;
    BOOL PortOpen;
    DWORD BaudRate;
    WORD Parity;
    BYTE StopBits;
    BYTE ByteSize;
    HANDLE      ReadThread;            /*线程句柄*/
    OVERLAPPED  OverlappedWrite;
} SPComm;

/*异步方式打开串口*/
SPComm* SPCommCreate(void);
/*打开串口*/
BOOL SPCommStart(SPComm* comm);

void SPCommSetBaudrate(SPComm* comm, DWORD baudrate);

void SPCommSetParity(SPComm* comm, BYTE parity);

DWORD SPCommSend(SPComm* comm, BYTE* buf, DWORD len);

#endif /*_UART_H*/