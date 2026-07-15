# ADR 0001: Estrutura de Conexão com Banco de Dados

* **Status:** Aceito
* **Data:** 2026-07-12
* **Autor:** Matheus Nascimento

---

## 1. Contexto e Declaração do Problema
Para garantir a portabilidade, segurança e flexibilidade da aplicação, precisamos estabelecer um mecanismo de conexão com o banco de dados MySQL que não exponha credenciais sensíveis e permita que o software rode em diferentes ambientes (desenvolvimento, homologação, produção) sem a necessidade de alteração ou recompilação do código-fonte escrito em C.

## 2. Alternativas Consideradas

* **Opção A: Conexão direta com dados fixos (*Hardcoded*) no código**
  * **Prós:** Abordagem menos verbosa, implementação imediata e menor tempo de desenvolvimento inicial.
  * **Contras:** Péssima prática de segurança (exposição de credenciais no repositório). Qualquer alteração de IP, porta, usuário ou senha exige a modificação do código-fonte e uma nova recompilação do binário.

* **Opção B: Conexão através de variáveis de ambiente**
  * **Prós:** Segue as boas práticas do manifesto *Twelve-Factor App*. Desacopla as credenciais do código, permitindo rodar a mesma build em qualquer máquina apenas alterando o ambiente. Permite o uso de valores padrão (fallback) caso alguma variável não seja definida.
  * **Contras:** Exige que o ambiente de execução (ou arquivo de configuração `.env`) esteja devidamente configurado e populado antes da inicialização do programa.

## 3. Decisões Tomadas

### Decisão 1: Utilização de variáveis de ambiente para configuração do banco
Optou-se pela **Opção B**. Os dados de conexão serão lidos em tempo de execução através da função `getenv()`. Caso variáveis opcionais (como porta ou host) não sejam encontradas no ambiente, a aplicação adotará valores padrão seguros (ex: host `127.0.0.1` e porta `3306`).

### Decisão 2: Estrutura de Dados Dinâmica (`db_config_t`)
Para gerenciar essas configurações na memória de forma eficiente, foi adotada uma estrutura baseada em ponteiros de leitura. Os ponteiros na struct possuem tamanho estático (8 bytes em arquiteturas de 64 bits), enquanto os dados de texto apontados por eles são alocados dinamicamente pelo sistema operacional no momento em que as variáveis de ambiente são lidas.

## 4. Consequências

### Positivas:
* **Segurança:** Credenciais sensíveis não são versionadas no repositório Git.
* **Portabilidade:** O sistema pode ser implantado em qualquer servidor ou container Docker sem alterações no binário compilado.
* **Resiliência:** Flexibilidade para definir fallbacks (valores padrão) caso o utilizador esqueça de configurar variáveis não críticas.
* **Eficiência de Memória:** A estrutura de dados utiliza ponteiros `const char *`, evitando alocações manuais desnecessárias (`malloc`/`free`) já que o ciclo de vida dessas strings é gerenciado pelo próprio ambiente do processo.

### Negativas / Desafios:
* **Dependência de Configuração:** A aplicação falhará em tempo de execução se as variáveis obrigatórias (como usuário e senha) não forem fornecidas.
* **Necessidade de Tratamento de Erros:** O código precisa validar ativamente se o retorno de `getenv()` é `NULL` para os campos obrigatórios e interromper a execução graciosamente com uma mensagem de erro clara ao usuário.