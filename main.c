

#include <stdio.h>
#include "windows.h"
#include "spcomm.h"

int main(void)
{
    SPComm* comm = SPCommCreate();
    SPCommStart(comm);

    while (TRUE)
    {
        Sleep(1000);
        SPCommSend(comm, "gaofeng", 7);
    }
    return 0;
}