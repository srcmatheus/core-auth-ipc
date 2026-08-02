# Layout de Alinhamento de Memória - CoreAuth IPC

Para evitar desalinhamento de dados (*data alignment*) e quebras de integridade ao enviar pacotes do motor para o banco (ou futuramente entre a API em Java e o C via memória compartilhada), as estruturas de dados foram documentadas quanto ao seu tamanho exato em bytes.

### 1. Estrutura: `user_protocol_t`
Armazena a carga de dados (payload) do usuário que trafega no sistema.

| Campo | Tipo C | Tamanho | Descrição Mecânica |
| :--- | :--- | :---: | :--- |
| `op_code` | `uint8_t` | 1 Byte | Identificador do comando/operação (ex: 1 = Cadastro, 2 = Login) |
| `access_level` | `uint8_t` | 1 Byte | Nível de privilégio ou permissão do usuário no sistema |
| `full_name` | `char[100]` | 100 Bytes | Nome completo do usuário (preenchido com `\0` no final) |
| `email` | `char[100]` | 100 Bytes | E-mail corporativo único utilizado para autenticação |

### Razões de Design de Baixo Nível

1. **Uso de `__attribute__((packed))`:**
   Sem esse atributo, o GCC (por padrão em CPUs x86_64) alinharia os campos adicionando 2 bytes vazios (*padding*) logo após o campo de 1 byte `access_level` para alinhar o início do array de caracteres a um endereço múltiplo de 4 bytes. O empacotamento força o layout a ser contíguo, poupando tráfego e facilitando a leitura binária bruta pelo Java.

2. **Otimização de Descarte Precoce (Early Discard):**
   Os metadados críticos de controle (`op_code` e `access_level`) foram colocados de propósito nos primeiros 2 bytes da struct. Isso permite que qualquer listener do canal de IPC leia apenas um cabeçalho curto de 2 bytes para validar a permissão e o tipo do comando antes de alocar memória para ler e processar os 202 bytes restantes de dados textuais.

* **Tamanho total:** 202 Bytes cravados na memória RAM.

---

### 2. Estrutura: `db_config_t`
Carrega os ponteiros de configuração de ambiente necessários para estabelecer uma conexão isolada com o driver `libmysqlclient`.

| Campo | Tipo C | Tamanho na Struct | Tamanho do Dado Real | Descrição Mecânica |
| :--- | :--- | :---: | :---: | :--- |
| `host` | `const char *` | 8 Bytes | **Dinâmico** | Endereço de IP ou domínio do banco (ex: `127.0.0.1`) |
| `port` | `unsigned int` | 4 Bytes | 4 Bytes | Porta numérica TCP/IP (ex: `3306`) |
| `user` | `const char *` | 8 Bytes | **Dinâmico** | Usuário de acesso ao banco de dados |
| `pass` | `const char *` | 8 Bytes | **Dinâmico** | Senha de autenticação |
| `db_name`| `const char *` | 8 Bytes | **Dinâmico** | Nome do Schema a ser provisionado e utilizado |

### Razões de Design de Baixo Nível

1. **Tamanho Estático da Estrutura com Dados Apontados Dinâmicos:**
   Diferente de arrays de caracteres fixos, esta estrutura armazena exclusivamente ponteiros de leitura (`const char *`) e um valor inteiro direto. Em arquiteturas de 64 bits, cada ponteiro ocupa exatamente 8 bytes e o `unsigned int` ocupa 4 bytes. Devido às regras de alinhamento de memória do compilador (*padding*), o `unsigned int` de 4 bytes será seguido por 4 bytes invisíveis de preenchimento para alinhar o próximo ponteiro a um endereço múltiplo de 8. Isso faz com que o tamanho total ocupado pela struct na memória RAM seja fixo em **40 bytes** (4 ponteiros × 8 bytes + 4 bytes do `int` + 4 bytes de *padding*), independentemente do tamanho das strings de configuração recebidas.

2. **Gerenciamento de Ciclo de Vida via Ambiente:**
   Como as variáveis de ambiente já residem em um bloco de memória gerenciado pelo próprio sistema operacional durante a inicialização do processo, a struct apenas referencia esses endereços no caso das strings. Isso elimina a necessidade de alocações dinâmicas manuais (`malloc`/`free`) e evita o risco de fragmentação ou vazamento de memória (*memory leaks*) na camada do motor em C.

* **Tamanho total da Struct:** 40 Bytes fixos na memória RAM (em sistemas de 64 bits, já contabilizando o alinhamento/*padding*).

---

### 3. Estrutura: `client_context_t`
Gerencia a sessão de cada cliente conectado ao servidor IPC, armazenando o estado da conexão, controle de autenticação e os buffers acumulativos de leitura (`rx`) e escrita (`tx`) para operações assíncronas de I/O não bloqueante.

| Campo | Tipo C | Tamanho | Descrição Mecânica |
| :--- | :--- | :---: | :--- |
| `fd` | `int` | 4 Bytes | File Descriptor do socket do cliente retornado pela chamada `accept()` |
| `is_authenticated` | `bool` | 1 Byte | Flag de controle do estado de handshake/autenticação da sessão |
| *Padding Interno* | — | 3 Bytes | Preenchimento automático para alinhar o início do array de bytes |
| `rx_buffer` | `uint8_t[1024]` | 1024 Bytes | Buffer circular de recepção para acumular fragmentos de streaming do socket |
| `rx_bytes` | `size_t` | 8 Bytes | Quantidade atual de bytes pendentes acumulados no `rx_buffer` |
| `tx_buffer` | `uint8_t[1024]` | 1024 Bytes | Buffer de transmissão para enfileirar payloads pendentes de envio |
| `tx_bytes` | `size_t` | 8 Bytes | Quantidade total de bytes que ainda precisam ser enviados via `tx_buffer` |
| `tx_offset` | `size_t` | 8 Bytes | Deslocamento (ponteiro de leitura) de quantos bytes já foram transmitidos |

#### Razões de Design de Baixo Nível

1. **Garantia de Integridade em I/O Não Bloqueante (`SOCK_STREAM`):** Como conexões `SOCK_STREAM` não preservam os limites dos pacotes e podem fragmentar chamadas de rede, os buffers fixos de 1024 bytes (`rx_buffer` e `tx_buffer`) atuam como acumuladores de estado por cliente. Isso permite construir e processar structs completas (como a `user_protocol_t`) somente após receber a carga inteira, lidando com respostas parciais quando o kernel sinaliza `EAGAIN` / `EWOULDBLOCK` e eliminando a necessidade de *threads* dedicadas ou bloqueios de execução com `usleep`.

2. **Gerenciamento Descentralizado via `epoll` (`data.ptr`):** Instanciar essa estrutura por cliente e associar seu ponteiro ao evento do `epoll` (`ev.data.ptr`) elimina a dependência de matrizes fixas indexadas diretamente pelo valor de `fd`. Isso blinda o sistema contra acessos fora dos limites (*out-of-bounds*) caso o sistema operacional atribua descritores de arquivo elevados (acima de `MAX_FDS`).

3. **Cálculo de Alinhamento de Memória (64 bits):** * Os dois primeiros campos (`int` = 4 bytes e `bool` = 1 byte) totalizam 5 bytes. O compilador insere **3 bytes de padding** para que o array subsequente fique alinhado.
   * `rx_buffer` (1024 bytes) e `rx_bytes` (8 bytes) ocupam 1032 bytes contíguos.
   * `tx_buffer` (1024 bytes), `tx_bytes` (8 bytes) e `tx_offset` (8 bytes) ocupam 1040 bytes.

* **Tamanho total da Struct:** 2080 Bytes fixos na memória RAM por cliente ativo.

---

### 4. Estrutura: `ipc_config_t`
Armazena as definições de inicialização do servidor de IPC, controlando o socket de domínio Unix, limites de tráfego e timeouts do laço de eventos.

| Campo | Tipo C | Tamanho na Struct | Tamanho do Dado Real | Descrição Mecânica |
| :--- | :--- | :---: | :---: | :--- |
| `socket_path` | `const char *` | 8 Bytes | **Dinâmico** | Caminho absoluto do ficheiro `.sock` no SO (ex: `/tmp/coreauth.sock`) |
| `auth_token` | `const char *` | 8 Bytes | **Dinâmico** | Token estático utilizado no handshake para autorizar a conexão |
| `max_connections`| `int` | 4 Bytes | 4 Bytes | Limite máximo de conexões aceitas e tamanho da fila do `listen()` |
| `timeout_ms` | `int` | 4 Bytes | 4 Bytes | Tempo de timeout em milissegundos passado ao `epoll_wait()` |

#### Razões de Design de Baixo Nível

1. **Layout Compacto e Perfeitamente Alinhado (Sem Padding):** A ordem de declaração dos membros foi estruturada para otimização de alinhamento em arquiteturas 64 bits. Os dois ponteiros de 8 bytes (`socket_path` e `auth_token`) ocupam os primeiros 16 bytes. Os dois inteiros de 4 bytes (`max_connections` e `timeout_ms`) vêm em seguida, somando exatamente 8 bytes. Como a soma total resulta em um valor múltiplo natural de 8 (24 bytes), o compilador não precisa adicionar **nenhum byte de padding** de preenchimento ao final ou entre os campos.

2. **Ponteiros Read-Only de Baixa Sobrecarga:** Ao utilizar `const char *` para as propriedades de texto (`socket_path` e `auth_token`), a estrutura armazena apenas os endereços de memória dos literais ou variáveis alocadas no chamador. Isso evita cópias desnecessárias de buffer (`strcpy`), reduz a pegada de memória e permite inicializar o servidor IPC de forma instantânea através de uma configuração estática.

* **Tamanho total da Struct:** 24 Bytes fixos na memória RAM (em sistemas de 64 bits).