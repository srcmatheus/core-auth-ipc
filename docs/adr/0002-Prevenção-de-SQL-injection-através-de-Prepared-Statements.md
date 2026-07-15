# ADR 0002: Prevenção de SQL Injection através de Prepared Statements

* **Status:** Aceito
* **Data:** 2026-07-13
* **Autor:** Matheus Nascimento

---

## 1. Contexto e Declaração do Problema
Durante a persistência de dados enviados pelo usuário na base de dados MySQL, precisamos garantir a integridade do banco de dados e impedir ataques de segurança, especificamente a injeção de SQL (*SQL Injection*). O objetivo é definir o padrão técnico de montagem e execução de consultas que manipulam dados dinâmicos na aplicação escrita em C.

## 2. Alternativas Consideradas

* **Opção A: Conexão direta via consultas simples (ex: `mysql_query` / `mysql_real_query`)**
  * **Prós:** Desenvolvimento extremamente rápido, código enxuto e de fácil leitura imediata. Ideal para consultas 100% estáticas (onde não há parâmetros de usuários).
  * **Contras:** Altamente vulnerável a ataques de *SQL Injection* se houver concatenação manual de strings (utilizando `sprintf`, por exemplo). Exige etapas manuais exaustivas de sanitização de caracteres (como `mysql_real_escape_string`) para tentar mitigar os riscos.

* **Opção B: Utilização de Instruções Preparadas (*Prepared Statements*)**
  * **Prós:** Máxima segurança contra *SQL Injection*. A estrutura da query é enviada ao banco de dados separadamente dos parâmetros (dados), garantindo que dados maliciosos nunca sejam interpretados como comandos SQL pelo interpretador do banco. Além disso, traz otimização de performance pelo banco de dados ao reaproveitar o plano de execução da query em chamadas repetidas.
  * **Contras:** Implementação mais complexa e verbosa no ecossistema C (`mysql_stmt_init`, `mysql_stmt_prepare`, `mysql_stmt_bind_param`, etc.). Exige gerenciamento cuidadoso do ciclo de vida das estruturas da API cliente do MySQL.

## 3. Decisões Tomadas

### Decisão 1: Adoção obrigatória de Prepared Statements para dados dinâmicos
Optou-se pela **Opção B**. Fica estabelecido que qualquer operação de consulta, inserção, atualização ou exclusão que utilize dados originados de variáveis, parâmetros ou entradas de usuários deve, obrigatoriamente, ser executada utilizando as APIs de *Prepared Statements* da biblioteca `libmysqlclient` (estruturas `MYSQL_STMT` e `MYSQL_BIND`). 

Esta decisão prioriza a segurança da aplicação como requisito não funcional inegociável, distribuindo claramente a responsabilidade de formatação e tipagem de dados nas camadas corretas da biblioteca.

## 4. Consequências

### Positivas:
* **Segurança Robusta:** Eliminação completa da vulnerabilidade de *SQL Injection* para consultas parametrizadas.
* **Tipagem Forte:** O uso de estruturas `MYSQL_BIND` força a definição explícita do tipo de dados (ex: `MYSQL_TYPE_STRING`, `MYSQL_TYPE_LONG`), reduzindo erros de coerção em tempo de execução no banco.
* **Ganho de Performance:** Otimização no servidor de banco de dados, que compila e otimiza o plano de execução da query apenas uma vez, executando-a repetidamente apenas trocando os parâmetros.

### Negativas / Desafios:
* **Complexidade do Código (Verbosidade):** O fluxo de trabalho em C exige múltiplos passos (inicializar o statement, preparar a string, configurar o bind de entrada, associar os dados e executar), aumentando o volume de código necessário para uma única inserção.
* **Gerenciamento de Recursos:** Exige atenção redobrada na liberação de recursos com `mysql_stmt_close()` para evitar vazamentos de memória (*memory leaks*) e vazamentos de conexões no servidor.