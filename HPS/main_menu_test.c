/*
 * =========================================================================
 * main_menu_test.c:
 * =========================================================================
 *
 * Este programa fornece um menu interativo para testar todas as 
 * funções da 'api.s'.
 *
 * Ele gerencia o estado para garantir que as funções sejam chamadas
 * na ordem correta (ex: não se pode enviar imagem antes de inicializar).
 *
 */

#include "api.h" // O seu ficheiro .h
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> // Para usleep()

// Definimos um timeout de C (número de loops de 100ms)
#define C_TIMEOUT_LOOPS 50 // (50 * 100ms = 5 segundos)

/* ===================================================================
 * FUNÇÕES DE CARREGAMENTO DE BMP 
 * =================================================================== */

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits;
    uint32_t compression;
    uint32_t imagesize;
    int32_t  xresolution;
    int32_t  yresolution;
    uint32_t ncolours;
    uint32_t importantcolours;
} BMPInfoHeader;
#pragma pack(pop)

uint8_t rgb_to_gray(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
}

int load_bmp(const char *filename, uint8_t *image_data) {
    FILE *file;
    BMPHeader header;
    BMPInfoHeader infoHeader;
    
    file = fopen(filename, "rb");
    if (!file) {
        printf(" Erro ao abrir '%s'\n", filename);
        return -1;
    }
    
    fread(&header, sizeof(BMPHeader), 1, file);
    if (header.type != 0x4D42) { // 'BM'
        printf(" Arquivo nao e BMP valido (assinatura 0x%X)\n", header.type);
        fclose(file);
        return -1;
    }
    
    fread(&infoHeader, sizeof(BMPInfoHeader), 1, file);
    
    if (infoHeader.width != IMG_WIDTH || abs(infoHeader.height) != IMG_HEIGHT) {
        printf(" Dimensao incorreta: %dx%d (esperado %dx%d)\n", 
               infoHeader.width, abs(infoHeader.height), IMG_WIDTH, IMG_HEIGHT);
        fclose(file);
        return -1;
    }
    
    fseek(file, header.offset, SEEK_SET);
    
    int bytes_per_pixel = infoHeader.bits / 8;
    int row_size = ((infoHeader.width * infoHeader.bits + 31) / 32) * 4;
    
    uint8_t *row_data = (uint8_t*)malloc(row_size);
    if (!row_data) {
        printf("\nErro ao alocar memoria para a linha do BMP\n");
        fclose(file);
        return -1;
    }
    
    printf("Carregando");
    for (int y = 0; y < IMG_HEIGHT; y++) {
        fread(row_data, 1, row_size, file);
        
        for (int x = 0; x < IMG_WIDTH; x++) {
            uint8_t gray;
            
            if (infoHeader.bits == 32) { // BGRA
                uint8_t b = row_data[x * 4 + 0];
                uint8_t g = row_data[x * 4 + 1];
                uint8_t r = row_data[x * 4 + 2];
                gray = rgb_to_gray(r, g, b);
            } else if (infoHeader.bits == 24) { // BGR
                uint8_t b = row_data[x * 3 + 0];
                uint8_t g = row_data[x * 3 + 1];
                uint8_t r = row_data[x * 3 + 2];
                gray = rgb_to_gray(r, g, b);
            } else if (infoHeader.bits == 8) {
                gray = row_data[x];
            } else {
                printf("\nFormato %d bits nao suportado\n", infoHeader.bits);
                free(row_data);
                fclose(file);
                return -1;
            }
            
            int addr = (IMG_HEIGHT - 1 - y) * IMG_WIDTH + x;
            image_data[addr] = gray;
        }
        
        if (y % 60 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    printf(" OK!\n");
    free(row_data);
    fclose(file);
    return 0;
}


/* ===================================================================
 * FUNÇÕES AUXILIARES DE TESTE
 * =================================================================== */

/**
 * @brief Gera um padrão de gradiente (no buffer C).
 */
void gerar_padrao_teste(uint8_t *image_data) {
    printf("   [C] Gerando padrao de teste (gradiente 320x240)...\n");
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            uint8_t pixel_value = (uint8_t)((x * 255) / (IMG_WIDTH - 1));
            image_data[y * IMG_WIDTH + x] = pixel_value;
        }
    }
}

/**
 * @brief Envia o buffer C para a VRAM do FPGA.
 * (Testa implicitamente ASM_Store e ASM_Refresh)
 */
int enviar_imagem_para_fpga(uint8_t *image_data) {
    int total_pixels = IMG_WIDTH * IMG_HEIGHT;
    int errors = 0;

    printf("   [C] Enviando %d pixels para o FPGA (testando ASM_Store)...\n", total_pixels);
    
    for (int i = 0; i < total_pixels; i++) {
        int status = ASM_Store(i, image_data[i]);
        if (status != 0) {
            printf("\n   [C] ERRO: ASM_Store falhou no pixel %d (codigo %d)\n", i, status);
            errors++;
            if (errors > 10) {
                printf("   [C] Muitos erros, abortando envio.\n");
                return -1;
            }
        }
    }
    
    printf("   [C] Envio de pixels OK.\n");
    
    printf("   [C] Testando ASM_Refresh()...\n");
    ASM_Refresh();
    usleep(100000); // 100ms de espera

    if (errors > 0) {
        printf("   [C] ERRO: %d falhas de escrita de pixel.\n", errors);
        return -1;
    }
    return 0;
}

/**
 * @brief Função genérica para executar e testar um algoritmo assíncrono.
 */
int executar_algoritmo(const char *nome_algoritmo, void (*funcao_algoritmo)(void)) {
    printf("   [C] Executando '%s' (assincrono)...\n", nome_algoritmo);
    
    funcao_algoritmo(); 
    
    printf("   [C] Hardware iniciado. Aguardando FLAG_DONE (polling)...\n");

    int timeout_c = 0;
    while (ASM_Get_Flag_Done() == 0) {
        usleep(100000); // Espera 100ms
        timeout_c++;
        
        if (timeout_c > C_TIMEOUT_LOOPS) {
            printf("\n   [C] ERRO FATAL: TIMEOUT DO ALGORITMO '%s'!\n", nome_algoritmo);
            return -1; // Falha
        }
    }
    
    printf("   [C] FLAG_DONE recebida para '%s'!\n", nome_algoritmo);

    if (ASM_Get_Flag_Error() != 0) {
        printf("   [C] ATENCAO: O FPGA reportou um ERRO (Flag_Error) durante '%s'!\n", nome_algoritmo);
        return -1;
    }

    ASM_Pulse_Enable();
    
    printf("   [C] '%s' executado com sucesso.\n", nome_algoritmo);
    return 0; // Sucesso
}


/* ===================================================================
 * LÓGICA DO MENU
 * =================================================================== */

/**
 * @brief Exibe o menu principal e o estado atual.
 */
void exibir_menu(int inicializada, int img_carregada_c, int img_enviada_fpga) {
    // system("clear"); // Descomente para limpar a tela em cada loop (Linux/macOS)
    printf("\n\n=== MENU DE TESTE DA API ===\n");
    printf("ESTADO: API [%s] | Buffer C [%s] | FPGA VRAM [%s]\n",
           inicializada ? "ON" : "OFF",
           img_carregada_c ? "CARREGADA" : "VAZIA",
           img_enviada_fpga ? "CARREGADA" : "VAZIA");
    printf("----------------------------------------------------------\n");
    
    printf(" 1. Inicializar API\n");

    printf("\n--- Carga de Imagem (Buffer C) ---\n");
    printf(" 2. Carregar Imagem BMP\n");
    printf(" 3. Gerar Gradiente\n");
    printf(" 4. Enviar Imagem (Buffer C -> FPGA VRAM)\n");
    
    printf("\n--- Comandos/Algoritmos (FPGA) ---\n");
    printf(" 5. Algoritmo 1: NearestNeighbor\n");
    printf(" 6. Algoritmo 2: PixelReplication\n");
    printf(" 7. Algoritmo 3: Decimation\n");
    printf(" 8. Algoritmo 4: BlockAveraging\n");
    printf(" 9. Atualizar (ASM_Refresh - NOP)\n");
    printf(" 10. Reset (ASM_Reset)\n");
    
    printf("\n----------------------------------------------------------\n");
    printf(" 0. Encerrar API e Sair\n");
    
    printf("\nEscolha uma opcao: ");
}

/**
 * @brief Limpa o buffer de entrada (stdin)
 */
void limpar_buffer_entrada() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * @brief Pausa a execução e espera que o usuário pressione Enter.
 */
void esperar_enter() {
    printf("\nPressione Enter para continuar...");
    limpar_buffer_entrada(); // Limpa qualquer '\n' anterior
    getchar(); // Espera pelo Enter
}

/* ===================================================================
 * MAIN INTERATIVO
 * =================================================================== */
int main() {
    
    // Variáveis de estado
    int api_inicializada = 0;
    int img_carregada_c = 0;   // Imagem no buffer C (RAM)
    int img_enviada_fpga = 0; // Imagem enviada para a VRAM do FPGA
    
    int opcao;
    char nome_arquivo[256];

    // Alocar buffer de imagem (RAM)
    uint8_t *image_data = malloc(IMG_WIDTH * IMG_HEIGHT);
    if (!image_data) {
        printf("ERRO FATAL: Nao foi possivel alocar memoria para a imagem!\n");
        return 1;
    }

    while (1) {
        exibir_menu(api_inicializada, img_carregada_c, img_enviada_fpga);
        
        if (scanf("%d", &opcao) != 1) {
            limpar_buffer_entrada();
            printf("Erro: Entrada invalida. Por favor, digite um numero.\n");
            esperar_enter();
            continue;
        }

        printf("\n"); // Adiciona espaço após a entrada

        switch (opcao) {
            
            // --- OPÇÃO 1: Inicializar ---
            case 1:
                if (api_inicializada) {
                    printf("AVISO: A API ja esta inicializada.\n");
                } else {
                    printf("=== PASSO 1: Inicializando API (API_initialize) ===\n");
                    if (API_initialize() == (void*)-1 || API_initialize() == (void*)-2) {
                        printf("ERRO FATAL: API_initialize falhou. Verifique o sudo e o mmap.\n");
                        free(image_data);
                        return -1;
                    }
                    printf(">>> SUCESSO: API inicializada.\n");
                    api_inicializada = 1;
                }
                break;

            // --- OPÇÃO 2: Carregar BMP ---
            case 2:
                if (!api_inicializada) {
                    printf("ERRO: Inicialize a API primeiro (Opcao 1).\n");
                    break;
                }
                printf("=== PASSO 2: Carregando Imagem BMP ===\n");
                printf("Digite o nome do arquivo BMP: ");
                scanf("%255s", nome_arquivo); // Leitura simples do nome
                
                if (load_bmp(nome_arquivo, image_data) == 0) {
                    printf(">>> SUCESSO: Imagem BMP carregada no buffer C.\n");
                    img_carregada_c = 1;
                    img_enviada_fpga = 0; // Buffer C foi atualizado, FPGA está dessincronizado
                } else {
                    printf("ERRO: Nao foi possivel carregar o BMP.\n");
                }
                break;

            // --- OPÇÃO 3: Gerar Gradiente ---
            case 3:
                if (!api_inicializada) {
                    printf("ERRO: Inicialize a API primeiro (Opcao 1).\n");
                    break;
                }
                printf("=== PASSO 2: Gerando Gradiente ===\n");
                gerar_padrao_teste(image_data);
                printf(">>> SUCESSO: Gradiente gerado no buffer C.\n");
                img_carregada_c = 1;
                img_enviada_fpga = 0; // Buffer C foi atualizado, FPGA está dessincronizado
                break;

            // --- OPÇÃO 4: Enviar Imagem (C -> FPGA) ---
            case 4:
                if (!api_inicializada) {
                    printf("ERRO: Inicialize a API primeiro (Opcao 1).\n");
                    break;
                }
                if (!img_carregada_c) {
                    printf("ERRO: Nenhuma imagem no buffer C. Carregue (2) ou gere (3) primeiro.\n");
                    break;
                }
                printf("=== PASSO 3: Enviando Imagem para FPGA ===\n");
                if (enviar_imagem_para_fpga(image_data) == 0) {
                    printf(">>> SUCESSO: Imagem enviada para a VRAM do FPGA.\n");
                    img_enviada_fpga = 1;
                } else {
                    printf("ERRO FATAL: Falha ao enviar imagem para o FPGA.\n");
                    // Se falhar, melhor fechar
                    goto cleanup_error;
                }
                break;

            // --- OPÇÕES 5-8 (Algoritmos) e 10 (Reset) ---
            case 5: // NearestNeighbor
            case 6: // PixelReplication
            case 7: // Decimation
            case 8: // BlockAveraging
            case 10: // Reset
                if (!api_inicializada) {
                    printf("ERRO: Inicialize a API primeiro (Opcao 1).\n");
                    break;
                }
                if (!img_enviada_fpga) {
                    printf("ERRO: Nenhuma imagem na VRAM do FPGA. Envie uma imagem primeiro (Opcao 4).\n");
                    break;
                }
                
                printf("=== PASSO 4: Executando Algoritmo ===\n");
                int resultado;
                if (opcao == 5) resultado = executar_algoritmo("NearestNeighbor", &NearestNeighbor);
                else if (opcao == 6) resultado = executar_algoritmo("PixelReplication", &PixelReplication);
                else if (opcao == 7) resultado = executar_algoritmo("Decimation", &Decimation);
                else if (opcao == 8) resultado = executar_algoritmo("BlockAveraging", &BlockAveraging);
                else if (opcao == 10) resultado = executar_algoritmo("ASM_Reset", &ASM_Reset);
                
                if (resultado != 0) {
                     printf("ERRO FATAL: Falha na execucao do algoritmo.\n");
                     goto cleanup_error;
                }
                break;

            // --- OPÇÃO 9: Atualizar (Refresh) ---
            case 9:
                if (!api_inicializada) {
                    printf("ERRO: Inicialize a API primeiro (Opcao 1).\n");
                    break;
                }
                printf("=== EXECUTANDO: ASM_Refresh ===\n");
                printf("   [C] Enviando comando ASM_Refresh() (NOP)...\n");
                ASM_Refresh();
                usleep(10000); // Pequena pausa
                printf("   [C] Comando Refresh enviado.\n");
                break;

            // --- OPÇÃO 0: Encerrar ---
            case 0:
                printf("=== PASSO FINAL: Encerrando ===\n");
                if (api_inicializada) {
                    printf("Encerrando API...\n");
                    API_close();
                }
                free(image_data);
                printf("Saindo.\n");
                return 0; // Sai do loop while(1) e encerra o main

            default:
                printf("ERRO: Opcao invalida. Tente novamente.\n");
                break;
        }

        // Pausa para o usuário ler o resultado
        esperar_enter();
    } // Fim do while(1)

cleanup_error:
    printf("\n!!! OCORREU UM ERRO FATAL. ENCERRANDO API. !!!\n");
    if (api_inicializada) {
        API_close();
    }
    free(image_data);
    return -1;
}