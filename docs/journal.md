# Diário Técnico de Engenharia - CoreAuth IPC

---

## Registro 1: Setup do Ambiente de Desenvolvimento e Banco de Dados

Dificuldade em definir a estrutura de diretórios do projeto, instalar as dependências necessárias para compilação em C no Linux e provisionar o servidor de banco de dados.
* **Solução:** Configuração do acesso SSH ao servidor de desenvolvimento Ubuntu. Instalação do meta-pacote `build-essential` (que agrupa GCC, G++ e Make), do depurador `gdb` para análise de falhas em tempo de execução e das bibliotecas `mysql-server` e `libmysqlclient-dev` para viabilizar a comunicação nativa em C com o MySQL.

---

## Registro 2: Integração Inicial com a API C do MySQL

Dificuldades na compreensão do fluxo de inicialização de memória e gerenciamento de conexões com o MySQL utilizando ponteiros, além da manipulação inadequada de saídas de erro do sistema.
* **Solução:** Implementação do ciclo básico de conexões por meio das funções `mysql_init` (que aloca memória dinamicamente para a estrutura de controle) e `mysql_real_connect`. O fluxo foi estruturado para direcionar mensagens detalhadas de falha via `mysql_error` para o canal de erro padrão `stderr`, encerrando a conexão graciosamente com `mysql_close` ao término das rotinas.

---

## Registro 3: Gerenciamento de Conexões e Protocolo de Dados Unificado

O modelo inicial de persistência sofria de severos vazamentos de memória e conexões ao abrir e fechar sessões com o banco a cada chamada de função de inserção. Além disso, o código sofria de vulnerabilidades críticas a ataques de *SQL Injection* decorrentes da concatenação direta de entradas de usuários no comando SQL por meio de `snprintf`.
* **Solução:** Centralização da sessão de conexão por meio de um ponteiro estático persistente (`global_connection`)
 Definição de rotinas isoladas de inicialização (`db_init`) e encerramento seguro (`db_close`)
 Paralelamente, foi especificado o protocolo de comunicação de rede `user_protocol_t` com tipos de largura de bytes fixa através de `<stdint.h>` e o atributo `__attribute__((packed))` para suprimir o alinhamento de bytes (padding) do compilador.

---

## Registro 4: Prevenção de SQL Injection com Prepared Statements

Insegurança na camada de persistência de dados devido ao uso de queries dinâmicas formadas por strings puras expostas a injeções de código malicioso

* **Solução:** Transição obrigatória para a API de Instruções Preparadas (*Prepared Statements*) do MySQL. O novo fluxo de persistência passou a utilizar funções seguras da biblioteca cliente do MySQL, realizando o pré-processamento estrutural da instrução e vinculando os parâmetros dinamicamente por meio de estruturas `MYSQL_BIND`. Isso garantiu que os parâmetros de entrada nunca sejam interpretados como comandos lógicos pelo interpretador SQL.

---

## Registro 5: Desacoplamento de Credenciais via Variáveis de Ambiente

Exposição de credenciais sensíveis de acesso ao banco diretamente no código-fonte, violando boas práticas de segurança.

* **Solução:** Implementação de suporte para variáveis de ambiente via `getenv()`, permitindo que as credenciais do banco sejam alimentadas externamente. Foi estruturado um modelo de configuração capaz de carregar valores a partir de um arquivo de ambiente oculto `.env` (orientado por um modelo de exemplo `.env.example`), cujas variáveis são injetadas e exportadas diretamente no ecossistema do processo por meio do arquivo `Makefile`.

---

## Registro 6: Robustez na Configuração, Criação Automática de Esquemas e Menu de Testes

Falhas em tempo de execução associadas a variáveis de ambiente ausentes ou vazias, falta de automação para provisionar a estrutura do banco e conflitos decorrentes do buffer do teclado (`\n` residual do Enter) que pulavam etapas de entrada do menu interativo de testes.
* **Solução:** 
  * Refinamento das validações de configuração para checar não apenas valores nulos, mas também strings vazias (`'\0'`).
  * Implementação da criação dinâmica do banco e da tabela `users` dentro da função de inicialização, que passou a receber a estrutura de parâmetros de conexão como um ponteiro constante (`const db_config_t *`) para evitar modificações acidentais na memória.
  * Criação de um ambiente local interativo (`main_test.c`) aplicando a limpeza do buffer de entrada padrão antes das capturas com `scanf`, assegurando a leitura correta de nomes com espaços e e-mails.

  ---

---

## Registro 7: Parametrização de Porta, Otimização do InnoDB e Consolidação Documental

Limitação no acoplamento da porta do banco, anteriormente travada no valor padrão (`0`/`3306`).

Gargalo de I/O em disco gerado pelo desgaste físico e latência de gravação no SSD NVMe devido aos constantes acessos de gravação direta (*flushes* síncronos) no banco de dados.

Ausência de uma base documental técnica formalizada para rastreabilidade de decisões e consumo de recursos de baixo nível.
* **Solução:** 
  * **Injeção de Porta via Ambiente:** Inclusão do campo `port` na struct `db_config_t`, permitindo a parametrização dinâmica da porta TCP/IP por variáveis de ambiente.
  * **Mecanismo InnoDB e Otimização de I/O:** Configuração padrão do motor de tabelas para InnoDB, garantindo suporte nativo e conformidade estrita às propriedades ACID. Para mitigar a concorrência de escrita no SSD NVMe, aplicou-se a diretiva `SET GLOBAL innodb_flush_log_at_trx_commit = 2`. Isso flexibiliza o fluxo, permitindo que o log de transações (*Redo Log*) seja enviado ao cache do sistema operacional a cada transação e gravado em disco fisicamente em ciclos assíncronos de 1 segundo, reduzindo drasticamente o overhead de I/O e o desgaste do componente de hardware.
  * **Consolidação Documental:** Estruturação de uma robusta base de documentação de engenharia contendo:
    * **Diretrizes de Arquitetura Geral:** Visão macro sobre a organização do fluxo do sistema.
    * **Architecture Decision Records (ADRs):** Justificativa e registro formal das principais escolhas de design do software.
    * **Journal (Diário Técnico):** Registro cronológico detalhando desafios e soluções de engenharia do projeto.
    * **Memory Layout (Mapeamento de Memória):** Documento técnico analítico mapeando o consumo físico em bytes das structs para garantir o controle rigoroso da memória de baixo nível.

    ---

## Registro 8: Inclusão da Função de Visualização de Dados e Adequação da Estrutura de Configuração

Necessidade de expor uma funcionalidade de visualização de registros para o usuário e correção de um tipo de dado incompatível na estrutura de configuração inicial, que gerava inconsistência ao inicializar a conexão com o banco de dados.

**Solução:**
* **Implementação da Consulta:** Criação da função de listagem de dados, a qual realiza a extração dos registros diretamente do banco, aloca os resultados temporariamente na memória RAM (`mysql_stmt_store_result`) e transfere os valores para as variáveis vinculadas aos buffers de recepção (`mysql_stmt_fetch`).
* **Refatoração da Estrutura:** Alteração do tipo de dado do campo `port` na estrutura `db_config_t`. O campo, que antes era declarado como `const char *`, passou a ser um `unsigned int` (ocupando 4 bytes em memória). Essa modificação alinha a estrutura ao tipo primitivo exato exigido pela assinatura da função `mysql_real_connect`.
* **Atualização do Ambiente de Testes:** Modificação do arquivo `main_test.c` para integrar a nova rotina de exibição de dados, permitindo a validação e a execução de testes dinâmicos locais.

> **Commits de Referência:** `93648bf` | `5f656c8` | `f16ac44`

---

## Registro 9: Inclusão de Função de Edição de Dados e Validação de Escopo por ID

Necessidade de implementar uma funcionalidade para a alteração cadastral de usuários no banco de dados, restringindo a mutabilidade aos campos informativos e garantindo que o identificador único permaneça estático para evitar colisões de dados.

**Solução:**
* **Implementação da Edição:** Criação da função de atualização de dados. As strings são recebidas como `const char *` para blindar os buffers originais contra modificações involuntárias.
* **Mecanismo de Validação:** Integração da função `mysql_stmt_affected_rows` imediatamente após a execução do statement. Isso permite separar logicamente o sucesso da operação (retorno `0`), da ausência do registro no banco (retorno `1` para ID inexistente), além de mapear falhas críticas de infraestrutura (retorno `-1`).
* **Atualização do ambiente de testes:** arquivo `main_test.c` foi atualizado para incluir a função de edição de dados do usuário.

---

## Registro 10: Inclusão de Função de Exclusão de Dados

Necessidade de implementar a funcionalidade de deleção de registros de usuários no banco de dados, utilizando uma abordagem segura que minimize o uso de recursos e garanta que nenhuma remoção acidental em larga escala ocorra.

**Solução:**
* **Implementação da Exclusão:** Criação da função de deleção direta baseada em *Prepared Statements*, parametrizando o array `MYSQL_BIND` com um único elemento de tamanho reduzido (`MYSQL_TYPE_LONG`) mapeado estritamente à cláusula condicional da query (`DELETE FROM users WHERE id = ?`).
* **Mecanismo de Validação:** Utilização da função `mysql_stmt_affected_rows` logo após a execução do comando atômico de deleção. A lógica foi desenhada para retornar `0` em caso de exclusão real do registro, `1` caso o ID fornecido não corresponda a nenhum usuário existente na base (evitando falsos positivos de sucesso), e `-1` para falhas físicas na camada de persistência.
* **Otimização de Escopo:** Redução do overhead de memória RAM do driver através do dimensionamento exato do buffer de binding (`bind[1]`), eliminando campos de metadados textuais desnecessários (como `.buffer_length` e `.length`) por se tratar de um tipo numérico de tamanho fixo.

---

## Registro 11: Função de teste para exibição completa dos dados

Surgiu a necessidade de visualizar todos os dados cadastrados para fins de validação do código e checagem do estado real da base.

**Solução:**
* **Função de exibição geral:** Criação de uma função chamada `db_list_users`, responsável por realizar a requisição e a captura de todos os registros armazenados no banco de dados. Por meio de um laço de repetição que valida o retorno de `mysql_stmt_fetch`, realiza-se a listagem completa dos dados. Caso a base esteja vazia ou ocorra uma falha física no streaming, o sistema dispara o aviso correspondente.
* **Código de testes refatorado:** além da função mencionada acima, foi realizado a refatoração da estrutura de testes `main_test.c` para se tornar uma interface segura e limpa de testes via CLI.

>**Observação importante:** Esta função foi projetada estritamente para testes de visualização em ambiente de desenvolvimento (CLI) e não integrará a arquitetura final da aplicação. Por essa razão, foi implementada de maneira simplificada, sem paginação de registros.

---

## Registro 12: Refatoração do Módulo de Banco de Dados

A estrutura anterior de gerenciamento do banco de dados possuía repetições desnecessárias de código (redundâncias), além de vulnerabilidades que poderiam causar vazamentos de memória (*memory leaks*) e falhas de segmentação (*segmentation faults*).

**Soluções aplicadas:**
* **Modularização e Abstração:** O código foi desacoplado, e a lógica de infraestrutura foi movida para o novo arquivo `db_utils.c`. Este submódulo agora centraliza a inicialização da conexão e o tratamento base de erros. Também foi implementada uma função utilitária para centralizar e padronizar o ciclo de vida (preparação, vinculação e execução) de *Prepared Statements*.
* **Eliminação de Números Mágicos:** Foi introduzido o tipo enumerado `db_status_t` (`DB_SUCCESS`, `DB_WARNING`, `DB_CRITICAL_ERROR`) para abstrair os retornos das funções. Isso eliminou o uso de constantes numéricas arbitrárias e aumentou significativamente a legibilidade e a manutenibilidade do código.
* **Segurança de Memória (String Safety):** Foi adicionada uma validação rigorosa para strings oriundas do banco de dados que correm o risco de omitir o caractere de encerramento (`\0`). O código agora força a terminação nula baseada no ponteiro de tamanho retornado pela API do MySQL (`len_out`), mitigando riscos de *buffer overflow*.
* **Consistência de Assinaturas:** Funções do CRUD que antes retornavam inteiros genéricos foram atualizadas para retornar estritamente o enum `db_status_t`, garantindo previsibilidade no fluxo de controle.
* **Adequação da Interface:** O arquivo `main_test.c` foi adaptado para consumir os novos tipos enumerados, isolando os retornos das funções em variáveis locais para evitar o disparo duplicado de consultas ao banco de dados (*side effects*) dentro das estruturas condicionais.

---

## Registro 13: Testes de Desempenho, Estresse e Emissão de Relatórios

Implementação de uma suíte de testes de estresse (carga) para mensurar a eficiência, a vazão de transações por segundo (TPS) e a segurança de memória do motor de banco de dados em C.

**Soluções aplicadas:**
* **Modularização dos Testes:** Para manter a organização e o desacoplamento do código de produção, os módulos de teste foram consolidados no diretório `tests/`, incluindo a interface interativa CLI (`main_test.c`) e a rotina de estresse (`stress_test.c`).
* **Automação de Relatórios em Markdown:** Foi implementada uma rotina de geração dinâmica de relatórios em um subdiretório dedicado (`tests/reports/`). O arquivo é nomeado de forma sequencial e incremental (`stress_test_report_X.md`) para garantir a rastreabilidade e evitar sobrescrita entre execuções.
* **Coleta de Métricas do Ambiente (Hardware Profiling):** Integração com APIs e arquivos do sistema operacional (`/proc/cpuinfo` e `/proc/meminfo`) para registrar automaticamente o modelo do processador (CPU) e a memória RAM disponível (convertida dinamicamente para GB) diretamente no cabeçalho do relatório de testes.
* **Medição de Alta Precisão:** Utilização do *clock* monotônico POSIX (`clock_gettime` com `CLOCK_MONOTONIC`) para calcular o tempo real de execução com precisão de nanossegundos, imune a alterações do relógio do sistema.
* **Gestão Rigorosa de Recursos:** Ajuste do ciclo de vida dos *Prepared Statements* em `db_insert_user`, garantindo o fechamento explícito do `MYSQL_STMT` (`mysql_stmt_close`) tanto em fluxos de sucesso quanto em tratamentos de erro, eliminando o esgotamento de *file descriptors* e vazamentos de memória sob carga massiva.

> **Observação operacional:** Como o teste de estresse gera e-mails baseados na variável de iteração do loop, registros duplicados na chave primária/única ocorrerão se o banco de dados não for limpo entre as baterias de testes. Além disso, a quantidade de testes deve ser editada diretamente no código.

## Registro 14: Teste de desempenho em memória com desativação do autocommit

O código anterior possuía gargalos que, teoricamente, reduziam a performance e limitavam a métrica de TPS. Por isso, foi necessário refatorar a base de código referente às operações CRUD do banco de dados. Embora a taxa de transações por segundo tenha se mantido estável após as alterações, o tempo ativo de CPU caiu expressivamente para menos de 2% do tempo total durante o processamento de 10.000 cadastros.

**Solução:**
* Implementação da desativação temporária do mecanismo de *autocommit* na conexão com o banco de dados durante as rotinas de estresse. Isso permite o agrupamento das inserções em memória RAM (*batch processing*), eliminando o gargalo de escrita síncrona em disco e permitindo medir a capacidade real de processamento em RAM do motor em C.

---

## Registro 15: Refatoração de toda a estrutura do banco de dados, visando aumento de desempenho e segurança

O código anterior possuía gargalos que, teoricamente, reduziam a performance e limitavam a métrica de TPS. Por isso, foi necessário refatorar a base de código referente às operações CRUD do banco de dados. Embora a taxa de transações por segundo não tenha dado um salto, o tempo ativo de CPU caiu expressivamente em relação ao tempo total durante o processamento de 10.000 cadastros.

**Solução:**
* **Inicialização Única de Prepared Statements (`db_utils.c`):** Reestruturação do ciclo de vida das instruções SQL. As consultas de `INSERT`, `SELECT`, `UPDATE` e `DELETE` passaram a ser alocadas e preparadas no MySQL apenas uma vez, durante o arranque da aplicação (`db_init`). Com isso, as rotinas do `db.c` realizam apenas o vínculo de parâmetros (*bind*) e a execução do comando, eliminando o *overhead* repetitivo de transporte, parse e compilação de SQL no banco de dados a cada chamada.
* **Paginação por Cursor (Keyset Pagination) e Índice Composto:** Substituição da busca convencional pelo modelo de paginação por cursor (`WHERE name LIKE ? AND (name > ? OR (name = ? AND id > ?)) ORDER BY name ASC, id ASC LIMIT 50`). Para suportar consultas de alta performance em bases com milhões de registros, adicionou-se o índice composto `idx_name_id (name, id)` na tabela `users` da engine `InnoDB`, garantindo complexidade de busca $O(\log N)$ sem varredura completa da tabela.
* **Suporte ao Nível de Acesso (RBAC):** Atualização da tabela do banco de dados e das estruturas de dados (`user_protocol_t` e `user_data_t`) para trafegar a coluna `access_level` (`TINYINT UNSIGNED`). Na C-API do MySQL, o mapeamento foi padronizado com `MYSQL_TYPE_TINY` e variáveis `uint8_t` (1 byte), assegurando o alinhamento correto de memória sem o risco de truncamento ou leitura de bytes inválidos na *stack*.
* **Gestão Rigorosa de Memória na Stack:** Eliminação da função `memset` executada repetidamente em tempo de execução, substituída pela inicialização direta na compilação (`MYSQL_BIND bind[...] = {0};`). Remoção de cópias redundantes de variáveis numéricas (como ponteiros temporários de ID) e adição de tratamento explícito do caractere finalizador (`\0`) com limites estritos (`len_name`, `len_email`) nas operações de leitura e escrita de strings, zerando o crescimento de RAM (*0 KB de Memory Leak*).
* **Robustez no Módulo de Configuração (`config.c`):** Substituição do conversor vulnerável `atoi` por `strtol` na leitura de variáveis de ambiente (`DB_PORT`). A implementação passou a validar o registrador `errno`, ponteiros de fim de string e o intervalo válido de portas TCP (1 a 65535).
* **Conformidade de Segurança e Resiliência DevSecOps:** Remoção da flag depreciada de reconexão automática (`MYSQL_OPT_RECONNECT`), cuja execução silenciosa destruía os descritores dos *Prepared Statements* e causava falhas de segmentação (*segfaults*). Eliminação de comandos administrativos dinâmicos (`SET GLOBAL`) de dentro do código C, readequando o motor ao Princípio do Privilégio Mínimo (PoLP) para que a aplicação rode com usuário restrito de banco de dados.

---

## Registro 17: Health Check e Auto-Healing de Conexão (db_ping)

Por manter a conexão com o MySQL persistentemente aberta no processo C, existe o risco de a sessão ser encerrada por inatividade durante períodos ociosos (devido ao parâmetro `wait_timeout` do banco de dados ou políticas do Kernel/firewall).

**Solução:**
* **Implementação de Health Check com Auto-Healing (`db_ping`):** Criação da função responsável por despachar um pacote de teste leve (`mysql_ping`) para verificar a saude do socket. Caso a conexão esteja inativa ou corrompida, o sistema destrói com segurança os ponteiros dos *Prepared Statements* obsoletos (`db_close`), aciona a reconexão automática (`db_init`) e re-prepara toda a estrutura de comandos na memória, garantindo resiliência contínua sem vazamento de recursos.

---

## Registro 18: Especificação de Interface da Camada de IPC Não Bloqueante (`ipc_server.h`)

Para viabilizar a comunicação assíncrona de alto desempenho via sockets de domínio Unix e evitar bloqueios na execução do motor, fez-se necessária a definição de uma interface de IPC baseada em laço de eventos (`epoll`) e buffers de estado por cliente.

**Solução:**
* **Modelagem de Estruturas de Sessão e Configuração (`ipc_server.h`):** Criação dos tipos de dados e contratos de API para o servidor de IPC. A estrutura `client_context_t` centraliza o estado individual de cada conexão, alocando buffers isolados de 1024 bytes para recepção (`rx_buffer`) e envio (`tx_buffer`), além de ponteiros de deslocamento (`tx_offset`) e controle de autenticação (`is_authenticated`). O contrato abstrai a inicialização do serviço via `ipc_config_t`, vinculando a pilha de IPC ao banco de dados em `ipc_server_start` e estabelece o enum estrito `ipc_status_t` para tratamento previsível de falhas de sistema (*socket*, *bind*, *listen*, *epoll* e *DB*).

---

## Registro 19: Implementação das Funções Auxiliares de Socket e Não Bloqueio (`ipc_server.c`)

Para garantir a correta inicialização do ciclo de vida do servidor de IPC e impedir que operações de I/O travem o loop de eventos principal, implementou-se a camada base de gerenciamento de file descriptors e abstração de sockets Unix.

**Solução:**
* **Modo Não Bloqueante e Inicialização do Socket (`ipc_server.c`):** Desenvolvimento das funções utilitárias internas de baixo nível. A função `set_nonblocking` utiliza chamadas de sistema `fcntl` para consultar (`F_GETFL`) e aplicar (`F_SETFL`) a flag `O_NONBLOCK` via operação *bitwise OR*, preservando as configurações nativas do socket. A função `bind_socket` centraliza a criação do socket Unix (`AF_UNIX`, `SOCK_STREAM`), a limpeza preventiva de resíduos de instâncias anteriores no sistema de arquivos (`unlink`), a vinculação de endereço (`bind`), a conversão do descriptor para modo não bloqueante e a abertura da fila de escuta (`listen`), tratando falhas em cada etapa com fechamento seguro de recursos (`close`).

---

## Registro 20: Implementação do Gerenciamento de Memória do Contexto de Clientes (`ipc_server.c`)

Para gerenciar o ciclo de vida e a integridade do estado de cada cliente conectado sem comprometer o footprint de memória ou a estabilidade do servidor IPC, implementou-se a camada base de alocação e destruição do contexto individual por conexão.

**Solução:**
* **Alocação e Desalocação Segura de Memória (`ipc_server.c`):** Desenvolvimento das funções utilitárias internas `client_context` e `client_destroy`. A função `client_create` valida o file descriptor recebido, aloca a estrutura `client_context_t` na Heap usando `malloc` e utiliza `memset` para zerar uniformemente o bloco de memória, garantindo a inicialização implícita de buffers, offsets e do estado de autenticação como falso, atribuindo o descriptor ao final. A função `client_destroy` intercepta chamadas com guarda contra ponteiros nulos, encerra o socket de forma segura através da instrução `close` redefinindo o descriptor para `-1` para prevenir *double close*, e libera o bloco de memória alocado através de `free`, prevenindo *memory leaks* e *dangling pointers*.

---

## Registro 21: Implementação da Rotina de Aceitação de Conexões Não Bloqueantes (`ipc_server.c`)

Para garantir a drenagem completa de conexões pendentes no socket e o registro assíncrono de novos clientes sem bloquear a execução do servidor, implementou-se a função de tratamento de conexões `handle_accept`.

**Solução:**
* **Aceitação Não Bloqueante e Registro no Epoll (`ipc_server.c`):** Desenvolvimento da rotina interna `handle_accept`. A função executa um laço contínuo invocando a chamada de sistema `accept` para capturar todos os clientes pendentes até que o retorno `-1` com `errno` igual a `EAGAIN` ou `EWOULDBLOCK` sinalize o esvaziamento da fila do kernel. Para cada cliente aceito, a função aplica o modo não bloqueante (`set_nonblocking`), aloca a estrutura de estado do cliente (`client_context_t`) e registra o descriptor no `epoll` utilizando notificação orientada a borda (`EPOLLET`) associada ao evento `EPOLLIN`. Em caso de erro em qualquer etapa da inicialização do cliente, o contexto é descartado e o descriptor é encerrado com segurança.

---

## Registro 22: Inicialização e Loop de Eventos do Servidor IPC (`ipc_server.c`)

Para prover a infraestrutura principal do servidor IPC, garantir a orquestração assíncrona de conexões e manter a resiliência operacional do sistema, implementou-se a função de inicialização e ciclo de vida `ipc_server_start`.

**Solução:**
* **Inicialização da Infraestrutura e Ciclo Reativo (`ipc_server.c`):** Desenvolvimento da rotina principal `ipc_server_start`. A função valida as configurações recebidas (aplicando *fallbacks* para valores padrão), realiza o *bind* do socket Unix e instancia a interface do Kernel via `epoll_create1`. O descriptor do servidor (`server_fd`) é devidamente cadastrado para monitoramento de entrada (`EPOLLIN`). Em seguida, a rotina entra em um laço infinito que invoca `epoll_wait` para aguardar eventos de rede em lote. O ciclo lida com interrupções de sinais do sistema (`EINTR`), realiza pings preventivos ao banco de dados (`db_ping`) durante períodos de ociosidade (*timeout*) e faz o *dispatch* reativo dos eventos: redirecionando novas conexões para a rotina `handle_accept` ou recuperando o contexto (`client_context_t`) de clientes ativos. Todo o fluxo conta com tratamento rigoroso de exceções para encerramento e liberação segura de recursos (*cleanup*) em caso de falhas na inicialização.

---

## Registro 23: Mecanismo de Interrupção e Encerramento Gracioso (*Graceful Shutdown*) (`ipc_server.c`)

Para assegurar a interrupção segura do servidor, a liberação adequada de recursos e o tratamento previsível de sinais do sistema operacional sem finalização abrupta do processo, implementou-se a infraestrutura de *graceful shutdown*.

**Solução:**
* **Gestão Assíncrona de Sinais e Parada do Servidor (`ipc_server.c`):** Implementação do manipulador de sinais `sig_handler` e da rotina de inicialização `setup_signals`. A solução utiliza a API `sigaction` para capturar os sinais `SIGINT` (interrupção via terminal) e `SIGTERM` (solicitação de encerramento do SO), modificando com segurança atômica a flag `keep_running` (declarada como `static volatile sig_atomic_t`). Adicionalmente, disponibilizou-se a função pública `ipc_server_stop` para permitir a interrupção programática do servidor por outros módulos da aplicação. Esse mecanismo garante que o loop de eventos seja interrompido de forma controlada, permitindo a execução das rotinas de limpeza (*cleanup*), fechamento de descritores e remoção do socket no sistema de arquivos.

---

## Registro 24: Rotina de Leitura Não Bloqueante e Drenagem de Sockets (`ipc_server.c`)

Para garantir a ingestão contínua de dados de clientes sem bloquear o loop de eventos e prover o descarte seguro de conexões inválidas ou encerradas, implementou-se a função interna `handle_client_data`.

**Solução:**
* **Drenagem de Buffer e Tratamento de Eventos de Leitura (`ipc_server.c`):** Desenvolvimento da rotina `handle_client_data`. A função executa um laço contínuo de leitura (`read`) sobre o descriptor do cliente até que o erro `EAGAIN` ou `EWOULDBLOCK` confirme o esvaziamento completo do buffer do Kernel (padrão *Edge-Triggered*). O algoritmo realiza o cálculo dinâmico de espaço restante (`free_space`) e protege o sistema contra estouro de memória (*buffer overflow*). Caso o buffer seja totalmente preenchido sem processamento, ocorram erros não recuperáveis de leitura ou o cliente encerre a conexão de forma graciosa (sinalizado por `bytes_read == 0`), a função descadastra o descriptor da instância do `epoll` (`EPOLL_CTL_DEL`) e executa a liberação segura do contexto via `client_destroy`. Interrupções por sinais de sistema (`EINTR`) são tratadas com retransmissão imediata.

---

### Registro 25: Descarregamento Assíncrono de Buffer e Envio Não Bloqueante (`ipc_server.c`)

Para assegurar a transmissão de respostas e dados ao cliente sem causar travamentos no laço de eventos do *epoll*, implementou-se a rotina interna `handle_client_write`.

**Solução:**
* **Gerenciamento de Buffer de Transmissão e I/O Não Bloqueante (`ipc_server.c`):** Desenvolvimento da rotina `handle_client_write`. A função é responsável por descarregar os bytes acumulados no buffer de escrita do cliente (`tx_buffer`) diretamente para o *file descriptor* (`ctx->fd`). A transmissão ocorre através de um laço executando a syscall `write` com controle contínuo de deslocamento (`tx_offset`), garantindo o envio parcial de dados caso o buffer do Kernel atinja o limite e retorne `EAGAIN` ou `EWOULDBLOCK`. O algoritmo trata interrupções por sinais (`EINTR`) reapresentando a chamada e realiza o encerramento gracioso do contexto via `client_destroy` e remoção do *epoll* (`EPOLL_CTL_DEL`) em caso de erros críticos de I/O (`bytes_written < 0`). Ao concluir a entrega de todos os bytes (`tx_offset == tx_bytes`), os ponteiros de controle do buffer são zerados sem alocação dinâmica.

---

### Registro 26: Desempacotamento de Protocolo e Roteamento de Comandos (`ipc_server.c`)

Para realizar a desserialização atômica de pacotes recebidos pela rede e direcionar o processamento do CRUD para a camada do MySQL, implementou-se a função interna `parse_and_execute_command`.

**Solução:**
* **Parsing por Deslocamento e Execução de Comandos (`ipc_server.c`):** Desenvolvimento da rotina `parse_and_execute_command`. A função faz o *casting* direto do ponteiro do buffer de recepção (`rx_buffer`) para a estrutura `user_protocol_t` sem realizar cópias adicionais de memória (mecanismo *zero-copy*). Com base no campo `op_code`, um laço condicional (*switch*) mapeia e executa a respectiva instrução do banco de dados (`db_insert_user`, `db_find_user`, `db_edit_user` ou `db_delete_user`), gravando o byte de status resultante diretamente no início do buffer de transmissão (`tx_buffer`). O algoritmo ajusta o deslocamento do `rx_buffer` via `memmove` para descartar o pacote processado e, ao final, invoca a rotina `handle_client_write` para efetuar o envio assíncrono da resposta ao cliente.