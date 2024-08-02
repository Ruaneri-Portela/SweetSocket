#include <stdio.h>
#include "SweetSocket.h"

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "server") == 0)
        {
            struct socketGlobalContext *context = initSocketGlobalContext(SOCKET_SERVER);
            context->useHeader = false;
            pushNewConnection(&context->connections, createSocket(context, AF_INET, "127.0.0.1", 9950));
            pushNewConnection(&context->connections, createSocket(context, AF_INET6, "::1", 9950));
            startListening(context, APPLY_ALL);
            startAccepting(context, APPLY_ALL);
            while (true)
            {
                //sendData("Hello World\n", 13, context, APPLY_ALL, APPLY_ALL);
                for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
                {
                    for (struct socketClients *client = current->clients; client != NULL; client = client->next)
                    {
                        struct dataPool reviced;
                        while (true)
                        {
                            if (reviceData(context, current->id, client->id, &reviced))
                            {
                                const char* http_request =
                                    "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Content-Length: 13\r\n"
                                    "\r\n"
                                    "Hello, World!";
                                sendData(http_request, strlen(http_request), context, current->id, client->id);
                                /*
                                printf("Reviced: %s\n", reviced.data);
                                */
                                free(reviced.data);
                                continue;
                            }
                            break;
                        };
                    }
                }
                sweetThread_Sleep(1);
            }
            closeSocketGlobalContext(&context);
        }
        if (strcmp(argv[1], "client") == 0)
        {
            struct socketGlobalContext *context = initSocketGlobalContext(SOCKET_CLIENT);
            int64_t connectionId = pushNewConnection(&context->connections, createSocket(context, AF_INET, "127.0.0.1", 9950));
            startConnection(context, connectionId);
            enablePools(context, connectionId, true, true);
            while (true)
            {
                sendData("Hello World\n", 13, context, APPLY_ALL, APPLY_ALL);
                while (true)
                {
                    struct dataPool reviced;
                    if (reviceData(context, connectionId, connectionId, &reviced))
                    {
                        printf("Reviced: %s\n", reviced.data);
                        free(reviced.data);
                        continue;
                    }
                    break;
                }
                sweetThread_Sleep(1000);
            }
        }
    }
    return 0;
}