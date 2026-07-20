#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "database/db.h"
#include "database/config.h"
#include "database/db_utils.h"
#include "protocol.h"

typedef enum {
    OP_INSERT = 1,
    OP_FIND   = 2,
    OP_EDIT   = 3,
    OP_DELETE = 4,
    OP_LIST   = 5,
    OP_EXIT   = 6
} MenuOption;

int main() {
    db_config_t config = {0};
    user_protocol_t user = {0};
    user_data_t user_data = {0};
    
    char input_buffer[256] = {0};
    int option = 0;
    int running = 1;

    config_init(&config);
    db_init(&config);

    printf("\n=== CoreAuth IPC - Modo de Teste Local ===\n");

    while (running) {
        printf("\n------------------------------------------\n");
        printf("1 - Inserir | 2 - Exibir | 3 - Editar | 4 - Excluir | 5 - Listar | 6 - Sair\n");
        printf("Digite uma opção: ");
        
        if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
            sscanf(input_buffer, "%d", &option);
        }

        switch (option) {
            case OP_INSERT: {
                printf("\n--- Novo Cadastro ---\n");
                
                printf("Nome completo: ");
                fgets(user.full_name, sizeof(user.full_name), stdin);
                user.full_name[strcspn(user.full_name, "\n")] = '\0';

                printf("E-mail: ");
                fgets(user.email, sizeof(user.email), stdin);
                user.email[strcspn(user.email, "\n")] = '\0';

                db_insert_user(&user);
                printf("[+] Cadastro realizado com sucesso!\n");
                break;
            }

            case OP_FIND: {
                char search_name[100] = {0};
                
                printf("\n--- Buscar Usuário ---\n");
                printf("Digite o nome exato: ");
                fgets(search_name, sizeof(search_name), stdin);
                search_name[strcspn(search_name, "\n")] = '\0';

                db_status_t status = db_find_user(&user_data, search_name);

                if(status == DB_WARNING) {
                    printf("[-] Usuário não encontrado.\n");
                }else if (status == DB_CRITICAL_ERROR) {
                    printf("[-] Erro crítico ao acessar o banco de dados.\n");
                }else {
                    printf("\n[+] Resultado:\n");
                    printf("  ID     : %d\n", user_data.id);
                    printf("  Nome   : %s\n", user_data.full_name);
                    printf("  E-mail : %s\n", user_data.email);
                }
                break;
            }

            case OP_EDIT: {
                int target_id = 0;
                char new_name[100] = {0};
                char new_email[100] = {0};

                printf("\n--- Editar Usuário ---\n");
                
                printf("ID do usuário: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                if(sscanf(input_buffer, "%d", &target_id) != 1) target_id = 0;

                printf("Novo nome: ");
                fgets(new_name, sizeof(new_name), stdin);
                new_name[strcspn(new_name, "\n")] = '\0';

                printf("Novo e-mail: ");
                fgets(new_email, sizeof(new_email), stdin);
                new_email[strcspn(new_email, "\n")] = '\0';

                db_status_t status = db_edit_user(target_id, new_name, new_email);

                if(status == DB_WARNING) {
                    printf("[-] Falha: Usuário não encontrado ou nenhuma alteração feita.\n");
                }else if (status == DB_CRITICAL_ERROR) {
                    printf("[-] Erro crítico no servidor de banco de dados.\n");
                }else {
                    printf("[+] Cadastro alterado com sucesso!\n");
                }
                break;
            }

            case OP_DELETE: {
                int target_id = 0;

                printf("\n--- Excluir Usuário ---\n");
                
                printf("ID do usuário a excluir: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                if(sscanf(input_buffer, "%d", &target_id) != 1) target_id = 0;

                db_status_t status = db_delete_user(target_id);

                if(status == DB_WARNING) {
                    printf("[-] Falha: Usuário não encontrado.\n");
                }else if (status == DB_CRITICAL_ERROR) {
                    printf("[-] Erro crítico ao tentar excluir.\n");
                }else {
                    printf("[+] Usuário excluído com sucesso!\n");
                }
                break;
            }

            case OP_LIST:
                db_list_users();
                break;

            case OP_EXIT: {
                printf("\nEncerrando o sistema...\n");
                running = 0;
                break;
            }

            default: {
                printf("[-] Opção inválida. Tente novamente.\n");
                break;
            }
        }
    }

    db_close();
    return 0;
}