@ =========================================================================
@ api.s: Library for FPGA Coprocessor Control
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

    @ --- REVIEW ADDRESSES - TO BE CHECKED ---

    @ --- PIOs OFFSETS (Qsys) ---
    .equ PIO_INSTR_OFS,    0x00
    .equ PIO_DATA_IN_OFS,  0x10
    .equ PIO_MEM_ADDR_OFS, 0x20
    .equ PIO_CONTROL_OFS,  0x30
    .equ PIO_FLAGS_OFS,    0x40

    @ --- INSTRUCTIONS ---
    .equ INSTR_NOP,        0
    .equ INSTR_LOAD,       1
    .equ INSTR_STORE,      2
    .equ INSTR_NHI_ALG,    3
    .equ INSTR_PR_ALG,     4
    .equ INSTR_BA_ALG,     5
    .equ INSTR_NH_ALG,     6
    .equ INSTR_RESET,      7
        
    @ --- BIT MASKS ---
    .equ ENABLE_BIT_MASK,      1
    .equ SEL_MEM_BIT_MASK,     2
    .equ FLAG_DONE_MASK,       1
    .equ FLAG_ERROR_MASK,      2
    .equ FLAG_MAX_ZOOM_MASK,   4
    .equ FLAG_MIN_ZOOM_MASK,   8

@ ===================================================================
@ BSS SECTION (Global Variables)
@ ===================================================================
.section .bss
    .lcomm fd_mem, 4           @ File Descriptor for /dev/mem
    .lcomm lw_bridge_ptr, 4    @ virtual pointer for LW bridge

@ ===================================================================
@ Text Section
@ ===================================================================
.section .text

@ ===================================================================
@ SUB-ROUTINES "PRIVATE" (Non-visible to C)
@ ===================================================================

@ wait_for_done: wait (polling) for FLAG_DONE
@ Only used by ASM_Store (blocking)
_wait_for_done_internal:
    PUSH {R0, R1, R2, R3, R4}
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]
    
    MOV R1, #PIO_FLAGS_OFS
    MOV R2, #FLAG_DONE_MASK

poll_loop:
    LDR R3, [R4, R1]       
    TST R3, R2             
    B.EQ poll_loop          
    
    POP {R0, R1, R2, R3, R4}
    BX LR                  

@ _pulse_enable_internal: pulse the ENABLE bit in PIO_CONTROL
@ Doesn't affects other flags (e.g SEL_MEM)
_pulse_enable_internal:
    PUSH {R0, R1, R2, R3, R4}
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]

    MOV R1, #PIO_CONTROL_OFS
    MOV R2, #ENABLE_BIT_MASK

    LDR R3, [R4, R1]       
    ORR R3, R3, R2         
    STR R3, [R4, R1]       
    BIC R3, R3, R2         
    STR R3, [R4, R1]       
    
    POP {R0, R1, R2, R3, R4}
    BX LR
    
@ _ASM_Set_Instruction: Internal function
@ ONLY sets the instruction opcode in PIO_INSTR (no pulse or wait)
@ R0 = opcode

_ASM_Set_Instruction:
    PUSH {R0, R4, R8}
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]
    
    MOV R8, #PIO_INSTR_OFS
    STR R0, [R4, R8]         @ writes opcode (R0) in PIO
    
    POP {R0, R4, R8}
    BX LR                    @ Returns (e.g from NearestNeighbor)


@@@@ REVIEW - TO BE CHECKED
@ _ASM_Get_Flag: Internal function
@ Retorna 1 se a flag (passada em R0) estiver ativa, 0 se não
@ Recebe: R0 = Máscara da Flag (ex: FLAG_DONE_MASK)

_ASM_Get_Flag:
    PUSH {R1, R3, R4}
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]
    
    MOV R1, #PIO_FLAGS_OFS
    LDR R3, [R4, R1]       @ Read flags PIO
    
    TST R3, R0             @ Testa a máscara (de R0)
    
    MOV R0, #0             @ Prepara o valor de retorno (0 = falso)
    MOV.NE R0, #1          @ Se TST Não for Zero (NE), muda para 1 (verdadeiro)
    
    POP {R1, R3, R4}
    BX LR                  @ Returns w R0 = 0 or 1

@ ===================================================================
@ PUBLIC FUNCTIONS (Visible to C)
@ ===================================================================

@ --- API_initialize(void) ---
@ Opens e maps bridge. Returns pointer in R0, or 0 (NULL) on error.

.global API_initialize
.type API_initialize, %function

API_initialize:
    PUSH {R4-R11, LR}
    LDR R0, =dev_mem_path
    MOV R1, #2
    MOV R7, #__NR_open

    SVC 0

    CMP R0, #0
    B.LT init_fail

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
    B.LT init_fail         @ MAY VERIFY IF CMP IS CORRECT - TO BE CHECKED
    
    LDR R1, =lw_bridge_ptr
    STR R0, [R1]
    POP {R4-R11, PC}       @ RETURNS WITH POINTER IN R0

init_fail:
    MOV R0, #0             @ RETURNS NULL (0) ON ERROR
    POP {R4-R11, PC}
.size API_initialize, .-API_initialize

@ --- API_close (void) ---
@ Unmap and close /dev/mem

.global API_close
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
@ Uses _pulse_enable_internal and _wait_for_done_internal

.global ASM_Store
.type ASM_Store, %function

ASM_Store:
    PUSH {R0-R4, R8-R11, LR}
    
    @ R0 = 'address', R1 = 'pixel_data'
    
    LDR R4, =lw_bridge_ptr
    LDR R4, [R4]

    MOV R8, #PIO_INSTR_OFS
    MOV R9, #PIO_MEM_ADDR_OFS
    MOV R10, #PIO_DATA_IN_OFS
    MOV R11, #INSTR_STORE

    STR R11, [R4, R8]
    STR R0, [R4, R9]
    STR R1, [R4, R10]
    
    BL      _pulse_enable_internal  @ pulse
    BL      _wait_for_done_internal @ wait
    
    POP {R0-R4, R8-R11, PC}
.size ASM_Store, .-ASM_Store

@ --- ASM_Pulse_Enable (void) --- 
@ Pulse ENABLE bit in PIO_CONTROL

.global ASM_Pulse_Enable
.type ASM_Pulse_Enable, %function

ASM_Pulse_Enable:
    PUSH {LR}
    BL _pulse_enable_internal @ Calls internal function
    POP {PC}
.size ASM_Pulse_Enable, .-ASM_Pulse_Enable

@ ===================================================================
@ ALGORITHM BLOCKS - Non-blocking functions
@ ONLY DEFINE THE INSTRUCTIONS - PULSE THE ENABLE WITH ASM_Pulse_Enable TO RUN
@ e ASM_Get_Flag_Done para executá-las.
@ ===================================================================

@ --- NearestNeighbor (void) ---
.global NearestNeighbor
.type NearestNeighbor, %function

NearestNeighbor:
    PUSH {LR}
    MOV R0, #INSTR_NHI_ALG       @ opcode
    BL _ASM_Set_Instruction      @ calls internal function
    POP {PC}                     @ return (no pulse, no wait)
.size NearestNeighbor, .-NearestNeighbor

@ --- PixelReplication (void) ---
.global PixelReplication
.type PixelReplication, %function

PixelReplication:
    PUSH {LR}
    MOV R0, #INSTR_PR_ALG
    BL _ASM_Set_Instruction
    POP {PC}
.size PixelReplication, .-PixelReplication

@ --- Decimation (void) ---
.global Decimation
.type Decimation, %function

Decimation:
    PUSH {LR}
    MOV R0, #INSTR_NH_ALG
    BL _ASM_Set_Instruction
    POP {PC}
.size Decimation, .-Decimation

@ --- BlockAveraging (void) ---
.global BlockAveraging
.type BlockAveraging, %function

BlockAveraging:
    PUSH {LR}
    MOV R0, #INSTR_BA_ALG
    BL _ASM_Set_Instruction
    POP {PC}
.size BlockAveraging, .-BlockAveraging

@ --- RESET FUNCTION ---
@ --- ASM_Run_Reset (void) ---
.global ASM_Reset
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

.global ASM_Get_Flag_Done
.type ASM_Get_Flag_Done, %function

ASM_Get_Flag_Done:
    PUSH {LR}
    MOV R0, #FLAG_DONE_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Done, .-ASM_Get_Flag_Done

.global ASM_Get_Flag_Error
.type ASM_Get_Flag_Error, %function

@ FLAG ERROR gets high when an error occurs during an operation

ASM_Get_Flag_Error:
    PUSH {LR}
    MOV R0, #FLAG_ERROR_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Error, .-ASM_Get_Flag_Error

.global ASM_Get_Flag_Max_Zoom
.type ASM_Get_Flag_Max_Zoom, %function

@ FLAG MAX ZOOM gets high when maximum zoom level is reached

ASM_Get_Flag_Max_Zoom:
    PUSH {LR}
    MOV R0, #FLAG_MAX_ZOOM_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Max_Zoom, .-ASM_Get_Flag_Max_Zoom

.global ASM_Get_Flag_Min_Zoom
.type ASM_Get_Flag_Min_Zoom, %function

@ FLAG MIN ZOOM gets high when minimum zoom level is reached

ASM_Get_Flag_Min_Zoom:
    PUSH {LR}
    MOV R0, #FLAG_MIN_ZOOM_MASK
    BL _ASM_Get_Flag
    POP {PC}
.size ASM_Get_Flag_Min_Zoom, .-ASM_Get_Flag_Min_Zoom