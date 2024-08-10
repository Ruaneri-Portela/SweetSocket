#include "http.h"
#include <signal.h>

// Variável global para indicar quando o servidor deve ser fechado
static int g_closing = 0;

// Função para processar requisições de clientes
void HTTP_processClientRequest(char* data, uint64_t size, struct socketGlobalContext* ctx, struct socketClients* thisClient, void* parms);

// Função que lida com o sinal SIGINT (Ctrl+C)
static void HTTP_handleSigint(int sig)
{
    (void)sig;  // Evita warnings para parâmetro não utilizado
    g_closing = 1;
}

// Estruturas de dados usadas para passar parâmetros entre funções
struct HTTP_upper_server_hosts {
    struct socketGlobalContext* context;
    struct HTTP_server_config* server;
};

struct HTTP_upper_server_ports {
    struct HTTP_upper_server_hosts* up;
    wchar_t* host;
};

// Função para processar portas do servidor
static enum HTTP_linked_list_actions HTTP_ports(struct HTTP_object* actual, void* parms, uint64_t count) {
    struct HTTP_upper_server_ports* up = (struct HTTP_upper_server_ports*)parms;
    struct socketGlobalContext* context = up->up->context;
    uint16_t* port = (uint16_t*)actual->object;
    uint8_t type = 0;

    size_t len = wcslen(up->host) + 1;
    char* host = (char*)malloc(len);
    if (!host) {
        perror("Failed to allocate memory for host");
        return ARRAY_STOP; // Interrompe a execução em caso de erro
    }
    wcstombs(host, up->host, len);

    if (strchr(host, ':') != NULL) {
        type = AF_INET6;
    } else if (strchr(host, '.') != NULL) {
        type = AF_INET;
    }

    pushNewConnection(&context->connections, createSocket(context, type, host, *port));
    free(host);
    return ARRAY_CONTINUE;
}

// Função para processar hosts do servidor
static enum HTTP_linked_list_actions HTTP_hosts(struct HTTP_object* actual, void* parms, uint64_t count) {
    struct HTTP_upper_server_hosts* up = (struct HTTP_upper_server_hosts*)parms;
    struct HTTP_upper_server_ports upPort = { up, (wchar_t*)actual->object };

    HTTP_arrayForEach(&up->server->ports, HTTP_ports, &upPort);
    return ARRAY_CONTINUE;
}

// Função principal
int main()
{
    // Inicialização do ambiente do servidor
    struct HTTP_server_envolvirment envolviment = { 0 };
    envolviment.server = HTTP_loadConfig();
    envolviment.context = initSocketGlobalContext(SOCKET_SERVER);
    envolviment.context->useHeader = false;

    // Carregamento de MIME types e plugins
    HTTP_loadMimeTypes(&envolviment);
    HTTP_loadPlugins(&envolviment);

    // Configuração de hosts e portas
    struct HTTP_upper_server_hosts up = { envolviment.context, &envolviment.server };
    HTTP_arrayForEach(&envolviment.server.hosts, HTTP_hosts, &up);

    // Início do servidor
    if (!startListening(envolviment.context, APPLY_ALL)) {
        closeSocketGlobalContext(&envolviment.context);
        perror("Failed to start listening");
        return 1;
    }

    // Aceitação de conexões e processamento de requisições
    startAccepting(envolviment.context, APPLY_ALL, NULL, &HTTP_processClientRequest, &envolviment, NULL, ONLY_RECIVE);
    signal(SIGINT, HTTP_handleSigint);
    wprintf(L"Press Ctrl+C to stop\n");

    // Loop principal do servidor
    while (!g_closing) {
        sweetThread_Sleep(1000);
    }

    // Fechamento do servidor
    closeSocketGlobalContext(&envolviment.context);
    return 0;
}
