/*
 * =========================================================================
 * main_full_test_bmp.c:
 * =========================================================================
 *
 * Este programa é um "script" de teste completo para a api.s.
 * Ele testa todas as funções exportadas no api.h.
 *
 * AGORA INCLUI UM LOADER DE BMP:
 * - Se um argumento (caminho.bmp) for passado, ele carrega o BMP.
 * - Se nenhum argumento for passado, ele gera o gradiente de teste.
 *
 * FLUXO:
 * 1. Inicializa a API (API_initialize)
 * 2. Carrega imagem (BMP ou Gradiente)
 * 3. Envia imagem para o FPGA (Testa ASM_Store e ASM_Refresh)
 * 4. Lê todas as flags de status (Get_Flag_*)
 * 5. Executa CADA algoritmo (NearestNeighbor, PixelReplication, etc.)
 * 6. Lê todas as flags de status novamente
 * 7. Tenta um teste de falha (ASM_Store em endereço inválido)
 * 8. Fecha a API (API_close)
 *
 */

#include "api.h" // O seu ficheiro .h
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> // Para usleep()
// (stdlib.h já inclui abs() para inteiros)

// Definimos um timeout de C (número de loops de 100ms)
#define C_TIMEOUT_LOOPS 50 // (50 * 100ms = 5 segundos)

/* ===================================================================
 * NOVAS FUNÇÕES: LOADER DE BMP (Seu código)
 * =================================================================== */

// Estrutura do cabeçalho BMP (14 bytes)
#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

// Estrutura do cabeçalho de informação (DIB header)
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

/**
 * @brief Converte um pixel RGB para Grayscale (8 bits).
 */
uint8_t rgb_to_gray(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
}

/**
 * @brief Carrega um arquivo BMP (24, 32 ou 8 bits) e converte para grayscale 8-bit.
 * A imagem deve ter exatamente 320x240.
 */
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
    
    // Verifica dimensões (IMG_WIDTH e IMG_HEIGHT vêm de api.h)
    if (infoHeader.width != IMG_WIDTH || abs(infoHeader.height) != IMG_HEIGHT) {
        printf(" Dimensao incorreta: %dx%d (esperado %dx%d)\n", 
               infoHeader.width, abs(infoHeader.height), IMG_WIDTH, IMG_HEIGHT);
        fclose(file);
        return -1;
    }

    // BMPs podem ser "top-down" (height negativo) ou "bottom-up" (height positivo)
    
    fseek(file, header.offset, SEEK_SET);
    
    int bytes_per_pixel = infoHeader.bits / 8;
    // Calcula o tamanho da linha (row) com padding
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
                // Ignora A (alpha)
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
            
            // Endereço no buffer final (image_data)
            // O BMP "bottom-up" (height > 0) armazena a linha 0 no fundo
            // O seu cálculo (IMG_HEIGHT - 1 - y) inverte isso,
            // colocando a linha 0 do BMP (fundo) na linha 0 do buffer (topo).
            int addr = (IMG_HEIGHT - 1 - y) * IMG_WIDTH + x;
            image_data[addr] = gray;
        }
        
        if (y % 60 == 0) { // Feedback de progresso
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
 * Funções Auxiliares 
 * =================================================================== */

/**
 * @brief Gera um padrão de gradiente horizontal simples.
 * (Usado como fallback se nenhum BMP for fornecido)
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
            if (errors > 10) { // Evita spam
                printf("   [C] Muitos erros, abortando envio.\n");
                return -1;
            }
        }
    }
    
    printf("   [C] Envio de pixels OK.\n");
    
    // Envia o comando de refresh
    printf("   [C] Testando ASM_Refresh()...\n");
    ASM_Refresh();
    usleep(100000); // 100ms de espera

    if (errors > 0) {
        printf("   [C] ERRO: %d falhas de escrita de pixel.\n", errors);
        return -1;
    }
    return 0;
}

/* ===================================================================
 * Funções de Lógica de Teste 
 * =================================================================== */

/**
 * @brief Função genérica para executar e testar um algoritmo assíncrono.
 */
int executar_algoritmo(const char *nome_algoritmo, void (*funcao_algoritmo)(void)) {
    printf("   [C] Executando '%s' (assincrono)...\n", nome_algoritmo);
    
    // 1. Define a instrução E INICIA o hardware
    funcao_algoritmo(); 
    
    printf("   [C] Hardware iniciado. Aguardando FLAG_DONE (polling)...\n");

    // 2. Fazer polling (sondagem) da flag em C com TIMEOUT
    int timeout_c = 0;
    while (ASM_Get_Flag_Done() == 0) {
        usleep(100000); // Espera 100ms
        timeout_c++;
        
        if (timeout_c > C_TIMEOUT_LOOPS) {
            printf("\n   [C] ERRO FATAL: TIMEOUT DO ALGORITMO '%s'!\n", nome_algoritmo);
            printf("   [C] O FPGA não respondeu após %d segundos.\n", C_TIMEOUT_LOOPS / 10);
            return -1; // Falha
        }
    }
    
    printf("   [C] FLAG_DONE recebida para '%s'!\n", nome_algoritmo);

    // 3. Verificar o resultado
    if (ASM_Get_Flag_Error() != 0) {
        printf("   [C] ATENCAO: O FPGA reportou um ERRO (Flag_Error) durante '%s'!\n", nome_algoritmo);
        return -1;
    }

    // 4. Pulsa o enable (APÓS a conclusão, para limpar flag/reconhecer)
    ASM_Pulse_Enable();
    
    printf("   [C] '%s' executado com sucesso.\n", nome_algoritmo);
    return 0; // Sucesso
}

/**
 * @brief Lê e exibe o estado de todas as flags de status.
 */
void testar_flags_status() {
    printf("   [C] Lendo todas as flags de status:\n");
    printf("     - ASM_Get_Flag_Done():     %d\n", ASM_Get_Flag_Done());
    printf("     - ASM_Get_Flag_Error():    %d\n", ASM_Get_Flag_Error());
    printf("     - ASM_Get_Flag_Max_Zoom(): %d\n", ASM_Get_Flag_Max_Zoom());
    printf("     - ASM_Get_Flag_Min_Zoom(): %d\n", ASM_Get_Flag_Min_Zoom());
}

// ===================================================================
// MAIN DE TESTE COMPLETO 
// ===================================================================
int main(int argc, char *argv[]) {
    
    printf("=== INICIANDO TESTE COMPLETO DA API (com loader BMP) ===\n");
    char *bmp_filename = NULL;
    
    // --- 0. Processar Argumentos ---
    if (argc > 2) {
        printf("ERRO: Muitos argumentos.\n");
        printf("Uso: %s [caminho_para_imagem.bmp]\n", argv[0]);
        return 1;
    }
    if (argc == 2) {
        bmp_filename = argv[1];
        printf("Modo: Carregar imagem '%s'\n", bmp_filename);
    } else {
        printf("Modo: Gerar padrao de gradiente (nenhum arquivo fornecido).\n");
    }
    printf("\n");
    
    // Alocar memória para a nossa imagem (grayscale 8-bit)
    uint8_t *image_data = malloc(IMG_WIDTH * IMG_HEIGHT);
    if (!image_data) {
        printf("ERRO FATAL: Nao foi possivel alocar memoria para a imagem!\n");
        return 1;
    }

    // --- 1. Inicializar a API ---
    printf("=== PASSO 1: Inicializando API (API_initialize) ===\n");
    if (API_initialize() == (void*)-1 || API_initialize() == (void*)-2) {
        printf("ERRO FATAL: API_initialize falhou. Verifique o sudo e o mmap.\n");
        free(image_data);
        return -1;
    }
    printf(">>> SUCESSO: API inicializada.\n\n");


    // --- 2. Gerar ou Carregar e Enviar Imagem de Teste ---
    printf("=== PASSO 2: Carregando e Enviando Imagem ===\n");
    
    if (bmp_filename != NULL) {
        // Tentar carregar o BMP
        printf("   [C] Carregando imagem do arquivo: %s\n", bmp_filename);
        if (load_bmp(bmp_filename, image_data) != 0) {
            printf("ERRO FATAL: Nao foi possivel carregar o BMP.\n");
            goto cleanup_error;
        }
    } else {
        // Usar o fallback (gradiente)
        printf("   [C] Nenhum arquivo BMP fornecido. Usando padrao de gradiente.\n");
        gerar_padrao_teste(image_data);
    }
    
    // Agora, enviar a imagem (carregada ou gerada)
    if (enviar_imagem_para_fpga(image_data) != 0) {
        printf("ERRO FATAL: Falha ao enviar imagem para o FPGA.\n");
        goto cleanup_error;
    }
    printf(">>> SUCESSO: Imagem enviada para a VRAM do FPGA.\n\n");


    // --- 3. Testar Flags de Status (Pré-execução) ---
    printf("=== PASSO 3: Testando Leitura de Flags (Pre-execucao) ===\n");
    testar_flags_status();
    printf(">>> SUCESSO: Flags lidas.\n\n");


    // --- 4. Executar TODOS os Algoritmos ---
    printf("=== PASSO 4: Executando Algoritmos Assincronos ===\n");
    
    if (executar_algoritmo("NearestNeighbor", &NearestNeighbor) != 0) goto cleanup_error;
    if (executar_algoritmo("PixelReplication", &PixelReplication) != 0) goto cleanup_error;
    if (executar_algoritmo("Decimation", &Decimation) != 0) goto cleanup_error;
    if (executar_algoritmo("BlockAveraging", &BlockAveraging) != 0) goto cleanup_error;
    if (executar_algoritmo("ASM_Reset", &ASM_Reset) != 0) goto cleanup_error;
    
    printf(">>> SUCESSO: Todos os algoritmos executados.\n\n");

    
    // --- 5. Testar Flags de Status (Pós-execução) ---
    printf("=== PASSO 5: Testando Leitura de Flags (Pos-execucao) ===\n");
    printf("   (Verificando o estado apos os testes, especialmente apos o ASM_Reset)\n");
    testar_flags_status();
    printf(">>> SUCESSO: Flags lidas novamente.\n\n");


    // --- 6. Testar Falha de ASM_Store (Opcional, mas recomendado) ---
    printf("=== PASSO 6: Testando Falha Controlada (ASM_Store) ===\n");
    unsigned int invalid_addr = IMG_SIZE + 1000; // Endereço fora dos limites
    printf("   [C] Tentando escrever no endereco invalido %u (limite: %d)...\n", invalid_addr, IMG_SIZE - 1);
    
    int status = ASM_Store(invalid_addr, 0xFF);
    
    if (status == STORE_ERR_ADDR) {
        printf("   [C] SUCESSO: API reportou o erro esperado (STORE_ERR_ADDR, codigo %d).\n", status);
    } else {
        printf("   [C] AVISO: API retornou %d, mas era esperado %d (STORE_ERR_ADDR).\n", status, STORE_ERR_ADDR);
    }
    printf(">>> SUCESSO: Teste de falha concluído.\n\n");

    
    // --- 7. Limpeza ---
    printf("=== PASSO 7: Encerrando API ===\n");
    API_close();
    free(image_data);
    
    printf("\n>>> TESTE COMPLETO CONCLUIDO COM SUCESSO <<<\n");
    return 0;

// Bloco de limpeza em caso de erro
cleanup_error:
    printf("\n!!! TESTE FALHOU. ENCERRANDO API. !!!\n");
    API_close();
    free(image_data);
    return -1;
}