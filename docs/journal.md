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