#include "http.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

// Variável global para indicar quando o servidor deve ser fechado
static int g_closing = 0;

void HTTP_processClientRequest(char *data, uint64_t size, struct SweetSocket_global_context *ctx, struct SweetSocket_peer_clients *thisClient, void *parms);

static void HTTP_handleSigint(int sig)
{
    (void)sig;
    g_closing = 1;
}

struct HTTP_upper_server_hosts
{
    struct SweetSocket_global_context *context;
    struct HTTP_server_config *server;
};

struct HTTP_upper_server_ports
{
    struct HTTP_upper_server_hosts *up;
    wchar_t *host;
};

static enum HTTP_linked_list_actions HTTP_ports(struct HTTP_object *actual, void *parms, uint64_t count)
{
    (void)count;
    struct HTTP_upper_server_ports *up = (struct HTTP_upper_server_ports *)parms;
    struct SweetSocket_global_context *context = up->up->context;
    uint16_t *port = (uint16_t *)actual->object;
    uint8_t type = 0;

    // Converter o endereço do host de wide string para string
    size_t len = wcslen(up->host) + 1;
    char *host = (char *)malloc(len);
    if (!host)
    {
        perror("Failed to allocate memory for host");
        return ARRAY_STOP; // Interrompe a execução em caso de erro
    }
    wcstombs(host, up->host, len);

    // Determinar o tipo de socket com base no endereço do host
    if (strchr(host, ':') != NULL)
    {
        type = AF_INET6;
    }
    else if (strchr(host, '.') != NULL)
    {
        type = AF_INET;
    }

    // Criar e adicionar a conexão ao contexto
    SweetSocket_pushNewConnection(&context->connections, SweetSocket_createPeer(context, type, host, *port));
    free(host);
    return ARRAY_CONTINUE;
}

static enum HTTP_linked_list_actions HTTP_hosts(struct HTTP_object *actual, void *parms, uint64_t count)
{
    (void)count;
    struct HTTP_upper_server_hosts *up = (struct HTTP_upper_server_hosts *)parms;
    struct HTTP_upper_server_ports upPort = {up, (wchar_t *)actual->object};

    HTTP_arrayForEach(&up->server->ports, HTTP_ports, &upPort);
    return ARRAY_CONTINUE;
}

int main()
{
    // Inicialização do ambiente do servidor
    struct HTTP_server_envolvirment envolviment = {0};
    envolviment.server = HTTP_loadConfig();
    envolviment.context = SweetSocket_initGlobalContext(PEER_SERVER);
    envolviment.context->useHeader = false;

    // Carregamento de MIME types e plugins
    HTTP_loadMimeTypes(&envolviment);
    HTTP_loadPlugins(&envolviment);

    // Configuração de hosts e portas
    struct HTTP_upper_server_hosts up = {envolviment.context, &envolviment.server};
    HTTP_arrayForEach(&envolviment.server.hosts, HTTP_hosts, &up);

    // Início do servidor
    if (!SweetSocket_serverStartListening(envolviment.context, APPLY_ALL))
    {
        SweetSocket_closeGlobalContext(&envolviment.context);
        perror("Failed to start listening");
        return 1;
    }

    // Aceitação de conexões e processamento de requisições
    SweetSocket_serverStartAccepting(envolviment.context, APPLY_ALL, NULL, (void *)&HTTP_processClientRequest, &envolviment, NULL, ONLY_RECIVE);
    signal(SIGINT, HTTP_handleSigint);
    wprintf(L"Press Ctrl+C to stop\n");

    // Loop principal do servidor
    while (!g_closing)
    {
        SweetThread_sleep(1000); // Pausa por 1 segundo
    }

    // Fechamento do servidor
    SweetSocket_closeGlobalContext(&envolviment.context);
    return 0;
}
