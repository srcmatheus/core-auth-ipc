# Decisões de Arquitetura Geral do Projeto — CoreAuth IPC

Este documento formaliza as decisões de arquitetura e engenharia de software para o **Projeto DSO: CoreAuth IPC**, um sistema robusto, seguro e de alta performance desenvolvido para ambientes Linux (Ubuntu Server). 

O lema fundamental do projeto é: **"O hardware falha antes do código."**

---

## 1. Visão Geral e Fluxo do Sistema

O sistema é estruturado em quatro camadas distintas, distribuindo as responsabilidades de forma clara e isolada:

```
[ Front-end (Web) ] <---(HTTP/REST)---> [ Back-end (Java / Spring Boot) ] <---(IPC / Unix Sockets)---> [ Motor de Persistência (C) ] <---(API Nativa)---> [ Banco de Dados (MySQL) ]
```

*   **Front-end**: Interface do usuário responsável pela experiência visual (UX) e validações básicas de formulário.
*   **Back-end (Java / Spring Boot)**: Gateway de API e orquestrador de segurança. Responsável por autenticação, autorização de acesso e validação estrita de dados antes do envio ao motor de persistência.
*   **Motor de Persistência (C)**: Motor nativo de alta performance responsável pelo acesso direto ao banco de dados e execução de regras lógicas de escrita estruturada.
*   **Banco de Dados (MySQL)**: Camada de armazenamento definitivo.

---

## 2. Design de Arquitetura de Macro (IPC e Conexões)

### 2.1. Conexão entre o Motor em C e o Banco de Dados (MySQL)
Para a comunicação do motor em C com o banco MySQL, optou-se pela **API Nativa do MySQL** em detrimento do padrão ODBC.

*   **Decisão**: **API Nativa do MySQL (MySQL C API)**.
*   **Justificativa**: Performance máxima absoluta e controle total sobre o ciclo de vida dos ponteiros e conexões. Dado que este é um projeto focado em alta eficiência e que o banco de dados está consolidado como MySQL, os benefícios de portabilidade do ODBC não justificam a introdução de uma camada intermediária de tradução de chamadas.

### 2.2. Comunicação entre Processos (IPC): Java <-> C
A conexão entre a JVM (Java) e o binário executável (C) é a espinha dorsal do projeto. Para habilitar testes comparativos de arquitetura (*A/B Testing* / *Benchmarking*), adotamos duas abordagens:

1.  **Sockets Locais (Unix Domain Sockets)**: É o **ponto de partida e arquitetura padrão**. O motor em C abre uma porta lógica representada por um arquivo no sistema operacional (ex: `/var/run/coreauth.sock`) e a JVM se conecta a ele. Oferece comunicação assíncrona, excelente escalabilidade e isolamento de processos (se o C falhar, a JVM continua ativa).
2.  **JNI (Java Native Interface) / JNA (Java Native Access)**: Utilizado para fins de **benchmarking comparativo**. O código C é compilado como uma biblioteca dinâmica (`.so` no Linux) e carregado diretamente para a memória da JVM. Elimina qualquer latência de IPC, mas viola a meta de robustez extrema: um *Segmentation Fault* no C derrubará instantaneamente toda a máquina virtual Java.

### 2.3. Conexão entre o Front-end e o Back-end (Spring Boot)
Para a comunicação entre a interface do usuário e o back-end Java, analisou-se o uso de gRPC, WebSockets e APIs RESTful.

*   **Decisão**: **API RESTful (JSON sobre HTTP/1.1)**.
*   **Justificativa**: Garante excelente suporte a ferramentas de testes de carga e estresse (como Apache JMeter e Gatling), integração nativa com o ecossistema de segurança do **Spring Security** e total compatibilidade com os navegadores web atuais (eliminando a necessidade de proxies como Envoy, exigidos pelo gRPC-Web). WebSockets e conexões persistentes foram descartados por adicionarem complexidade de gerenciamento de estado desnecessária para fluxos transacionais simples (cadastro/login).

---

## 3. Arquitetura de Dados e Protocolos de Comunicação

### 3.1. Protocolo de Serialização (Java <-> C)
A troca de mensagens através do Unix Domain Socket exige um protocolo de tradução universal e eficiente.

*   **Decisão**: **Binário Puro utilizando Structs em C**.
*   **Justificativa**: Evita o overhead de processamento que bibliotecas de parser JSON causariam no motor em C, mantendo o executável enxuto e performático. Os dados estruturados são escritos e lidos diretamente da memória.
*   **Especificação Técnica de Alinhamento e Endianness**:
    1.  **Ordem dos Bytes (Endianness)**: A JVM adota por padrão o formato *Big-Endian*, enquanto a CPU x86_64 e a linguagem C utilizam *Little-Endian*. Configura-se o buffer Java explicitamente para ordenar os bytes em formato nativo (`ByteOrder.LITTLE_ENDIAN`).
    2.  **Alinhamento de Dados (Padding)**: Para impedir que o compilador GCC insira bytes de preenchimento invisíveis na memória, as structs no código C serão anotadas com diretivas de compilação rígidas como `__attribute__((packed))`.

### 3.2. Persistência e Integridade Transacional (ACID)
Para assegurar a integridade dos dados contra quedas repentinas de energia ou falhas críticas de hardware, combinamos soluções lógicas e de hardware gerenciadas programaticamente.

* **Mecanismo de Armazenamento**: Uso obrigatório do motor **InnoDB** do MySQL, garantindo propriedades ACID através do mecanismo de WAL (*Write-Ahead Logging*), onde as transações são primeiramente persistidas no *Redo Log* sequencial para recuperação automática de falhas (*Crash Recovery*).
* **Política de Sincronização Física (innodb_flush_log_at_trx_commit)**:
    * **Decisão**: **Modo Flexível (valor = 2)** configurado em tempo de execução.
    * **Justificativa**: No modo estrito (valor = 1), a gravação física imediata no SSD NVMe após cada cadastro estrangula a taxa de IOPS e reduz severamente a vida útil do hardware. Com o valor `2`, os dados são processados instantaneamente na memória RAM do Linux e descarregados em lote no SSD uma vez por segundo pelo Kernel, equilibrando máxima performance com desgaste reduzido do hardware, aceitando o risco tolerável de perda do último segundo de transações em caso de blecaute completo do servidor.
    * **Implementação Dinâmica**: Para garantir a independência de infraestrutura e o comportamento de *Setup Zero*, o motor C injeta de forma ativa o comando `SET GLOBAL innodb_flush_log_at_trx_commit = 2;` via query programática imediatamente após estabelecer conexão estável e selecionar o schema alvo no driver `libmysqlclient`.

---

## 4. Estratégias de Segurança e Defesa

A segurança é aplicada de forma redundante em múltiplas camadas, seguindo o princípio de que o front-end nunca é confiável.

```
[ Front-end ] 
      │  (CORS)
      ▼
[ API Java (Spring Security) ] - (JWT Validation & Role-Based Access)
      │  (Unix Socket + Handshake Token)
      ▼
[ Motor em C ] - (Strict Struct Validation & Kernel ACLs)
```

### 4.1. Segurança no Canal de Comunicação IPC (Java <-> C)
*   **Decisão**: **Handshake com Token Estático (Nível de Aplicação)** associado a **Controle de Acesso Discricionário (DAC - Permissões do Linux)**.
*   **Funcionamento**: 
    1.  O motor C cria o socket e o kernel do Linux restringe o acesso ao arquivo (`chmod 660`) para que somente os usuários específicos das aplicações Java e C o acessem.
    2.  Ao estabelecer a conexão via socket, a API Java deve enviar imediatamente um token secreto estático. Se o token recebido pelo motor C for inválido ou ausente, a conexão é encerrada imediatamente.

### 4.2. Segurança e Controle de Acesso no Back-end (Java)
A API Java atua como o principal escudo contra requisições malformadas ou não autorizadas através das seguintes implementações:

*   **Autenticação Stateless via JWT (JSON Web Tokens)**: Tokens criptografados e assinados digitalmente. Qualquer alteração indevida de caracteres invalida a assinatura digital instantaneamente na memória RAM.
*   **Controle de Origem (CORS)**: Proteção contra requisições silenciosas originadas por domínios maliciosos externos.
*   **Autorização Baseada em Regras (RBAC)**: O sistema diferencia usuários **Comuns** (ações de leitura/cadastro básico) de **Administradores** (leitura, escrita, edição, exclusão e criação de novos administradores). O nível de privilégio (`role`) é gravado no banco como `TINYINT` (0 para Comum, 1 para Admin) e embutido no payload do token JWT. Métodos críticos no Spring Boot são blindados com anotações de segurança (ex: `@PreAuthorize("hasRole('ADMIN')")`), impedindo que requisições não autorizadas enviem dados ao socket.

### 4.3. Validação Redundante no Motor C (Robustez Defensiva)
Para garantir que o banco de dados nunca seja violado caso a camada Java seja comprometida, o motor em C implementará regras lógicas de validação estrita:

*   A struct do socket conterá um byte sinalizando a operação (`EXCLUIR`, `CADASTRAR`) e outro indicando o nível de permissão do usuário.
*   O motor C avalia os bytes logicamente: se a operação for `EXCLUIR` e a permissão indicada for `COMUM`, o processo aborta a operação imediatamente, registra a tentativa de invasão em arquivos de log dedicados e fecha a conexão do socket para proteger o banco de dados.