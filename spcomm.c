
#include "windows.h"
#include <stdio.h>
#include "spcomm.h"


static BOOL HandleCommEvent(SPComm* uart, OVERLAPPED* overlapped_comm_event, DWORD* fdwEvtMask, BOOL fRetrieveEvent)
{
    DWORD dwDummy;
    DWORD dwLastError;
    if (fdwEvtMask)
    {
        if (!GetOverlappedResult(uart->hCommFile, overlapped_comm_event, &dwDummy, FALSE))
        {
            dwLastError = GetLastError();
            if (dwLastError == ERROR_INVALID_HANDLE)
            {
                return FALSE;
            }
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL SetupCommEvent(SPComm* comm, OVERLAPPED* overlapped_comm_event, DWORD* fdwEvtMask)
{
    DWORD dwLastError;
    while (WaitCommEvent(comm->hCommFile, fdwEvtMask, overlapped_comm_event))
    {
        if (HandleCommEvent(comm, overlapped_comm_event, fdwEvtMask, FALSE) == FALSE)
        {
            return FALSE;
        }
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

static BOOL HandleReadData(BYTE* input_buffer, DWORD buffer_len)
{
    int i;
    for (i = 0; i < buffer_len; i++)
    {
        printf("%X", input_buffer[i]);
    }
    printf("\n");
    return TRUE;
}

static BOOL SetupReadEvent(SPComm* comm, OVERLAPPED* read_ol, BYTE* input_buffer, DWORD buf_len)
{
    DWORD ret;
    int read_len;
    while (ReadFile(comm->hCommFile, input_buffer, buf_len, &read_len, read_ol))
    {
        //������յ�����
        HandleReadData(input_buffer,read_len);
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
    return FALSE;
}

BOOL HandleReadEvent(SPComm* comm, OVERLAPPED* read_ol, BYTE* input_buffer, DWORD buffer_size, DWORD* read_len)
{
    if (GetOverlappedResult(comm->hCommFile, read_ol, read_len, FALSE))
    {
        return HandleReadData(input_buffer, *read_len);
    }
    return TRUE;
}

static void SPCommReadThread(SPComm* comm)
{
    DWORD RealRead = 0;
    OVERLAPPED read_ol;
    OVERLAPPED comm_ol;
    DWORD fdwEvtMask;
    DWORD ret = 0;
    BYTE input_buffer[1024];
    HANDLE handles_to_wait_for[2];
    DWORD handle_signaled;
    DWORD read_len;

    memset(&read_ol, 0x00, sizeof(OVERLAPPED));
    memset(&comm_ol, 0x00, sizeof(OVERLAPPED));

    read_ol.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (read_ol.hEvent == NULL)
    {
        while(TRUE);
    }

    comm_ol.hEvent = CreateEvent(
        NULL,   // default security attributes 
        TRUE,   // manual-reset event 
        TRUE,  // not signaled 
        NULL    // no name
        );
    if (comm_ol.hEvent == NULL)
    {
        while(TRUE);
    }

    handles_to_wait_for[0] = comm_ol.hEvent;
    handles_to_wait_for[1] = read_ol.hEvent;

    if (SetCommMask(comm->hCommFile, EV_ERR | EV_RLSD | EV_RING) == FALSE)
    {
        while(TRUE);
    }

    SetupCommEvent(comm, &comm_ol, &fdwEvtMask);

    SetupReadEvent(comm, &read_ol, input_buffer, sizeof(input_buffer));

    while (TRUE)
    {        
        handle_signaled = WaitForMultipleObjects(2, handles_to_wait_for, FALSE, INFINITE);

        switch(handle_signaled)
        {
        case WAIT_OBJECT_0:
            //Handle the CommEvent.
            if (!HandleCommEvent(comm, &comm_ol, &fdwEvtMask, TRUE))
            {
                goto EndReadThread;
            }
            if (!SetupCommEvent(comm, &comm_ol, &fdwEvtMask))
            {
                goto EndReadThread;
            }
            break;
        case WAIT_OBJECT_0 + 1:
            HandleReadEvent(comm, &read_ol, input_buffer, sizeof(input_buffer), &read_len);
            if (!SetupReadEvent(comm, &read_ol, input_buffer, sizeof(input_buffer)))
            {
                goto EndReadThread;
            }
            break;
        }
    }

EndReadThread:

    PurgeComm(comm->hCommFile, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
    CloseHandle(read_ol.hEvent);
    CloseHandle(comm_ol.hEvent);

}

/*�첽��ʽ�򿪴���*/
SPComm* SPCommCreate(void)
{
    SPComm* comm = NULL;

    comm = (SPComm*)malloc(sizeof(SPComm));
    if (comm == NULL)
    {
        return NULL;
    }

    memset(comm, 0, sizeof(SPComm));

    comm->ComName = "COM1";
    comm->BaudRate = 9600;
    comm->ByteSize = 8;
    comm->StopBits = 1;
    comm->Parity = PARITY_EVEN;
    comm->hCommFile = NULL;
    comm->hCommFile = NULL;
    comm->PortOpen = FALSE;
    return comm;
}

/*�򿪴���*/
BOOL SPCommStart(SPComm* comm)
{
    COMMTIMEOUTS timeouts;
    DCB dcb;

    /*�����豸*/
    comm->hCommFile = CreateFile(comm->ComName,                               /*���ں�*/
                      GENERIC_READ | GENERIC_WRITE,           /*��д����*/
                      FILE_SHARE_WRITE|FILE_SHARE_READ,       /*��������*/
                      0,                                      /*��ȫ����, 0ΪĬ�ϰ�ȫ����*/
                      OPEN_EXISTING,                          /*�����е��豸, ���Ǵ���*/
                      FILE_FLAG_OVERLAPPED,                   /*�ļ����������첽I/O*/
                      0                                       /*ģ����, */
                      );
    if (comm == INVALID_HANDLE_VALUE)
    {
        printf("Error opening serial port.\n");
        return FALSE;
    }

    if (GetFileType(comm->hCommFile) != FILE_TYPE_CHAR)
    {
        CloseHandle(comm->hCommFile);
        printf("File handle is not a comm handle.\n");
        return FALSE;
    }

    if(!SetupComm(comm->hCommFile, 4096, 4096))
    {
        CloseHandle(comm->hCommFile);
        printf("Cannot setup comm buffer.\n");
        return FALSE;
    }

    PurgeComm(comm->hCommFile, PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);

    /*���ַ�֮�������ʱ, ��ȡ����ʱ��һ�������ַ������ʱ������ʱ�䣬��ȡ�������������е�����*/
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    /*��ȡÿ���ַ���ĳ�ʱ*/
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    /*��һ��ȡ�������ݵĹ̶���ʱ*/
    timeouts.ReadTotalTimeoutConstant    = 0;
    /*дÿ���ַ���ĳ�ʱ*/
    timeouts.WriteTotalTimeoutMultiplier = 50;
    /*дһ��ȡ�������ݵĹ̶���ʱ*/
    timeouts.WriteTotalTimeoutConstant   = 2000;
    SetCommTimeouts(comm, &timeouts);

    /*���ô��ڲ���*/
    GetCommState(comm->hCommFile, &dcb);
    dcb.BaudRate = comm->BaudRate;
    dcb.ByteSize = comm->ByteSize;
    dcb.Parity = (BYTE)comm->Parity;
    dcb.StopBits = comm->StopBits;
    dcb.fBinary = 1;
    SetCommState(comm->hCommFile, &dcb);

    //EscapeCommFunction(comm, SETDTR);
    comm->ReadThread = CreateThread(NULL, 2048, (LPTHREAD_START_ROUTINE)SPCommReadThread,
        comm, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

    comm->OverlappedWrite.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (comm->OverlappedWrite.hEvent == NULL)
    {
        printf("Can't create write event of overlapped.\n");
        return FALSE;
    }

    return TRUE;
}

void SPCommSetBaudrate(SPComm* comm, DWORD baudrate)
{
    if (comm == NULL)
    {
        return;
    }
    comm->BaudRate = baudrate;
}

void SPCommSetParity(SPComm* comm, BYTE parity)
{
    if (comm == NULL)
    {
        return;
    }
    comm->Parity = parity;
}

DWORD SPCommSend(SPComm* comm, BYTE* buf, DWORD len)
{
    DWORD WError;
    DWORD RealSend = 0;
    DWORD ret;
    DWORD send_len = 0;
    OVERLAPPED ol = comm->OverlappedWrite;

    if ((comm == NULL) || (comm->hCommFile == NULL))
    {
        return 0;
    }

    if (ClearCommError(comm->hCommFile, &WError, NULL) == TRUE)
    {
        PurgeComm(comm->hCommFile, PURGE_TXABORT | PURGE_TXCLEAR);

        if (!WriteFile(comm->hCommFile, buf, len, &RealSend, &ol))
        {
            /*���ڷ���*/
            if (GetLastError() == ERROR_IO_PENDING)
            {
                /*�ȴ��������*/
                do 
                {
                    ret = WaitForSingleObject(ol.hEvent,1000);
                    if (ret == WAIT_OBJECT_0)
                    {
                        /*�������*/
                        send_len = len;
                        break;
                    }
                } while (ret == WAIT_TIMEOUT);
                
                if (ret != WAIT_OBJECT_0)
                {
                    /*����ʧ��*/
                    send_len = 0;
                }
            }
        }
    }

    return send_len;
}
