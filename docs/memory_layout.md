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

| Campo | Tipo C | Tamanho do Ponteiro | Tamanho do Dado Real | Descrição Mecânica |
| :--- | :--- | :---: | :---: | :--- |
| `host` | `const char *` | 8 Bytes | **Dinâmico** | Endereço de IP ou domínio do banco (ex: `127.0.0.1`) |
| `port` | `const char *` | 8 Bytes | **Dinâmico** | Porta numérica TCP/IP (ex: `3306`) |
| `user` | `const char *` | 8 Bytes | **Dinâmico** | Usuário de acesso ao banco de dados |
| `pass` | `const char *` | 8 Bytes | **Dinâmico** | Senha de autenticação |
| `db_name`| `const char *` | 8 Bytes | **Dinâmico** | Nome do Schema a ser provisionado e utilizado |

### Razões de Design de Baixo Nível

1. **Tamanho Estático da Estrutura com Dados Apontados Dinâmicos:**
   Diferente de arrays de caracteres fixos, esta estrutura armazena exclusivamente ponteiros de leitura (`const char *`). Em arquiteturas de 64 bits, cada ponteiro ocupa exatamente 8 bytes, fazendo com que o tamanho total ocupado pela struct na memória RAM seja fixo em **40 bytes** (5 campos × 8 bytes), independentemente do tamanho das strings de configuração recebidas.

2. **Gerenciamento de Ciclo de Vida via Ambiente:**
   Como as variáveis de ambiente já residem em um bloco de memória gerenciado pelo próprio sistema operacional durante a inicialização do processo, a struct apenas referencia esses endereços. Isso elimina a necessidade de alocações dinâmicas manuais (`malloc`/`free`) e evita o risco de fragmentação ou vazamento de memória (*memory leaks*) na camada do motor em C.

* **Tamanho total da Struct:** 40 Bytes fixos na memória RAM (em sistemas de 64 bits).