@ =========================================================================
@ api.s: LiBrary for FPGA Coprocessor Control
@ =========================================================================

@ --- SYSCALLS (ARM Linux EABI) ---

.equ __NR_open, 5
.equ __NR_close, 6
.equ __NR_munmap, 91
.equ __NR_mmap2, 192

@ ===================================================================
@ Data Section (Constants)
@ ===================================================================

.section .data
    dev_mem_path:  .asciz "/dev/mem"
    FPGA_BRIDGE_BASE: .word 0xFF200000
    FPGA_BRIDGE_SPAN: .word 0x00001000 @ 4 KB

    @ --- HAVE TO FIX ADDRESSES --- DONE (CHECK QSYS LATER)

    @ --- PIOs OFFSETS (Qsys) ---
    .equ PIO_INSTR_OFS,    0x00
    .equ PIO_CONTROL_OFS,  0x10
    .equ PIO_FLAGS_OFS,    0x20

    @ --- INSTRUCTIONS ---
    .equ INSTR_NOP,        0
    .equ INSTR_LOAD,       1
    .equ INSTR_STORE,      2
    .equ INSTR_NHI_ALG,    3
    .equ INSTR_PR_ALG,     4
    .equ INSTR_BA_ALG,     5
    .equ INSTR_NH_ALG,     6
    .equ INSTR_RESET,      7

    @ ======================================================================
    @ BIT MASKS 
    @ =====================================================================

    @ --- CONTROL SIGNALS ---

    .equ ENABLE_BIT_MASK,      1
    .equ SEL_MEM_BIT_MASK,     2

    @ --- FLAGS ---

    .equ FLAG_DONE_MASK,       1
    .equ FLAG_ERROR_MASK,      2
    .equ FLAG_MAX_ZOOM_MASK,   4
    .equ FLAG_MIN_ZOOM_MASK,   8

    @ --- IMAGE PARAMETERS ---
    .equ IMAGE_WIDTH,      320 @ pixels
    .equ IMAGE_HEIGHT,     240 @ pixels
    .equ IMAGE_SIZE,       IMAGE_WIDTH * IMAGE_HEIGHT @ 76800 Bytes

    @ --- SYNCHRONIZATION PARAMETERS ---

    .equ TIMEOUT_LIMIT,    0x3000
    .equ DELAY_COUNT,      0x1000

    @ --- STATUS CODES ---

    @ to add

@ ===================================================================
@ BSS SECTION (Global Variables)
@ ===================================================================
.section .bss
    .lcomm fd_mem, 4           @ File Descriptor for /dev/mem
    .lcomm lw_bridge_ptr, 4    @ virtual pointer for LW Bridge

@ ===================================================================
@ Text Section
@ ===================================================================
.section .text

@ ===================================================================
@ SUB-ROUTINES "PRIVATE" (Non-visible to C)
@ ===================================================================
            

@ _pulse_enable_safe: pulse the ENABLE bit in PIO_CONTROL
@ Doesn't affects other flags (e.g SEL_MEM)

_pulse_enable_safe:
    PUSH    {r2, r3, r4}    @ Salva regs temporários
    LDR     r4, =lw_bridge_ptr
    LDR     r4, [r4]        @ r4 = ponteiro base

    MOV     r2, #1          @ Máscara para o bit 0 (Enable)
    
    LDR     r3, [r4, #PIO_CONTROL_OFS] @ Lê o valor atual (para preservar outros bits)
    
    ORR     r3, r3, r2      @ Seta o bit 0
    STR     r3, [r4, #PIO_CONTROL_OFS]
    
    BIC     r3, r3, r2      @ Limpa o bit 0
    STR     r3, [r4, #PIO_CONTROL_OFS]
    
    POP     {r2, r3, r4}
    BX      LR
    
@ _ASM_Set_Instruction: Internal function
@ ONLY sets the inSTRuction opcode in PIO_INSTR (no pulse or wait)
@ R0 = opcode

_ASM_Set_Instruction:
    PUSH {R0, R4, R8}
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]
    
    MOV R8, #PIO_INSTR_OFS
    STR R0, [R4, R8]         @ writes opcode (R0) in PIO
    
    POP {R0, R4, R8}
    BX LR                    @ Returns (e.g from NearestNeighBor)


@@@@ REVIEW - TO BE CHECKED
@ _ASM_Get_Flag: Internal function
@ Retorna 1 se a flag (passada em R0) estiver ativa, 0 se não
@ ReceBe: R0 = Máscara da Flag (ex: FLAG_DONE_MASK)

_ASM_Get_Flag:
    PUSH {R1, R3, R4}
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]
    
    MOV R1, #PIO_FLAGS_OFS
    LDR R3, [R4, R1]       @ Read flags PIO
    
    TST R3, R0             @ Tests mask (R0)
    
    MOV R0, #0             @ Set the return value (0 = false)
    MOVNE R0, #1           @ if TST != 0 (NE), switch for 1 (true)
    
    POP {R1, R3, R4}
    BX LR                  @ Returns w R0 = 0 or 1

@ ===================================================================
@ PUBLIC FUNCTIONS (VisiBLe to C)
@ ===================================================================

@ --- API_initialize (void) ---
@ Opens and maps bridge. Returns pointer in R0, or error code on error.

.gloBal API_initialize
.type API_initialize, %function

API_initialize:
    PUSH {R4-R11, LR}
    LDR R0, =dev_mem_path
    MOV R1, #2
    MOV R7, #__NR_open

    SVC 0

    CMP R0, #0
    BLT open_fail

    LDR R1, =fd_mem
    STR R0, [R1]

    MOV R0, #0
    LDR R1, =FPGA_BRIDGE_SPAN
    LDR R1, [R1]
    MOV R2, #3 @ PROT_READ | PROT_WRITE
    MOV R3, #1 @ MAP_SHARED
    LDR R4, =fd_mem
    LDR R4, [R4]
    LDR R5, =FPGA_BRIDGE_BASE
    LDR R5, [R5]
    LSR R5, R5, #12
    MOV R7, #__NR_mmap2

    SVC 0

    CMP R0, #0
    BLT mmap_fail         @ MAY VERIFY IF CMP IS CORRECT - TO BE CHECKED
    
    LDR R1, =lw_bridge_ptr
    STR R0, [R1]
    POP {R4-R11, PC}       @ RETURNS WITH POINTER IN R0

open_fail:
    MOV R0, #-1             @ RETURNS (-1) ON OPEN ERROR 
    POP {R4-R11, PC}

mmap_fail:
    MOV R0, #-2             @ RETURNS (-2) ON MAP ERROR
    POP {R4-R11, PC}

.size API_initialize, .-API_initialize

@ --- API_close (void) ---
@ Unmap and close /dev/mem

.gloBal API_close
.type API_close, %function

API_close:
    PUSH {R0-R7, LR}
    LDR R0, =lw_bridge_ptr
    LDR R0, [R0]
    LDR R1, =FPGA_BRIDGE_SPAN
    LDR R1, [R1]
    MOV R7, #__NR_munmap
    SVC 0
    
    LDR R0, =fd_mem
    LDR R0, [R0]
    MOV R7, #__NR_close
    SVC 0
    
    POP {R0-R7, PC}
.size API_close, .-API_close

@ --- ASM_Store (R0=address, R1=pixel_data) ---
@ BLOCKING FUNCTION - !
@ Uses _pulse_enaBLe_internal and _wait_for_done_internal

.gloBal ASM_Store
.type ASM_Store, %function

ASM_Store:
    PUSH    {r4-r6, lr}
    LDR     r4, =lw_bridge_ptr
    LDR     r4, [r4]
    CMP     r0, #IMAGE_SIZE
    BHS     .ADDR_INVALID_WR

.ASM_PACKET_CONSTRUCTION:
    @ Assembles the instruction packet

    @ OPCODE
    MOV     r2, #INSTR_STORE

    @ ADDRESS
    LSL     r3, r0, #3          
    ORR     r2, r2, r3
    
    @ PIXEL DATA
    LSL     r3, r1, #20
    ORR     r2, r2, r3
    
    STR     r2, [r4, #PIO_INSTR_OFS]
    DMB     sy
    BL      _pulse_enable_safe

    MOV     r5, #TIMEOUT_LIMIT

.WAIT_LOOP_WR:
    LDR     r2, [r4, #PIO_FLAGS_OFS]
    TST     r2, #FLAG_DONE_MASK
    BNE     .CHECK_ERROR_WR
    SUBS    r5, r5, #1
    BNE     .WAIT_LOOP_WR
    MOV     r0, #-2         @ Retorna -2 para TIMEOUT
    B       .EXIT_WR

.CHECK_ERROR_WR:
    TST     r2, #FLAG_ERROR_MASK
    BNE     .HW_ERROR_WR
    MOV     r0, #0          @ Sucesso

    MOV     r5, #DELAY_COUNT

.DELAY_LOOP:
    SUBS    r5, r5, #1
    BNE     .DELAY_LOOP
    B       .EXIT_WR

.ADDR_INVALID_WR:
    MOV     r0, #-1
    B       .EXIT_WR

.HW_ERROR_WR:
    MOV     r0, #-3

.EXIT_WR:
    POP     {r4-r6, pc}
.size ASM_Store, .-ASM_Store

.global ASM_Refresh
.type ASM_Refresh, %function
ASM_Refresh:
    PUSH    {r2, r4, lr}
    
    LDR     r4, =lw_bridge_ptr
    LDR     r4, [r4]

    MOV     r2, #INSTR_NOP
    STR     r2, [r4, #PIO_INSTR_OFS]

    BL      _pulse_enable_safe
    
    POP     {r2, r4, pc}
.size ASM_Refresh, .-ASM_Refresh

@ --- ASM_Pulse_Enable (void) --- 
@ Pulse ENABLE bit in PIO_CONTROL

.gloBal ASM_Pulse_Enable
.type ASM_Pulse_Enable, %function

ASM_Pulse_Enable:
    PUSH {LR}
    BL _pulse_enable_safe @ Calls internal function
    POP {PC}
.size ASM_Pulse_Enable, .-ASM_Pulse_Enable

@ ===================================================================
@ ALGORITHM BLOCKS - Non-blocking functions
@ ONLY DEFINE THE INSTRUCTIONS - PULSE THE ENABLE WITH ASM_Pulse_Enable TO RUN
@ ===================================================================

@ --- NearestNeighBor (void) ---
.gloBal NearestNeighbor
.type NearestNeighbor, %function

NearestNeighbor:
    PUSH {LR}
    MOV R0, #INSTR_NHI_ALG       @ opcode
    BL _ASM_Set_Instruction      @ calls internal function
    POP {PC}                     @ return (no pulse, no wait)
.size NearestNeighbor, .-NearestNeighbor

@ --- PixelReplication (void) ---
.gloBal PixelReplication
.type PixelReplication, %function

PixelReplication:
    PUSH {LR}
    MOV R0, #INSTR_PR_ALG
    BL _ASM_Set_Instruction
    POP {PC}
.size PixelReplication, .-PixelReplication

@ --- Decimation (void) ---
.gloBal Decimation
.type Decimation, %function

Decimation:
    PUSH {LR}
    MOV R0, #INSTR_NH_ALG
    BL _ASM_Set_Instruction
    POP {PC}
.size Decimation, .-Decimation

@ --- BlockAveraging (void) ---
.gloBal BlockAveraging
.type BlockAveraging, %function

BlockAveraging:
    PUSH {LR}
    MOV R0, #INSTR_BA_ALG
    BL _ASM_Set_Instruction
    POP {PC}
.size BlockAveraging, .-BlockAveraging

@ --- RESET FUNCTION ---
@ --- ASM_Run_Reset (void) ---
.gloBal ASM_Reset
.type ASM_Reset, %function

ASM_Reset:
    PUSH {LR}
    MOV R0, #INSTR_RESET
    BL _ASM_Set_Instruction
    POP {PC}
.size ASM_Reset, .-ASM_Reset

@ ===================================================================
@ GET FLAG FUNCTIONS
@ RETURNS 1 (true) or 0 (false)
@ ===================================================================

@ FLAG DONE gets high when an operation is complete

.gloBal ASM_Get_Flag_Done
.type ASM_Get_Flag_Done, %function

ASM_Get_Flag_Done:
    PUSH {LR}
    MOV R0, #FLAG_DONE_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Done, .-ASM_Get_Flag_Done

.gloBal ASM_Get_Flag_Error
.type ASM_Get_Flag_Error, %function

@ FLAG ERROR gets high when an error occurs during an operation

ASM_Get_Flag_Error:
    PUSH {LR}
    MOV R0, #FLAG_ERROR_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Error, .-ASM_Get_Flag_Error

.gloBal ASM_Get_Flag_Max_Zoom
.type ASM_Get_Flag_Max_Zoom, %function

@ FLAG MAX ZOOM gets high when maximum zoom level is reached

ASM_Get_Flag_Max_Zoom:
    PUSH {LR}
    MOV R0, #FLAG_MAX_ZOOM_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Max_Zoom, .-ASM_Get_Flag_Max_Zoom

.gloBal ASM_Get_Flag_Min_Zoom
.type ASM_Get_Flag_Min_Zoom, %function

@ FLAG MIN ZOOM gets high when minimum zoom level is reached

ASM_Get_Flag_Min_Zoom:
    PUSH {LR}
    MOV R0, #FLAG_MIN_ZOOM_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Min_Zoom, .-ASM_Get_Flag_Min_Zoom