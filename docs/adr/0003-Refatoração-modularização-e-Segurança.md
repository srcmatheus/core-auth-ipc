# ADR 0002: Refatoração, Modularização e Segurança de Memória no Módulo de Banco de Dados

* **Status:** Aceito
* **Data:** 2026-07-20
* **Autor:** Matheus Nascimento

---

## 1. Contexto e Declaração do Problema
A antiga estrutura de gerenciamento do banco de dados possuía redundâncias de código e vulnerabilidades que poderiam causar vazamentos de memória (*memory leaks*) e falhas de segmentação (*segmentation faults*). Para garantir a estabilidade e a manutenibilidade do software escrito em C, precisamos centralizar o ciclo de vida dos *Prepared Statements*, padronizar o retorno das funções do CRUD e mitigar riscos de *buffer overflow* no tratamento de strings vindas do MySQL.

## 2. Alternativas Consideradas

* **Opção A: Manter a lógica diluída nas funções do CRUD com retornos inteiros genéricos**
  * **Prós:** Nenhuma necessidade de reescrever as funções existentes ou alterar a assinatura das interfaces atuais.
  * **Contras:** Manutenção complexa devido à repetição de código de infraestrutura. Alto risco de falhas de segurança por falta de um padrão estrito na validação do caractere nulo (`\0`) nas strings e dificuldade de rastrear erros com retornos numéricos arbitrários (números mágicos).

* **Opção B: Criar um módulo isolado com retornos tipados e validação compulsória de memória**
  * **Prós:** Centraliza o tratamento de erros e a inicialização de conexões. Introduz um tipo enumerado para padronizar os retornos, aumentando a legibilidade do código. Garante a segurança de memória (*string safety*) ao forçar a terminação nula baseada no ponteiro de tamanho (`len_out`) fornecido pela API do MySQL.
  * **Contras:** Exige uma refatoração completa nas assinaturas das funções do CRUD e a adequação da interface de testes (`main_test.c`).

## 3. Decisões Tomadas

### Decisão 1: Criação do submódulo isolado `db_utils`
Optou-se pela **Opção B**. Toda a lógica de inicialização de conexão e o ciclo de vida (preparação, vinculação e execução) de *Prepared Statements* foram movidos para o arquivo `db_utils.c`, eliminando o código duplicado.

### Decisão 2: Introdução do tipo enumerado `db_status_t`
Foi adicionada a estrutura enumerada `db_status_t` (`DB_SUCCESS`, `DB_WARNING`, `DB_CRITICAL_ERROR`) para abstrair o retorno de todas as funções do banco de dados, substituindo o uso de números mágicos e tornando o fluxo de controle previsível.

### Decisão 3: Validação rigorosa de strings via `len_out`
Para mitigar riscos de *buffer overflow*, foi implementada uma validação compulsória para o retorno de strings do banco de dados. O código agora insere ativamente o caractere nulo `\0` na posição exata indicada pelo ponteiro retornado pela API do MySQL.

### Decisão 4: Adequação e isolamento em `main_test.c`
A interface de testes foi atualizada para se adequar à nova estrutura tipada. As chamadas de funções foram isoladas em variáveis locais antes das estruturas condicionais, evitando disparos duplicados de consultas ao banco de dados (*side effects*).

## 4. Consequências

### Positivas:
* **Legibilidade e Manutenibilidade:** O código ficou limpo, modular e muito mais fácil de entender com a eliminação de redundâncias e números mágicos.
* **Segurança de Memória (Memory Safety):** Proteção robusta contra falhas de segmentação e estouro de buffer no manuseio de dados textuais.
* **Confiabilidade:** O isolamento das chamadas na `main_test.c` garante que nenhuma operação no banco de dados seja executada duas vezes por falha de lógica no código de teste.

### Negativas / Desafios:
* **Esforço de Refatoração:** Exigiu a reescrita de todas as funções do CRUD (`db_insert_user`, `db_find_user`, etc.) para adaptá-las à nova assinatura baseada em `db_status_t`.
* **Rigor no Desenvolvimento:** Qualquer nova funcionalidade ou consulta adicionada ao sistema deverá obrigatoriamente seguir os padrões de liberação de memória e tratamento de strings instituídos por este módulo central.