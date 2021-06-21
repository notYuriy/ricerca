format ELF64

extrn interrupt_handle
public interrupt_raw_callbacks

; Define interrupt handler with no error code
macro int_no_err_code name, intno {
name:
	sub rsp, 8
	push qword intno
	jmp interrupts_common
}

; Define interrupt handler with error code
macro int_err_code name, intno {
name:
	push qword intno
	jmp interrupts_common
}

; Common interrupt stub
section '.text' executable
interrupts_common:
	push rax
	push rbx
	push rcx
	push rdx
	push rdi
	push rsi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	mov rax, es
	push rax
	mov rax, ds
	push rax
	mov rax, gs
	push rax
	mov rax, fs
	push rax
	mov rdi, rsp
	call interrupt_handle
	pop rax
	mov fs, ax
	pop rax
	mov gs, ax
	pop rax
	mov ds, ax
	pop rax
	mov es, ax
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
	iretq

; Define raw handlers for all interrupt vectors
int_no_err_code isr0, 0
int_no_err_code isr1, 1
int_no_err_code isr2, 2
int_no_err_code isr3, 3
int_no_err_code isr4, 4
int_no_err_code isr5, 5
int_no_err_code isr6, 6
int_no_err_code isr7, 7
int_err_code isr8, 8
int_no_err_code isr9, 9
int_err_code isr10, 10
int_err_code isr11, 11
int_err_code isr12, 12
int_err_code isr13, 13
int_err_code isr14, 14
int_no_err_code isr15, 15
int_no_err_code isr16, 16
int_err_code isr17, 17
int_no_err_code isr18, 18
int_no_err_code isr19, 19
int_no_err_code isr20, 20
int_err_code isr21, 21
; rept starts indexes from 1
rept 234 i {
	int_no_err_code isr_free##i, 21 + i
}

section '.rodata'
; Define table with all raw interrupt callbacks
interrupt_raw_callbacks:
dq isr0
rept 21 i {
	dq isr##i
}
rept 234 i {
	dq isr_free##i
}
