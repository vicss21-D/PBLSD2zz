// Finalizado
#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#define MAX_IMAGES 10
#define MAX_FILENAME 100

// Estrutura do cabeçalho BMP (14 bytes)
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

// Converte RGB para Grayscale
uint8_t rgb_to_gray(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
}

// Carrega imagem BMP
int load_bmp(const char *filename, uint8_t *image_data) {
    FILE *file;
    BMPHeader header;
    BMPInfoHeader infoHeader;
    
    file = fopen(filename, "rb");
    if (!file) {
        printf("❌ Erro ao abrir '%s'\n", filename);
        return -1;
    }
    
    fread(&header, sizeof(BMPHeader), 1, file);
    if (header.type != 0x4D42) {
        printf("❌ Arquivo não é BMP válido\n");
        fclose(file);
        return -1;
    }
    
    fread(&infoHeader, sizeof(BMPInfoHeader), 1, file);
    
    if (infoHeader.width != IMG_WIDTH || abs(infoHeader.height) != IMG_HEIGHT) {
        printf("❌ Dimensão incorreta: %dx%d (esperado 320x240)\n", 
               infoHeader.width, abs(infoHeader.height));
        fclose(file);
        return -1;
    }
    
    fseek(file, header.offset, SEEK_SET);
    
    int bytes_per_pixel = infoHeader.bits / 8;
    int row_size = ((infoHeader.width * bytes_per_pixel + 3) / 4) * 4;
    uint8_t *row_data = (uint8_t*)malloc(row_size);
    
    printf("Carregando");
    for (int y = 0; y < IMG_HEIGHT; y++) {
        fread(row_data, 1, row_size, file);
        
        for (int x = 0; x < IMG_WIDTH; x++) {
            uint8_t gray;
            
            if (infoHeader.bits == 32) {
                uint8_t b = row_data[x * 4 + 0];
                uint8_t g = row_data[x * 4 + 1];
                uint8_t r = row_data[x * 4 + 2];
                gray = rgb_to_gray(r, g, b);
            } else if (infoHeader.bits == 24) {
                uint8_t b = row_data[x * 3 + 0];
                uint8_t g = row_data[x * 3 + 1];
                uint8_t r = row_data[x * 3 + 2];
                gray = rgb_to_gray(r, g, b);
            } else if (infoHeader.bits == 8) {
                gray = row_data[x];
            } else {
                printf("\n❌ Formato %d bits não suportado\n", infoHeader.bits);
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

// Envia imagem para FPGA
int send_to_fpga(uint8_t *image_data) {
    int total_pixels = IMG_WIDTH * IMG_HEIGHT;
    int errors = 0;
    
    printf("Enviando para FPGA");
    for (int i = 0; i < total_pixels; i++) {
        if (ASM_Store(i, image_data[i]) != 0) {
            errors++;
        }
        
        if (i % (total_pixels / 10) == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    printf(" OK!\n");
    
    ASM_Refresh();
    usleep(100000);
    
    if (errors > 0) {
        printf("⚠️  %d erros ao enviar pixels\n", errors);
        return -1;
    }
    
    if (ASM_Get_Flag_Error()) {
        printf("❌ Hardware reportou erro!\n");
        return -1;
    }
    
    return 0;
}

// Limpa a tela
void clear_screen() {
    printf("\033[2J\033[H");
}

// Menu principal
void print_main_menu() {
    clear_screen();
    printf("╔════════════════════════════════════════════╗\n");
    printf("║   SISTEMA DE ZOOM - COPROCESSADOR FPGA    ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    printf("  [1] Carregar Imagem\n");
    printf("  [2] Aplicar Zoom\n");
    printf("  [3] ASM_Reset do Sistema\n");
    printf("  [4] Status\n");
    printf("  [0] Sair\n\n");
    printf("Escolha: ");
}

// Menu de seleção de imagens
int select_image_menu(char *filename) {
    clear_screen();
    printf("╔════════════════════════════════════════════╗\n");
    printf("║         SELECIONE UMA IMAGEM (BMP)        ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    // Lista de imagens disponíveis
    char images[MAX_IMAGES][MAX_FILENAME] = {
        "Xadrez.bmp",
        "imagem1.bmp",
        "imagem2.bmp",
        "imagem3.bmp",
        "imagem4.bmp",
        "imagem5.bmp",
        "imagem6.bmp",
        "imagem7.bmp",
        "imagem8.bmp",
        "imagem9.bmp"
    };
    
    int available_count = 0;
    FILE *test_file;
    
    // Verifica quais imagens existem
    for (int i = 0; i < MAX_IMAGES; i++) {
        test_file = fopen(images[i], "rb");
        if (test_file) {
            printf("  [%d] %s\n", i + 1, images[i]);
            fclose(test_file);
            available_count++;
        }
    }
    
    if (available_count == 0) {
        printf("\n❌ Nenhuma imagem BMP encontrada!\n");
        printf("   Adicione arquivos .bmp (320x240) na pasta.\n\n");
        printf("Pressione ENTER para voltar...");
        getchar();
        return -1;
    }
    
    printf("  [0] Voltar\n\n");
    printf("Escolha: ");
    
    int choice;
    scanf("%d", &choice);
    getchar(); // Limpa buffer
    
    if (choice == 0) return -1;
    if (choice < 1 || choice > MAX_IMAGES) {
        printf("❌ Opção inválida!\n");
        sleep(2);
        return -1;
    }
    
    // Verifica se a imagem existe
    test_file = fopen(images[choice - 1], "rb");
    if (!test_file) {
        printf("❌ Imagem não encontrada!\n");
        sleep(2);
        return -1;
    }
    fclose(test_file);
    
    strcpy(filename, images[choice - 1]);
    return 0;
}

// Menu de zoom
int zoom_menu() {
    clear_screen();
    printf("╔════════════════════════════════════════════╗\n");
    printf("║          APLICAR ZOOM / ALGORITMO         ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    printf("  ZOOM IN (Aumentar):\n");
    printf("    [1] Vizinho Mais Próximo (2x, 4x, 8x)\n");
    printf("    [2] Replicação de Pixel (2x, 4x, 8x)\n\n");
    printf("  ZOOM OUT (Diminuir):\n");
    printf("    [3] Decimação (0.5x, 0.25x, 0.125x)\n");
    printf("    [4] Média de Blocos (0.5x, 0.25x, 0.125x)\n\n");
    printf("  [0] Voltar\n\n");
    printf("Escolha: ");
    
    int choice;
    scanf("%d", &choice);
    getchar();
    
    if (choice < 0 || choice > 4) {
        printf("❌ Opção inválida!\n");
        sleep(2);
        return -1;
    }
    
    return choice;
}

// Executa operação de zoom
void execute_zoom(int algorithm) {
    printf("\n");
    
    switch(algorithm) {
        case 1:
            printf("Aplicando Zoom IN - Vizinho Mais Próximo...\n");
            NearestNeighbor();
            ASM_Pulse_Enable();
            break;
        case 2:
            printf("Aplicando Zoom IN - Replicação de Pixel...\n");
            PixelReplication();
            ASM_Pulse_Enable();
            break;
        case 3:
            printf("Aplicando Zoom OUT - Decimação...\n");
            Decimation();
            ASM_Pulse_Enable();
            break;
        case 4:
            printf("Aplicando Zoom OUT - Média de Blocos...\n");
            BlockAveraging();
            ASM_Pulse_Enable();
            break;
        default:
            return;
    }
    
    // Aguarda conclusão
    printf("Processando");
    int timeout = 0;
    while (!ASM_Get_Flag_Done() && timeout < 50) {
        printf(".");
        fflush(stdout);
        usleep(100000); // 100ms
        timeout++;
    }
    
    if (ASM_Get_Flag_Done()) {
        printf(" Concluído!\n");
        
        if (ASM_Get_Flag_Error()) {
            printf("❌ Erro durante operação!\n");
        } else if (ASM_Get_Flag_Max_Zoom()) {
            printf("⚠️  Zoom máximo atingido (8x)\n");
        } else if (ASM_Get_Flag_Min_Zoom()) {
            printf("⚠️  Zoom mínimo atingido (0.125x)\n");
        } else {
            printf("✅ Operação bem-sucedida!\n");
        }
    } else {
        printf(" TIMEOUT!\n");
        printf("❌ Operação não concluiu no tempo esperado\n");
    }
    
    printf("\nPressione ENTER para continuar...");
    getchar();
}

// Mostra status do sistema
void show_status() {
    clear_screen();
    printf("╔════════════════════════════════════════════╗\n");
    printf("║            STATUS DO SISTEMA               ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    printf("FLAGS:\n");
    printf("  - DONE:     %s\n", ASM_Get_Flag_Done() ? "✓ Sim" : "✗ Não");
    printf("  - ERROR:    %s\n", ASM_Get_Flag_Error() ? "✓ Sim (ERRO!)" : "✗ Não");
    printf("  - ZOOM_MAX: %s\n", ASM_Get_Flag_Max_Zoom() ? "✓ Sim (8x)" : "✗ Não");
    printf("  - ZOOM_MIN: %s\n", ASM_Get_Flag_Min_Zoom() ? "✓ Sim (0.125x)" : "✗ Não");
    
    printf("\nDIMENSÕES SUPORTADAS:\n");
    printf("  - Resolução: 320x240 pixels\n");
    printf("  - Formato: BMP (8, 24 ou 32 bits)\n");
    
    printf("\nZOOM DISPONÍVEL:\n");
    printf("  - Zoom IN:  2x, 4x, 8x\n");
    printf("  - Zoom OUT: 0.5x, 0.25x, 0.125x\n");
    
    printf("\n\nPressione ENTER para voltar...");
    getchar();
}

int main() {
    uint8_t *image_data = NULL;
    int system_initialized = 0;
    int image_loaded = 0;
    char current_image[MAX_FILENAME] = "";
    
    // Aloca memória para imagem
    image_data = (uint8_t*)malloc(IMG_WIDTH * IMG_HEIGHT);
    if (!image_data) {
        printf("❌ Erro ao alocar memória!\n");
        return 1;
    }
    
    while (1) {
        print_main_menu();
        
        int choice;
        scanf("%d", &choice);
        getchar(); // Limpa buffer
        
        switch (choice) {
            case 1: { // Carregar Imagem
                char filename[MAX_FILENAME];
                if (select_image_menu(filename) == 0) {
                    // Inicializa sistema se necessário
                    if (!system_initialized) {
                        printf("\nInicializando sistema...\n");
                        API_initialize();
                        ASM_Reset();
                        usleep(10000);
                        system_initialized = 1;
                        printf("✓ Sistema inicializado\n\n");
                    }
                    
                    // Carrega imagem
                    if (load_bmp(filename, image_data) == 0) {
                        if (send_to_fpga(image_data) == 0) {
                            image_loaded = 1;
                            strcpy(current_image, filename);
                            printf("\n✅ '%s' carregada com sucesso!\n", filename);
                            printf("   Imagem visível na VGA.\n");
                        } else {
                            printf("\n❌ Erro ao enviar imagem para FPGA\n");
                        }
                    }
                    
                    printf("\nPressione ENTER para continuar...");
                    getchar();
                }
                break;
            }
            
            case 2: { // Aplicar Zoom
                if (!image_loaded) {
                    printf("\n⚠️  Carregue uma imagem primeiro!\n");
                    sleep(2);
                    break;
                }
                
                int algorithm = zoom_menu();
                if (algorithm > 0) {
                    execute_zoom(algorithm);
                }
                break;
            }
            
            case 3: { // ASM_Reset
                if (!system_initialized) {
                    API_initialize();
                    system_initialized = 1;
                }
                printf("\nASM_Resetando sistema...\n");
                ASM_Reset();
                usleep(10000);
                image_loaded = 0;
                current_image[0] = '\0';
                printf("✓ Sistema ASM_Resetado!\n");
                sleep(2);
                break;
            }
            
            case 4: { // Status
                show_status();
                break;
            }
            
            case 0: { // Sair
                printf("\nEncerrando...\n");
                free(image_data);
                return 0;
            }
            
            default:
                printf("\n❌ Opção inválida!\n");
                sleep(2);
        }
    }
    
    return 0;
}
