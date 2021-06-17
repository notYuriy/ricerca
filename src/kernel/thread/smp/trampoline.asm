bits 64

extern thread_smp_trampoline_code_start
extern thread_smp_trampoline_code_end

section .text
thread_smp_trampoline_code_start:
    incbin "thread/smp/trampoline.bin"
thread_smp_trampoline_code_end:
