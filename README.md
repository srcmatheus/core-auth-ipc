# core-auth-ipc

## Visão Geral do Projeto

Sistema poliglota de autenticação e controle de acesso baseado em IPC (Inter-Process Communication), integrando um motor nativo em C, uma API em Java (Spring Boot) e o banco de dados MySQL. O projeto foi arquitetado sob os pilares de escalabilidade, eficiência e segurança defensiva.

O sistema será implantado e testado em um ambiente de infraestrutura própria (*Home Lab*) dedicado, operando sobre o sistema operacional Ubuntu Server. O hardware hospedeiro possui as seguintes especificações:
* **Processador:** Intel Core i3-2100  
* **Memória RAM:** 8GB DDR3  
* **Armazenamento:** SSD NVMe 128GB  

A aplicação rodará localmente nessa máquina, servindo como laboratório real para testes rigorosos de estresse, desempenho, segurança e comportamento de infraestrutura.

O objetivo principal de engenharia deste projeto é garantir robustez de software, sob a premissa de que **o hardware deve falhar antes do código**. A aplicação será levada ao limite dos componentes físicos do servidor para garantir que atenda aos princípios impostos no planejamento.

Todos os relatórios estarão disponíveis para consulta e validação geral de engenharia.

---