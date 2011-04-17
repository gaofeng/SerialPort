
#ifndef _SPCOMM_H
#define _SPCOMM_H

/*VUart�豸����Ϣ*/
typedef struct VUART_INFO_st
{
    HANDLE hCommFile;
    const char* ComName;
    BOOL PortOpen;
    DWORD BaudRate;
    WORD Parity;
    BYTE StopBits;
    BYTE ByteSize;
    HANDLE      ReadThread;            /*�߳̾��*/
    OVERLAPPED  OverlappedWrite;
} SPComm;

/*�첽��ʽ�򿪴���*/
SPComm* SPCommCreate(void);
/*�򿪴���*/
BOOL SPCommStart(SPComm* comm);

void SPCommSetBaudrate(SPComm* comm, DWORD baudrate);

void SPCommSetParity(SPComm* comm, BYTE parity);

DWORD SPCommSend(SPComm* comm, BYTE* buf, DWORD len);

#endif /*_UART_H*/