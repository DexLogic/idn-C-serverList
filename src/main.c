// ----------------------------------------------------------------------------------------------
//  File main.c
//
//  Copyright (c) 2016, 2017 DexLogic, Dirk Apitz
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
// ----------------------------------------------------------------------------------------------
//  Change History:
//
//  05/2016 Dirk Apitz, created
//  01/2017 Dirk Apitz, ServiceMap
//  06/2017 Dirk Apitz, Windows port, GitHub release
// ----------------------------------------------------------------------------------------------

// Standard libraries
#include <stdio.h>
#include <stdarg.h>


// Platform includes
#if defined(_WIN32) || defined(WIN32)

    #include "plt-windows.h"

#else

    #include "plt-posix.h"

#endif


// Project headers
#include "idnServerList.h"


// -------------------------------------------------------------------------------------------------
//  Tools
// -------------------------------------------------------------------------------------------------

static char *vbufPrintf(char *bufPtr, char *limitPtr, const char *fmt, va_list arg_ptr)
{
    // Determine available space. Abort in case of invalid buffer.
    int len = limitPtr - bufPtr;
    if((bufPtr == (char *)0) || (len <= 0)) return (char *)0;

    // Print in case ellipsis would fit.
    if(len > 4) 
    {
        // Reserve margin and print string. Note: snprintf guarantees for a trailing '\0'.
        len -= 4;
        int rc = vsnprintf(bufPtr, len + 1, fmt, arg_ptr);

        if(rc > len) 
        {
            // String truncated (less characters available than needed). Append ellipsis.
            bufPtr = &bufPtr[len];
        }
        else if(rc >= 0)
        {
            // Printed string fits. Return new start pointer.
            return &bufPtr[rc];
        }
        else
        {
            // In case of error - ignore (make sure that the string is terminated).
            *bufPtr = '\0';
            return bufPtr;
        }
    }

    // In case of insufficient buffer: Append ellipsis.
    while((limitPtr - bufPtr) > 1) *bufPtr++ = '.';
    *bufPtr = '\0';

    return bufPtr;
}


static char *bufPrintf(char *bufPtr, char *limitPtr, const char *fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);

    return vbufPrintf(bufPtr, limitPtr, fmt, arg_ptr);
}


static void logError(const char *fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);

//    printf("\x1B[1;31m");
    vfprintf(stderr, fmt, arg_ptr);
//    printf("\x1B[0m");

    fprintf(stderr, "\n");
    fflush(stderr);
}


static void logInfo(const char *fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);

    vfprintf(stdout, fmt, arg_ptr);

    fprintf(stdout, "\n");
    fflush(stdout);
}


static void logServer(IDNSL_SERVER_INFO *serverInfo)
{
    // Allocate log buffer
    char logString[200], *logPtr = logString, *logLimit = &logString[sizeof(logString)];

    // Print unitID as a string
    unsigned unitIDLen = serverInfo->unitID[0];
    unsigned char *src = (unsigned char *)&serverInfo->unitID[1];
    for(unsigned i = 0; i < unitIDLen; i++)
    {
        logPtr = bufPrintf(logPtr, logLimit, "%02X", *src++);
        if(i == 0) logPtr = bufPrintf(logPtr, logLimit, "-");
    }

    // Append host name (in case available)
    if(serverInfo->hostName[0]) 
    {
        logPtr = bufPrintf(logPtr, logLimit, "(%s)", serverInfo->hostName);
    }

    // Append server address information
    for (unsigned i = 0; i < serverInfo->addressCount; i++)
    {
        IDNSL_SERVER_ADDRESS *addrInfo = &serverInfo->addressTable[i];

        // Append separator
        if(i == 0) logPtr = bufPrintf(logPtr, logLimit, " at ");
        else logPtr = bufPrintf(logPtr, logLimit, ", ");

        // Convert IP address to string
        char ifAddrString[20];
        if(inet_ntop(AF_INET, &addrInfo->addr, ifAddrString, sizeof(ifAddrString)) == (char *)0)
        {
            logError("inet_ntop() failed (error: %d)", plt_sockGetLastError());
            snprintf(ifAddrString, sizeof(ifAddrString), "<error>");
        }

        // Append address and reachability information
        const char *comment = "";
        if(addrInfo->errorFlags & IDNSL_ADDR_ERRORFLAG_AMBIGUOUS) comment = " (ambiguous)";
        else if(addrInfo->errorFlags & IDNSL_ADDR_ERRORFLAG_UNREACHABLE) comment = " (unreachable)";
        logPtr = bufPrintf(logPtr, logLimit, "%s%s", ifAddrString, comment);
    }

    // ... and write the server information log line
    logInfo(logString);

    // ------------------------------------------------------------------------.

    // Log all services
    int idWidth = 1;
    if(serverInfo->serviceCount >= 10) idWidth++;
    if(serverInfo->serviceCount >= 100) idWidth++;
    for(unsigned i = 0; i < serverInfo->serviceCount; i++) 
    {
        IDNSL_SERVICE_INFO *serviceEntry = &serverInfo->serviceTable[i];
        logPtr = logString;

        // Print service ID
        logPtr = bufPrintf(logPtr, logLimit, "  %*u: ", idWidth, serviceEntry->serviceID);

        // Print service name
        char *serviceName = serviceEntry->serviceName;
        if(serviceName[0] == '\0') serviceName = (char *)"<unnamed>";
        logPtr = bufPrintf(logPtr, logLimit, serviceName);

        // Eventually print referred relay name
        if(serviceEntry->parentRelay)
        {
            IDNSL_RELAY_INFO *relayEntry = serviceEntry->parentRelay;

            char *relayName = relayEntry->relayName;
            if(relayName[0] == '\0') relayName = (char *)"<blank>";
            logPtr = bufPrintf(logPtr, logLimit, "@%s", relayName);
        }

        // ... and write the service log line
        logInfo(logString);
    }
}


// -------------------------------------------------------------------------------------------------
//  Sample code entry point
// -------------------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
    logInfo("IDN server list");
    logInfo("------------------------------------------------------------");

    do
    {
        // Initialize platform sockets
        int rcStartup = plt_sockStartup();
        if(rcStartup)
        {
            logError("Socket startup failed (error: %d)", rcStartup);
            break;
        }

        // Find all IDN servers
        unsigned msTimeout = 500;
        IDNSL_SERVER_INFO *firstServerInfo;
        int rcGetList = getIDNServerList(&firstServerInfo, msTimeout);
        if(rcGetList != 0)
        {
            logError("getIDNServerList() failed (error: %d)", rcGetList);
            break;
        }

        // Print list of servers
        for(IDNSL_SERVER_INFO *serverInfo = firstServerInfo; serverInfo; serverInfo = serverInfo->next) 
        {
            logServer(serverInfo);
        }

        // Server list is dynamically allocated and must be freed
        freeIDNServerList(firstServerInfo);
    }
    while(0);

    // Platform sockets cleanup
    if(plt_sockCleanup()) logError("Socket cleanup failed (error: %d)", plt_sockGetLastError());

    return 0;
}

