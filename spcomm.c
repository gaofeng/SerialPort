
#include "windows.h"
#include <stdio.h>
#include "spcomm.h"


/*VUart设备的信息*/
typedef struct VUART_INFO_st
{
    HANDLE      ThreadHandle;            /*线程句柄*/
    HANDLE      CommHandle;              /*串口句柄*/
    OVERLAPPED  WROverlapped;
    u8* RecvBuff[2000];
    const char* ComName;
} SPComm;

static bool HandleCommEvent(SPComm* uart, OVERLAPPED* overlapped_comm_event, DWORD* fdwEvtMask, bool fRetrieveEvent)
{
    DWORD dwDummy;
    DWORD dwErrors;
    DWORD dwLastError;
    if (fdwEvtMask)
    {
        if (!GetOverlappedResult(uart->CommHandle, overlapped_comm_event, dwDummy, FALSE))
        {
            dwLastError = GetLastError();
        }
    }
}

static bool SetupCommEvent(SPComm uart, OVERLAPPED* overlapped_comm_event, DWORD* fdwEvtMask)
{
    DWORD dwLastError;
    while (WaitCommEvent(uart->CommHandle, fdwEvtMask, overlapped_comm_event))
    {

    }
    dwLastError = GetLastError();
    if (dwLastError == ERROR_IO_PENDING)
    {
        return TRUE;
    }
    if (dwLastError == ERROR_INVALID_HANDLE)
    {
        return FALSE;
    }
    while(TRUE);
}

static bool SetupReadEvent(SPComm vuart, u8* buf, u32 buf_len)
{
    DWORD ret;
    u32 read_len;
    while (ReadFile(vuart->CommHandle, buf, buf_len, &read_len, &(vuart->WROverlapped)))
    {
        //处理接收的数据
        BufAddData(&(vuart->RecvBuff), buf, read_len);
    }
    ret = GetLastError();
    if (ret == ERROR_IO_PENDING)
    {
        return TRUE;
    }
    if (ret = ERROR_INVALID_HANDLE)
    {
        return FALSE;
    }
}

static void UARTRecvTask(void* para)
{
    u32 RError;
    u32 RealRead = 0;
    u8 buf[1024] = {0};
    OVERLAPPED recv_ol = ((SPComm)para)->WROverlapped;
    OVERLAPPED comm_ol;
    DWORD evt_mask;
    DWORD ret = 0;
    SPComm uart = (SPComm)para;

    memset(&comm_ol, 0x00, sizeof(OVERLAPPED));
    comm_ol.hEvent = CreateEvent(
        NULL,   // default security attributes 
        TRUE,   // manual-reset event 
        TRUE,  // not signaled 
        NULL    // no name
        );
    if (SetCommMask(uart->CommHandle, EV_ERR | EV_RLSD | EV_RING) == FALSE)
    {
        DEAD_LOOP;
    }

    DEAD_LOOP
    {        
        if (WaitCommEvent(uart->CommHandle, &evt_mask, &comm_ol))
        {

        }
        if ((evt_mask & EV_RLSD) == 0)
        {
            Sleep(100);
            continue;
        }
        RealRead = 0;
        MemSet(buf, 0x00, 1024);
        OSALSemWait(uart->CommSem, 0);
        if (ClearCommError(uart->CommHandle, &RError, NULL))
        {
            PurgeComm(uart->CommHandle, PURGE_TXABORT | PURGE_TXCLEAR);

            while (ReadFile(uart->CommHandle, buf, 1024, &RealRead, &recv_ol))
            {
                BufAddData(&(uart->RecvBuff), buf, RealRead);
            }

            RError = GetLastError();
            if(RError == ERROR_IO_PENDING)
            {
                //使用WaitForSingleObject函数等待，直到读操作完成或延时已达到2秒钟
                //当串口读操作进行完毕后，m_osRead的hEvent事件会变为有信号
                do 
                {
                    ret = WaitForSingleObject(recv_ol.hEvent,2000);
                    if (ret == WAIT_OBJECT_0)
                    {
                        /*发送完成*/
                        break;
                    }
                } while (ret == WAIT_TIMEOUT);
                if (ret != WAIT_OBJECT_0)
                {
                    RealRead = 0;
                }
            }
            else if (RError != 0)
            {
                RealRead = 0;
            }

        }
        BufAddData(&(uart->RecvBuff), buf, RealRead);
        OSALSemRelease(uart->CommSem);
    }
}

/*异步方式打开串口*/
static SPComm UARTInit(u8* commname, u32 baudrate, u8 bytesize, u8 parity, u8 stopbits,
                      u32 send_buf_size, u32 recv_buf_size)
{
    HANDLE          comm;       /*串口Handle*/
    DCB             dcb;        /*串口参数*/
    COMMTIMEOUTS    timeouts;
    SPComm           uart = NULL;
    u8*             buf = NULL;
    u32 Error = 0;

    buf = Malloc(sizeof(u8) * recv_buf_size);
    if (buf == NULL)
    {
        return NULL;
    }

    uart = (SPComm)Malloc(sizeof(UART_INFO));
    if (uart == NULL)
    {
        return NULL;
    }
    MemSet(buf, 0x00, recv_buf_size);

    /*建立设备*/
    comm = CreateFile(
                        commname,                               /*串口号*/
                        GENERIC_READ | GENERIC_WRITE,           /*读写属性*/
                        FILE_SHARE_WRITE|FILE_SHARE_READ,       /*共享属性*/
                        0,                                      /*安全属性, 0为默认安全级别*/
                        OPEN_EXISTING,                          /*打开已有的设备, 不是创建*/
                        FILE_FLAG_OVERLAPPED,                   /*文件属性用于异步I/O*/
                        0                                       /*模板句柄, */
                      );
    if (comm == INVALID_HANDLE_VALUE)
    {
        Error = GetLastError();
        Free(uart);
        return NULL;
    }


    /*设置串口参数*/
    GetCommState(comm,   &dcb);
    dcb.BaudRate    =   baudrate;
    dcb.ByteSize    =   bytesize;
    dcb.Parity      =   parity;
    dcb.StopBits    =   stopbits;
    SetCommState(comm, &dcb);

    /*两字符之间最大延时, 读取数据时，一旦两个字符传输的时间差超过该时间，读取函数将返回现有的数据*/
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    /*读取每个字符间的超时*/
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    /*读一次取串口数据的固定超时*/
    timeouts.ReadTotalTimeoutConstant    = 0;
    /*写每个字符间的超时*/
    timeouts.WriteTotalTimeoutMultiplier = 50;
    /*写一次取串口数据的固定超时*/
    timeouts.WriteTotalTimeoutConstant   = 2000;
    SetCommTimeouts(comm, &timeouts);

    if(!SetupComm(comm, recv_buf_size, send_buf_size))
    {
        return NULL;
    }

    EscapeCommFunction(comm, SETDTR);

    ZeroMemory(&uart->WROverlapped, sizeof(uart->WROverlapped));
    if (uart->WROverlapped.hEvent != NULL)
    {
        ResetEvent(uart->WROverlapped.hEvent);
        uart->WROverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    uart->CommHandle = comm;
    uart->RecvBuff = BufInit(buf, recv_buf_size, BUF_MODE_CYCLE, NULL, NULL);
    uart->ComName = commname;

    /*创建SemHandle*/
    uart->CommSem = OSALSemCreate("vuartsem", 1, 1);
    uart->ThreadHandle = CreateThread(NULL, 2048, (LPTHREAD_START_ROUTINE)UARTRecvTask,
        uart, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

    return uart;
}

void VUARTSetBaudrate(Handle h, u32 baudrate)
{
    DCB             dcb;        /*串口参数*/
    HANDLE comm = INVALID_HANDLE_VALUE;
    char pos[30];
    TCHAR       szAppName[50] = {0};

    if (h == NULL)
    {
        return;
    }
    comm = ((SPComm)h)->CommHandle;

//    wSprintf(szAppName, TEXT(((VUART)h)->ComName));


    /*设置串口参数*/
    GetCommState(comm,   &dcb);
    dcb.BaudRate    =   baudrate;
    SetCommState(comm, &dcb);

    IToA(baudrate, pos, 10);
    //WritePrivateProfileString(szAppName, "baudrate", pos, TEXT("./vuart.ini"));
}

void VUARTSetParity(Handle h, ParityType parity)
{
    DCB             dcb;        /*串口参数*/
    HANDLE comm = INVALID_HANDLE_VALUE;
    char pos[30];
    TCHAR       szAppName[50] = {0};

    if (h == NULL)
    {
        return;
    }
    comm = ((SPComm)h)->CommHandle;

//    wSprintf(szAppName, TEXT(((VUART)h)->ComName));

    /*设置串口参数*/
    GetCommState(comm,   &dcb);
    dcb.Parity    =   parity;
    SetCommState(comm, &dcb);

    IToA(parity, pos, 10);
    //WritePrivateProfileString(szAppName, "baudrate", pos, TEXT("./vuart.ini"));
}

void VUARTSetByteSize(Handle h, DataBitsType byte_size)
{
    DCB             dcb;        /*串口参数*/
    HANDLE comm = INVALID_HANDLE_VALUE;
    char pos[30];
    TCHAR       szAppName[50] = {0};

    if (h == NULL)
    {
        return;
    }
    comm = ((SPComm)h)->CommHandle;

//    wSprintf(szAppName, TEXT(((VUART)h)->ComName));

    /*设置串口参数*/
    GetCommState(comm,   &dcb);
    dcb.ByteSize    =   byte_size;
    SetCommState(comm, &dcb);

    IToA(byte_size, pos, 10);
    //WritePrivateProfileString(szAppName, "baudrate", pos, TEXT("./vuart.ini"));
}

void VUARTSetStopBits(Handle h, StopBitsType stop_bits)
{
    DCB             dcb;        /*串口参数*/
    HANDLE comm = INVALID_HANDLE_VALUE;
    char pos[30];
    TCHAR       szAppName[50] = {0};

    if (h == NULL)
    {
        return;
    }
    comm = ((SPComm)h)->CommHandle;

 //   wSprintf(szAppName, TEXT(((VUART)h)->ComName));

    /*设置串口参数*/
    GetCommState(comm,   &dcb);
    dcb.StopBits    =   stop_bits;
    SetCommState(comm, &dcb);

    IToA(stop_bits, pos, 10);
    //WritePrivateProfileString(szAppName, "baudrate", pos, TEXT("./vuart.ini"));
}

Handle VUARTCreate(u8* comname)
{
    SPComm handle = NULL;
    TCHAR       szAppName[50] = {0};
    char name[32] = {V32(0x00)};
    u32 baudrate = 0;
    u8 bytesize = 0;
    u8 parity = 0;
    u8 stopbits = 0;
    u32 send_buf_size = 0; 
    u32 recv_buf_size = 0;
    char pos[30];

    wsprintf(szAppName, TEXT(comname));

    GetPrivateProfileString(szAppName, "comname", "com1", name, 20, TEXT("./vuart.ini"));
    baudrate = (u32)GetPrivateProfileInt(szAppName, TEXT("baudrate"), 38400, TEXT("./vuart.ini"));
    bytesize = (u8)GetPrivateProfileInt(szAppName, TEXT("bytesize"), 8, TEXT("./vuart.ini"));
    parity = (u8)GetPrivateProfileInt(szAppName, TEXT("parity"), PARITY__EVEN, TEXT("./vuart.ini"));
    stopbits = (u8)GetPrivateProfileInt(szAppName, TEXT("stopbits"), STOP_1, TEXT("./vuart.ini"));
//     send_buf_size = (u32)GetPrivateProfileInt(szAppName, TEXT("send_buf_size"), 2048, TEXT("./vuart.ini"));
//     recv_buf_size = (u32)GetPrivateProfileInt(szAppName, TEXT("recv_buf_size"), 2048, TEXT("./vuart.ini"));
        
    WritePrivateProfileString(szAppName, "comname", name, TEXT("./vuart.ini"));
    IToA(baudrate, pos, 10);
    WritePrivateProfileString(szAppName, "baudrate", pos, TEXT("./vuart.ini"));
    IToA(bytesize, pos, 10);
    WritePrivateProfileString(szAppName, "bytesize", pos, TEXT("./vuart.ini"));
    IToA(parity, pos, 10);
    WritePrivateProfileString(szAppName, "parity", pos, TEXT("./vuart.ini"));
    IToA(stopbits, pos, 10);
    WritePrivateProfileString(szAppName, "stopbits", pos, TEXT("./vuart.ini"));
//     IToA(send_buf_size, pos, 10);
//     WritePrivateProfileString(szAppName, "send_buf_size", pos, TEXT("./vuart.ini"));
//     IToA(recv_buf_size, pos, 10);
//     WritePrivateProfileString(szAppName, "recv_buf_size", pos, TEXT("./vuart.ini"));
    

    handle = UARTInit(name, baudrate, bytesize, parity, stopbits, 2048, 2048);

    if (handle == NULL)
    {
        printf("Serial Port %s Open Failed!!\n", name);

        return NULL;
    }
    printf("Serial Port %s Opened Successful!\n", name);

    return handle;
}

u32 VUARTSend(Handle h, u8* buf, u32 len)
{
    u32 WError;
    u32 RealSend = 0;
    DWORD ret;
    u32 send_len = 0;
    OVERLAPPED ol = ((SPComm)h)->WROverlapped;

    if (h == NULL)
    {
        return 0;
    }

    OSALSemWait(((SPComm)h)->CommSem, 0);
    if (ClearCommError(((SPComm)h)->CommHandle, &WError, NULL) == TRUE)
    {
        PurgeComm(((SPComm)h)->CommHandle, PURGE_TXABORT | PURGE_TXCLEAR);

        if (!WriteFile(((SPComm)h)->CommHandle, buf, len, &RealSend, &ol))
        {
            /*正在发送*/
            if (GetLastError() == ERROR_IO_PENDING)
            {
                /*等待发送完成*/
                do 
                {
                    ret = WaitForSingleObject(ol.hEvent,1000);
                    if (ret == WAIT_OBJECT_0)
                    {
                        /*发送完成*/
                        send_len = len;
                        break;
                    }
                } while (ret == WAIT_TIMEOUT);
                
                if (ret != WAIT_OBJECT_0)
                {
                    /*发送失败*/
                    send_len = 0;
                }
            }
        }
    }

    OSALSemRelease(((SPComm)h)->CommSem);
    return send_len;
}

u32 VUARTRecv(Handle h, u8* buf, u32 len, bool clear)
{
#if 1
    u32 RealRead = 0;

    if (h == NULL)
    {
        return 0;
    }

    OSALSemWait(((SPComm)h)->CommSem, 0);
    
    RealRead = BufGetData(&((SPComm)h)->RecvBuff, buf, 0x00, len);
    if (clear == TRUE)
    {
        BufDelHeadData(&((SPComm)h)->RecvBuff, RealRead);
    }    

    OSALSemRelease(((SPComm)h)->CommSem);
    return RealRead;
#else
    u32 RError;
    u32 RealRead = 0;

    if (h == NULL)
    {
        return 0;
    }

    OSALSemWait(((SPComm)h)->CommSem, 0);
    if (ClearCommError(((SPComm)h)->CommHandle, &RError, NULL))
    {
        PurgeComm(((SPComm)h)->CommHandle, PURGE_TXABORT | PURGE_TXCLEAR);

        if(!ReadFile(((SPComm)h)->CommHandle, buf, len, &RealRead, &((SPComm)h)->WROverlapped))
        {
            RError = GetLastError();
            if(RError == ERROR_IO_PENDING)
            {
                while(GetOverlappedResult(((SPComm)h)->CommHandle, &((SPComm)h)->WROverlapped, &RealRead, FALSE))
                {
                    OSALSemRelease(((SPComm)h)->CommSem);
                    return RealRead;
                }
            }
        }
    }

    OSALSemRelease(((SPComm)h)->CommSem);
    return RealRead;
#endif
}

u32 VUARTDel(Handle h, u32 len)
{
    u32 RealRead = 0;

    if (h == NULL)
    {
        return 0;
    }

    OSALSemWait(((SPComm)h)->CommSem, 0);

    RealRead = BufDelHeadData(&((SPComm)h)->RecvBuff, len);

    OSALSemRelease(((SPComm)h)->CommSem);
    return RealRead;
}