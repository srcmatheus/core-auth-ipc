#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "../database/db.h"
#include "../database/config.h"
#include "../database/db_utils.h"
#include "../protocol.h"
#include "stress_test.h"

typedef enum {
    OP_INSERT = 1,
    OP_FIND = 2,
    OP_EDIT = 3,
    OP_DELETE = 4,
    OP_TEST = 5,
    OP_EXIT = 6
} MenuOption;

int main() {
    db_config_t config = {0};
    user_protocol_t user = {0};
    user_data_t user_data_array[50] = {0};
    
    char input_buffer[256] = {0};
    int option = 0;
    int running = 1;

    config_init(&config);
    db_init(&config);

    printf("\n=== CoreAuth IPC - Modo de Teste Local ===\n");

    while (running) {
        printf("\n------------------------------------------\n");
        printf("1 - Inserir | 2 - Procurar/Listar | 3 - Editar | 4 - Excluir | 5 - Teste | 6 - Sair\n");
        printf("\nDigite uma opção: ");
        
        if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
            sscanf(input_buffer, "%d", &option);
        }

        switch (option) {
            case OP_INSERT: {
                memset(&user, 0, sizeof(user_protocol_t));
                printf("\n--- Cadastrar Usuário ---\n");
                
                printf("Nome: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                input_buffer[strcspn(input_buffer, "\n")] = 0;
                strncpy(user.full_name, input_buffer, sizeof(user.full_name) - 1);

                printf("Email: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                input_buffer[strcspn(input_buffer, "\n")] = 0;
                strncpy(user.email, input_buffer, sizeof(user.email) - 1);

                printf("Nível de Acesso (0 = Comum, 1 = Admin): ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                int level = 0;
                sscanf(input_buffer, "%d", &level);
                user.target_level = (uint8_t)level;

                if (db_insert_user(&user) == DB_SUCCESS) {
                    printf("\n[+] Usuário cadastrado com sucesso!\n");
                } else {
                    printf("\n[-] Falha ao cadastrar usuário.\n");
                }
                break;
            }

            case OP_FIND: {
                char search_name[100] = {0};
                char last_name[100] = {0};
                int last_id = 0;
                int out_count = 0;
                int searching = 1;

                printf("\n--- Procurar/Listar Usuários ---\n");
                printf("Digite o nome (ou deixe vazio para listar todos): ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                input_buffer[strcspn(input_buffer, "\n")] = 0;
                strncpy(search_name, input_buffer, sizeof(search_name) - 1);

                while (searching) {
                    memset(user_data_array, 0, sizeof(user_data_array));
                    
                    db_status_t status = db_find_user(user_data_array, &out_count, search_name, last_name[0] != '\0' ? last_name : NULL, last_id);

                    if (status == DB_WARNING || out_count == 0) {
                        printf("\n[-] Fim da lista ou nenhum usuário encontrado.\n");
                        searching = 0;
                    } else if (status == DB_CRITICAL_ERROR) {
                        printf("\n[-] Erro crítico ao buscar usuário(s).\n");
                        searching = 0;
                    } else {
                        printf("\n[Resultados da Busca]:\n");
                        for (int i = 0; i < out_count; i++) {
                            printf("ID: %d | %s | %s | Nível: %s\n", 
                                user_data_array[i].id, 
                                user_data_array[i].full_name, 
                                user_data_array[i].email,
                                user_data_array[i].access_level == 1 ? "Admin" : "Comum");
                        }

                        if (out_count == 50) {
                            last_id = user_data_array[49].id;
                            strncpy(last_name, user_data_array[49].full_name, sizeof(last_name) - 1);

                            printf("\n[P] Próxima Página | [Qualquer tecla] Sair: ");
                            fgets(input_buffer, sizeof(input_buffer), stdin);
                            if (input_buffer[0] == 'P' || input_buffer[0] == 'p') {
                                continue;
                            } else {
                                searching = 0;
                            }
                        } else {
                            printf("\n[+] Total de resultados lidos com sucesso.\n");
                            searching = 0;
                        }
                    }
                }
                break;
            }

            case OP_EDIT: {
                int target_id = 0;
                char new_name[100] = {0};
                char new_email[100] = {0};
                int new_access = 0;

                printf("\n--- Editar Usuário ---\n");
                
                printf("ID do usuário a editar: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                if(sscanf(input_buffer, "%d", &target_id) != 1) target_id = 0;

                printf("Novo Nome: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                input_buffer[strcspn(input_buffer, "\n")] = 0;
                strncpy(new_name, input_buffer, sizeof(new_name) - 1);

                printf("Novo Email: ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                input_buffer[strcspn(input_buffer, "\n")] = 0;
                strncpy(new_email, input_buffer, sizeof(new_email) - 1);

                printf("Novo Nível de Acesso (0 = Comum, 1 = Admin): ");
                fgets(input_buffer, sizeof(input_buffer), stdin);
                if(sscanf(input_buffer, "%d", &new_access) != 1) new_access = 0;

                db_status_t status = db_edit_user(target_id, new_name, new_email, (uint8_t)new_access);

                if(status == DB_WARNING) {
                    printf("\n[-] Falha: Dados inválidos ou usuário não encontrado.\n");
                }else if (status == DB_CRITICAL_ERROR) {
                    printf("\n[-] Erro crítico no servidor de banco de dados.\n");
                }else {
                    printf("\n[+] Cadastro alterado com sucesso!\n");
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
                    printf("\n[-] Falha: Usuário não encontrado.\n");
                }else if (status == DB_CRITICAL_ERROR) {
                    printf("\n[-] Erro crítico ao tentar excluir.\n");
                }else {
                    printf("\n[+] Usuário excluído com sucesso!\n");
                }
                break;
            }

            case OP_TEST:
                run_tps_stress_test();
                break;

            case OP_EXIT: {
                printf("\nEncerrando o sistema...\n");
                running = 0;
                break;
            }

            default: {
                printf("\n[-] Opção inválida. Tente novamente.\n");
                break;
            }
        }
    }

    db_close();
    return 0;
}