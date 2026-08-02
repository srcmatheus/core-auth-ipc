# ADR 0004: Definição da Interface de IPC, Gerenciamento de Estado por Conexão e Padronização de Tipos (`ipc_server.h`)

* **Status:** Aceito
* **Data:** 2026-08-02
* **Autor:** Matheus Nascimento

---

## 1. Contexto e Declaração do Problema
O subsistema IPC do CoreAuth precisa expor uma interface pública rigorosa, segura e coesa para permitir a comunicação assíncrona entre o motor C e clientes externos via sockets Unix. Anteriormente, as operações de I/O de socket não possuíam estruturas padronizadas para encapsular o estado individual de cada cliente, os limites de *buffer* eram arbitrários e o tratamento de erros operacionais dependia de códigos genéricos ou inteiros puros (`int`), o que dificultava o rastreamento de falhas na inicialização do servidor.

Para garantir a previsibilidade do sistema, evitar dependências circulares e expor um contrato claro no cabeçalho `ipc_server.h`, tornou-se necessário modelar tipos de dados explícitos para contextos de cliente, parametrização de ambiente e um mecanismo formal de retorno de status de execução.

## 2. Alternativas Consideradas

* **Opção A: Utilização de descritores de arquivo isolados (`int fd`) e passagem de ponteiros brutos (`void *`)**
  * **Prós:** API simples e mínima de ser declarada no cabeçalho.
  * **Contras:** Ausência de controle de estado por sessão no nível de tipo de dados. OBRIGA a alocação dinâmica contínua (`malloc`) no arquivo de implementação `.c` para controlar buffers de entrada/saída. Torna o contrato frágil e propício a vazamentos de memória e erros de *type casting*.

* **Opção B: Abstração via Contrato Estático (`ipc_server.h`) com Tipos Fortes (`client_context_t`, `ipc_config_t`) e Enums de Diagnóstico (`ipc_status_t`)**
  * **Prós:** Encapsulamento completo de estado por cliente diretamente na `struct`, permitindo associação direta no *loop* de eventos (`epoll`). Parametrização determinística com *fallbacks* estáticos predefinidos via `#define`. Retorno de erros de subsistemas totalmente tipados e previsíveis (`ipc_status_t`). Integração nativa com a camada de banco de dados (`db_config_t`).
  * **Contras:** Aumenta ligeiramente a pegada de memória por cliente devido à inclusão fixa dos *buffers* de 1024 bytes na própria declaração da `struct`.

## 3. Decisões Tomadas

### Decisão 1: Encapsulamento do Estado de Conexão (`client_context_t`)
Optou-se pela **Opção B**. A estrutura `client_context_t` foi definida para centralizar todo o contexto de um cliente conectado:
* **Identificação e Autenticação:** Acoplamento direto do *File Descriptor* (`fd`) e da flag booleana de sessão (`is_authenticated`).
* **Buffers de E/S Estáticos:** Alocação de arrays contíguos fixos de 1024 bytes (`rx_buffer` e `tx_buffer`) com seus respetivos contadores de bytes e deslocamento (`rx_bytes`, `tx_bytes`, `tx_offset`). Isso permite que o manipulador de eventos leia e escreva de forma assíncrona sem realizar chamadas a `malloc`/`free`.

### Decisão 2: Parametrização Baseada em Padrões Seguros (`ipc_config_t` e `#define`)
A inicialização do servidor passou a ser orientada pela estrutura `ipc_config_t`, contendo os ponteiros de configuração do socket, token de autenticação, limite de conexões e timeout. Para garantir o funcionamento out-of-the-box e resiliência em falhas de configuração, foram definidos valores padrão imutáveis no cabeçalho:
* `DEFAULT_SOCKET_PATH` (`"/tmp/coreauth.sock"`)
* `DEFAULT_AUTH_TOKEN` (`"coreauth_secret_token"`)
* `DEFAULT_MAX_CONN` (`10`)
* `DEFAULT_TIMEOUT_MS` (`6000`)

### Decisão 3: Tipagem Estrita de Erros da Camada IPC (`ipc_status_t`)
Substituiu-se o retorno numérico genérico pelo enumeration `ipc_status_t`. O enum isola com precisão a origem das falhas de infraestrutura durante o ciclo de vida do servidor (`IPC_ERROR_SOCKET`, `IPC_ERROR_BIND`, `IPC_ERROR_LISTEN`, `IPC_ERROR_EPOLL`) e de dependências de persistência (`IPC_ERROR_DB`), retornando `IPC_SUCCESS` (`0`) em execuções nominais.

### Decisão 4: Acoplamento de Inicialização IPC-Database (`ipc_server_start`)
A assinatura da função `ipc_server_start` exige obrigatoriamente ponteiros para as configurações de IPC (`const ipc_config_t *`) e de banco de dados (`const db_config_t *`). Esta decisão garante no nível de compilação que o servidor IPC não pode ser inicializado sem ter visibilidade da camada de dados.

## 4. Consequências

### Positivas:
* **Contrato de API Claro e Previsível:** O cabeçalho `ipc_server.h` expõe uma interface limpa, auto-documentada e modular, contendo apenas duas funções principais (`ipc_server_start` e `ipc_server_stop`).
* **Segurança de Tipos e Tratamento de Erros:** A utilização de `ipc_status_t` força o chamador a tratar explicitamente cada modo de falha da inicialização do socket ou da dependência do MySQL.
* **Memória Determinística:** O uso de tamanhos fixos (`RX_BUFFER_SIZE` / `TX_BUFFER_SIZE`) no `client_context_t` permite que a pilha de execução saiba exatamente a quantidade de RAM alocada por cliente (2080 bytes) antes mesmo da conexão ser aceita.

### Negativas / Desafios:
* **Acoplamento do Cabeçalho com o Banco de Dados:** A inclusão de `../database/config.h` em `ipc_server.h` cria uma dependência direta entre o cabeçalho do IPC e a estrutura `db_config_t`, exigindo a presença dos arquivos de cabeçalho do banco na árvore de compilação do projeto.