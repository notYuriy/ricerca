format ELF64

public thread_smp_trampoline_code_start
public thread_smp_trampoline_code_end

section '.text' executable
thread_smp_trampoline_code_start:
    file "thread/smp/trampoline.bin"
thread_smp_trampoline_code_end:
