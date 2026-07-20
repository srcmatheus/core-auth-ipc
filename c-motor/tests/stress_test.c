#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../protocol.h"
#include "../database/db.h"
#include "../database/db_utils.h"
#define test_REPORT_FILENAME "tests/reports/stress_test_report.md"

#define TOTAL_ITERATIONS 1000

void get_system_hardware(char *cpu_model, size_t cpu_sz, char *ram_total, size_t ram_sz) {
    FILE *fp;
    char line[256];

    strncpy(cpu_model, "Desconhecido", cpu_sz);
    strncpy(ram_total, "Desconhecido", ram_sz);

    fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *info = strchr(line, ':');
                if (info) {
                    info += 2; 
                    info[strcspn(info, "\n")] = 0;
                    strncpy(cpu_model, info, cpu_sz - 1);
                    break;
                }
            }
        }
        fclose(fp);
    }

    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal", 8) == 0) {
                char *info = strchr(line, ':');
                if (info) {
                    info += 2;
                    
                    long long kb = atoll(info); 
                    double gb = (double)kb / (1024.0 * 1024.0);
                    
                    snprintf(ram_total, ram_sz, "%.2f GB", gb);
                    break;
                }
            }
        }
        fclose(fp);
    }
}

void run_tps_stress_test(void){

    struct timespec start, end;
    int success_count = 0;
    int error_count = 0;
    user_protocol_t user;

    printf("Iniciando teste de stress...\n");
    printf("Alvo: %d inserções sequênciais.\n", TOTAL_ITERATIONS);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i = 0; i < TOTAL_ITERATIONS; i++){
        memset(&user, 0, sizeof(user));

        snprintf(user.full_name, sizeof(user.full_name), "Stress Test User %d", i);
        snprintf(user.email, sizeof(user.email), "stress_user_%d@domain.com", i);

        if(db_insert_user(&user) == DB_SUCCESS){
            success_count++;
        }else{
            error_count++;
        }

        if ((i + 1) % 1000 == 0) {
            printf("   -> Processados %d registos...\n", i + 1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double tps = (total_time > 0) ? (success_count / total_time) : 0;

    printf("\n============================================================\n");
    printf("                  RELATORIO DE PERFORMANCE                  \n");
    printf("============================================================\n");
    printf("Tempo total de execucao : %.2f segundos\n", total_time);
    printf("Insercoes com sucesso   : %d\n", success_count);
    printf("Falhas de insercao      : %d\n", error_count);
    printf("Desempenho do Motor     : %.2f TPS (Transacoes/Segundo)\n", tps);
    printf("============================================================\n");

    char filename[64];
    int file_index = 1;
    
    while (1) {
        snprintf(filename, sizeof(filename), "tests/reports/stress_test_report_%d.md", file_index);
        if (access(filename, F_OK) == -1) {
            break; 
        }
        file_index++;
    }

    char cpu_info[128];
    char ram_info[64];
    get_system_hardware(cpu_info, sizeof(cpu_info), ram_info, sizeof(ram_info));

    FILE *test_report = fopen(filename, "w");
    if (test_report == NULL) {
        fprintf(stderr, "Erro ao criar o arquivo de relatório: %s\n", filename);
        return;
    }

    fprintf(test_report, "# Relatório de Performance - Motor C (Execução #%d)\n\n", file_index);
    fprintf(test_report, "### Especificações do Ambiente de Teste\n");
    fprintf(test_report, "* **Processador (CPU):** %s\n", cpu_info);
    fprintf(test_report, "* **Memória RAM Total:** %s\n", ram_info);

    fprintf(test_report, "### Métricas Obtidas\n\n");
    fprintf(test_report, "| Métrica | Valor Obtido |\n");
    fprintf(test_report, "| :--- | :--- |\n");
    fprintf(test_report, "| **Iterações Planejadas** | %d |\n", TOTAL_ITERATIONS);
    fprintf(test_report, "| **Tempo Total** | %.2f segundos |\n", total_time);
    fprintf(test_report, "| **Inserções com Sucesso** | %d |\n", success_count);
    fprintf(test_report, "| **Falhas Detectadas** | %d |\n", error_count);
    fprintf(test_report, "| **Vazão (Throughput)** | **%.2f TPS** |\n\n", tps);
    
    if (error_count == 0) {
        fprintf(test_report, "> **Status:** Test completed with 100%% operational success.\n");
    } else {
        fprintf(test_report, "> **Warning:** Errors occurred during processing.\n");
    }

    fclose(test_report);
    
    printf("\nRelatório salvo com sucesso em: %s\n", filename);
}