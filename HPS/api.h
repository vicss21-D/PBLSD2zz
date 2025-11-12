/*
 * =========================================================================
 * API.h: Ficheiro Header (API)
 * =========================================================================
 *
 * Este ficheiro define a interface (protótipos de função e constantes)
 * para a biblioteca de driver em Assembly (api.s).
 *
 * #include "api.h"
 *
 */

#ifndef LIBCOPROCESSADOR_H_
#define LIBCOPROCESSADOR_H_

/* 
    Garante que o C++ possa linkar com estas funções C/Assembly.
*/
#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Constantes (do seu api.s)
 * =================================================================== */

/* Parâmetros da Imagem */
#define IMG_WIDTH 320
#define IMG_HEIGHT 240
#define IMG_SIZE (IMG_WIDTH * IMG_HEIGHT) // 76800

/* * Códigos de Retorno da API
*/
 
/* Códigos de Erro da API_initialize */
#define INIT_ERR_OPEN -1
#define INIT_ERR_MMAP -2

/* Códigos de Erro da ASM_Store */
#define STORE_SUCCESS       0
#define STORE_ERR_ADDR      -1  // Endereço inválido
#define STORE_ERR_TIMEOUT   -2  // Hardware não respondeu (timeout)
#define STORE_ERR_HW        -3  // FPGA reportou um erro (FLAG_ERROR)

/* ===================================================================
 * Protótipos das Funções Públicas (de api.s)
 * =================================================================== */

/**
 * @brief Abre /dev/mem e mapeia a ponte LW HPS-FPGA.
 * Deve ser chamada primeiro. Requer 'sudo'.
 * * @return Um ponteiro virtual (sucesso) ou um código de erro (NULL, -1, -2) se falhar.
 */
extern volatile void* API_initialize(void);

/**
 * @brief Desmapeia a ponte e fecha o /dev/mem.
 * Deve ser chamada no fim do programa.
 */
extern void API_close(void);

/**
 * @brief Envia um pixel para o FPGA (função SÍNCRONA/BLOQUEANTE).
 * Esta função usa o protocolo de "pacote" e espera pelo FLAG_DONE.
 * * @param address O endereço na VRAM do FPGA (0 a 76799).
 * @param pixel_data O valor do pixel (8 bits).
 * @return 0 (Sucesso), -1 (Endereço Inválido), -2 (Timeout), -3 (Erro de Hardware).
 */
extern int ASM_Store(unsigned int address, unsigned char pixel_data);

/**
 * @brief Envia um comando NOP (Refresh) para o FPGA (assíncrono).
 * (Baseado na sua função 'ASM_Refresh', mas usando o pulso seguro).
 */
extern void ASM_Refresh(void);

/**
 * @brief Pulsa o bit ENABLE de forma segura.
 * Esta é a função "disparar" para os algoritmos assíncronos.
 */
extern void ASM_Pulse_Enable(void);

/*
 * ===================================================================
 * Funções de Algoritmo (ASSÍNCRONAS)
 *
 * Estas funções *apenas* definem o opcode.
 * O C deve chamar ASM_Pulse_Enable() para iniciar
 * e ASM_Get_Flag_Done() para verificar a conclusão.
 * ===================================================================
 */

extern void NearestNeighbor(void);   // (Opcode 3: Vizinho Mais Próximo)
extern void PixelReplication(void);  // (Opcode 4: Replicação de Pixel)
extern void Decimation(void);        // (Opcode 6: Decimação)
extern void BlockAveraging(void);    // (Opcode 5: Média de Blocos)
extern void ASM_Reset(void);         // (Opcode 7: Reset)

/*
 * ===================================================================
 * Funções de Leitura de Flag (para Polling)
 *
 * Retornam 1 (verdadeiro/ativo) ou 0 (falso/inativo).
 * ===================================================================
 */

/**
 * @brief Verifica se o FPGA terminou a última operação.
 * @return 1 se (FLAG_DONE == 1), 0 se (FLAG_DONE == 0).
 */
extern int ASM_Get_Flag_Done(void);

/**
 * @brief Verifica se o FPGA reportou um erro.
 * @return 1 se (FLAG_ERROR == 1), 0 se (FLAG_ERROR == 0).
 */
extern int ASM_Get_Flag_Error(void);

/**
 * @brief Verifica se o FPGA atingiu o zoom máximo.
 * @return 1 se (FLAG_MAX_ZOOM == 1), 0 se (FLAG_MAX_ZOOM == 0).
 */
extern int ASM_Get_Flag_Max_Zoom(void);

/**
 * @brief Verifica se o FPGA atingiu o zoom mínimo.
 * @return 1 se (FLAG_MIN_ZOOM == 1), 0 se (FLAG_MIN_ZOOM == 0).
 */
extern int ASM_Get_Flag_Min_Zoom(void);


#ifdef __cplusplus
}
#endif

#endif