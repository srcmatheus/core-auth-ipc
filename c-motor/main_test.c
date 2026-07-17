#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "database/db.h"
#include "database/config.h"
#include "protocol.h"

int main(){

    db_config_t config = {0};
    user_protocol_t user = {0};
    user_data_t user_data = {0};

    int opcao = 0;
    char nome[100] = {0};

    config_init(&config);
    db_init(&config);

    printf("\n=== CoreAuth IPC - Modo de Teste Local ===\n");
    printf("1 - Inserir usuário | 2 - Exibir | 5 - Sair\n");

    while(1){
        printf("\nDigite uma opção: ");
        scanf("%d", &opcao);
        printf("\n");

        switch(opcao){
            case 1:
                printf("Digite um nome: ");
                scanf(" %[^\n]", user.full_name);

                printf("Digite o email: ");
                scanf(" %[^\n]", user.email);

                db_insert_user(&user);

                printf("\nCadastro realizado com sucesso!\n");

                printf("\n==========================================\n");
                while(getchar() != '\n');
                break;
            case 2:
                while(getchar() != '\n');
                printf("Digite o nome do usuário que deseja encontrar: ");
                fgets(nome, sizeof(nome), stdin);
                nome[strcspn(nome, "\n")] = '\0';

                if(db_find_user(&user_data, nome) == 1){
                    printf("\nUsuário não encontrado...");
                    printf("\n==========================================\n");
                    break;
                }

                printf("\nResultado ================================\n");
                printf("\nID: %d\n", user_data.id);
                printf("Nome completo: %s\n", user_data.full_name);
                printf("E-mail: %s\n", user_data.email);
                printf("\n==========================================\n");
                break;
            case 5:
                printf("Encerrando programa...\n");
                db_close();
                exit(0);
            default:
                printf("Opção inválida. ");
                while(getchar() != '\n');
                continue;
        }
    }
    return 0;
}