# ADR 0004: Reutilização de Prepared Statements, Paginação por Cursor e Conformidade DevSecOps no Módulo de Banco de Dados

* **Status:** Aceito
* **Data:** 2026-07-26
* **Autor:** Matheus Nascimento

---

## 1. Contexto e Declaração do Problema
O módulo de banco de dados em C apresentava gargalos de processamento resultantes do ciclo de vida ineficiente de consultas SQL, alocando, preparando e destruindo *Prepared Statements* a cada operação CRUD. Além disso, a busca de usuários não possuía um mecanismo de paginação escalável para volumes massivos de dados, o parsing do arquivo de configuração apresentava fragilidades e existiam rotinas que violavam boas práticas de segurança, como a execução de comandos administrativos dinâmicos (`SET GLOBAL`).

Para preparar o motor em C para produção com alta vazão e baixo consumo de recursos do host, tornou-se imperativo refatorar a camada de persistência, otimizar a gestão de memória na *stack*, implementar paginação por cursor e adequar o código aos princípios de DevSecOps.

## 2. Alternativas Consideradas

* **Opção A: Manter a preparação dinâmica de queries por requisição e paginação via `OFFSET`**
  * **Prós:** Menor complexidade no código de inicialização e facilidade na construção de queries dinâmicas simples.
  * **Contras:** Alto consumo de CPU no banco de dados para realizar o *parse* repetitivo de SQL. Degradação severa de performance $O(N)$ em tabelas com milhões de registros devido ao descarte de linhas no `OFFSET`. Riscos de segurança ao manter funcionalidades depreciadas e dependência de permissões administrativas (`SUPER`) no usuário da aplicação.

* **Opção B: Inicialização estática de Prepared Statements, Keyset Pagination, RBAC nativo e endurecimento DevSecOps**
  * **Prós:** Redução expressiva do tempo ativo de CPU ao compilar as instruções SQL apenas uma vez no arranque (`db_init`). Busca eficiente em tempo logarítmico $O(\log N)$ utilizando paginação por cursor apoiada por índice composto. Mapeamento preciso de tipos numéricos (`uint8_t`/`TINYINT`) e inicialização direta na *stack* (`{0}`). Conformidade estrita com o Princípio do Privilégio Mínimo (PoLP) e garantia de resiliência sem *memory leaks*.
  * **Contras:** Exige um contrato mais rígido de navegação entre o chamador e o banco de dados (gerenciamento dos cursores de `last_name` e `last_id`) e reestruturação dos arrays de `MYSQL_BIND`.

## 3. Decisões Tomadas

### Decisão 1: Reutilização Global de Prepared Statements (`db_utils.c`)
Optou-se pela **Opção B**. As instruções SQL de `INSERT`, `SELECT`, `UPDATE` e `DELETE` foram centralizadas na estrutura `db_stmts` e preparadas exatamente uma vez durante a execução da função `db_init`. As funções no `db.c` passaram a realizar apenas o vínculo (*bind*) de parâmetros e a execução, eliminando o *overhead* de parsing no MySQL.

### Decisão 2: Paginação por Cursor (Keyset Pagination) com Índice Composto
A busca de usuários em `db_find_user` foi refatorada para utilizar cursores baseados nas últimas colunas ordenadas (`WHERE name LIKE ? AND (name > ? OR (name = ? AND id > ?)) ORDER BY name ASC, id ASC LIMIT 50`). Para suportar essa consulta sem varredura completa da tabela, adicionou-se o índice composto `idx_name_id (name, id)` na definição do `InnoDB`.

### Decisão 3: Mapeamento Estrito de RBAC e Otimização de Memória na Stack
Inclusão do campo `access_level` (`TINYINT UNSIGNED`) na tabela de banco de dados e nas estruturas de transporte (`user_protocol_t` e `user_data_t`). As chamadas custosas de `memset` em tempo de execução foram removidas e substituídas pela inicialização limpa na *stack* (`MYSQL_BIND bind[...] = {0};`). O tipo do nível de acesso foi padronizado em C como `uint8_t` acoplado a `MYSQL_TYPE_TINY`, garantindo o alinhamento exato de 1 byte na memória.

### Decisão 4: Parsing Seguro de Configuração (`config.c`)
A função frágil `atoi` foi substituída por `strtol` na conversão da variável de ambiente `DB_PORT`. O módulo passou a validar o registrador `errno`, ponteiros de término de string e o intervalo válido de portas TCP (1 a 65535), adotando *fallback* seguro em caso de injeção de valores inválidos.

### Decisão 5: Conformidade DevSecOps e Remoção de Comandos Administrativos
A flag depreciada `MYSQL_OPT_RECONNECT` foi totalmente removida de `db_utils.c` para evitar a destruição silenciosa dos ponteiros de *Prepared Statements* em quedas de sessão. Removeu-se também a instrução `SET GLOBAL innodb_flush_log_at_trx_commit` do código C, delegando a responsabilidade de *flush* de disco à camada de infraestrutura (`mysqld.cnf`) e permitindo que o motor rode sob um usuário MySQL com privilégios restritos de CRUD.

## 4. Consequências

### Positivas:
* **Eficiência Extrema de CPU:** O tempo ativo de CPU caiu para apenas 0,38 segundos durante o processamento de 10.000 transações sequenciais (menos de 2% do tempo total de execução).
* **Estabilidade de RAM (Zero Memory Leak):** Consumo de memória estabilizado com crescimento nulo (0 KB de variação no *VmRSS/VmHWM*), comprovando a correta alocação e liberação na *stack*.
* **Escalabilidade em Leitura:** Capacidade de navegar por milhões de registros de forma contínua (*infinite scroll*) mantendo complexidade $O(\log N)$.
* **Segurança Operacional:** Adstrição ao Princípio do Privilégio Mínimo, prevenção contra manipulações indevidas de variáveis globais e imunidade nativa a *SQL Injections* via *Prepared Statements*.

### Negativas / Desafios:
* **Dependência da Camada de Infraestrutura:** A otimização máxima de I/O de disco (ex: ajuste de *flush* do `InnoDB`) passa a exigir configuração prévia no ambiente do servidor/container (`mysqld.cnf`), não sendo mais manipulada dinamicamente pelo binário C.
* **Gestão Manual de Reconexão:** Como a reconexão automática silenciosa foi desativada por segurança de ponteiros, a resiliência contra oscilações de rede noturnas deverá ser tratada de forma explícita (*Heartbeat / Auto-healing*) na futura camada do Servidor de Sockets IPC.