/*
 * =========================================================================
 * main_simple_test.c:
 * =========================================================================
 *
 * Este programa é um "script" simples para testar o driver Assembly (api.s)
 * sem a complexidade de menus ou leitura de ficheiros BMP.
 *
 * FLUXO:
 * 1. Inicializa a API (Lib)
 * 2. Gera um padrão de imagem (gradiente)
 * 3. Envia a imagem para o FPGA (write_pixel x 76800)
 * 4. Executa UM algoritmo (ex: Vizinho_Prox)
 * 5. Faz polling (sondagem) com timeout
 * 6. Fecha a API (encerraLib)
 *
 *
 */

#include "api.h" // (O seu ficheiro .h com os protótipos)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> // Para usleep()

// IMG_WIDTH (320)
// IMG_HEIGHT (240)
// E todos os protótipos das suas funções (Lib, encerraLib, write_pixel, etc.)

// Definimos um timeout de C (número de loops de 100ms)
#define C_TIMEOUT_LOOPS 50 // (50 * 100ms = 5 segundos)

/**
 * @brief Gera um padrão de gradiente horizontal simples.
 */
void gerar_padrao_teste(uint8_t *image_data) {
    printf("   [C] Gerando padrao de teste (gradiente 320x240)...\n");
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            // Cria um gradiente de 0 (esquerda) a 255 (direita)
            uint8_t pixel_value = (uint8_t)((x * 255) / (IMG_WIDTH - 1));
            image_data[y * IMG_WIDTH + x] = pixel_value;
        }
    }
}

/**
 * @brief Envia o buffer de imagem para a VRAM do FPGA.
 */
int enviar_imagem_para_fpga(uint8_t *image_data) {
    int total_pixels = IMG_WIDTH * IMG_HEIGHT;
    int errors = 0;

    printf("   [C] Enviando %d pixels para o FPGA (sincrono)...\n", total_pixels);
    printf("   [C] Progresso: ");
    
    for (int i = 0; i < total_pixels; i++) {
        int status = ASM_Store(i, image_data[i]);
        if (status != 0) {
            printf("\n   [C] ERRO: write_pixel falhou no pixel %d (codigo %d)\n", i, status);
            errors++;
        }

        // Imprime um "." a cada 10%
        if (i > 0 && (i % (total_pixels / 10) == 0)) {
            printf("+10%%");
            fflush(stdout);
        }
    }
    
    printf(" OK!\n");
    
    // Envia o comando de refresh (do seu maestro C)
    ASM_Refresh();
    usleep(100000); // 100ms de espera

    if (errors > 0) {
        printf("   [C] ERRO: %d falhas de escrita de pixel.\n", errors);
        return -1;
    }
    return 0;
}

/**
 * @brief Executa e espera por um algoritmo (lógica de polling do seu maestro C)
 */
int executar_algoritmo_teste() {
    printf("   [C] Executando 'Vizinho_Prox' (assincrono)...\n");
    
    // 1. Define a instrução E pulsa o enable
    NearestNeighbor(); 
    
    printf("   [C] Hardware iniciado. Aguardando FLAG_DONE (polling)...\n");

    // 2. Fazer polling (sondagem) da flag em C com TIMEOUT
    int timeout_c = 0;
    while (ASM_Get_Flag_Done() == 0) {
        usleep(100000); // Espera 100ms
        timeout_c++;
        
        if (timeout_c > C_TIMEOUT_LOOPS) {
            printf("\n   [C] ERRO FATAL: TIMEOUT DO ALGORITMO!\n");
            printf("   [C] O FPGA não respondeu após %d segundos.\n", C_TIMEOUT_LOOPS / 10);
            return -1; // Falha
        }
    }
    
    printf("   [C] FLAG_DONE recebida!\n");

    // 3. Verificar o resultado
    if (ASM_Get_Flag_Error() != 0) {
        printf("   [C] ATENCAO: O FPGA reportou um ERRO (Flag_Error)!\n");
        return -1;
    }

    ASM_Pulse_Enable();
    
    printf("[C] Algoritmo executado com sucesso.\n");
    return 0; // Sucesso
}


// ===================================================================
// MAIN DE TESTE SIMPLES
// ===================================================================
int main() {
    // Alocar memória para a nossa imagem de teste
    uint8_t *image_data = malloc(IMG_WIDTH * IMG_HEIGHT);
    if (!image_data) {
        printf("ERRO FATAL: Nao foi possivel alocar memoria para a imagem!\n");
        return 1;
    }

    // --- 1. Inicializar a API ---
    printf("=== PASSO 1: Inicializando API (Lib) ===\n");
    if (API_initialize() == (void*)-1 || API_initialize() == (void*)-2) {
        printf("ERRO FATAL: API_initialize falhou. Verifique o sudo e o mmap.\n");
        free(image_data);
        return -1;
    }
    printf(">>> SUCESSO: API inicializada.\n\n");


    // --- 2. Gerar e Enviar Imagem de Teste ---
    printf("=== PASSO 2: Gerando e Enviando Imagem ===\n");
    gerar_padrao_teste(image_data);
    
    if (enviar_imagem_para_fpga(image_data) != 0) {
        printf("ERRO FATAL: Falha ao enviar imagem para o FPGA.\n");
        API_close();
        free(image_data);
        return -1;
    }
    printf(">>> SUCESSO: Imagem enviada para a VRAM do FPGA.\n\n");


    // --- 3. Executar Algoritmo de Teste ---
    printf("=== PASSO 3: Executando Algoritmo ===\n");
    if (executar_algoritmo_teste() != 0) {
        printf("ERRO FATAL: Falha ao executar o algoritmo.\n");
        API_close();
        free(image_data);
        return -1;
    }
    printf(">>> SUCESSO: Algoritmo executado.\n\n");

    
    // --- 4. Limpeza ---
    printf("=== PASSO 4: Encerrando API ===\n");
    API_close();
    free(image_data);
    
    printf("\n>>> TESTE CONCLUIDO COM SUCESSO <<<\n");
    return 0;
}