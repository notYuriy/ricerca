format ELF64

public thread_smp_trampoline_code_start
public thread_smp_trampoline_code_end

; GDT descripor
struc gdt_descriptor base, limit, executable, long
{
	dw limit and 0xffff
	dw (base and 0xffff)
	db (base shr 16) and 0xff
	db ((executable shl 3) or 010b) or (1001b shl 4)
	db ((limit shr 16) and 0xf) or 10000000b or (long shl 5) or ((not long) shl 6)
	db ((base shr 24) and 0xff)
}

; Per cpu area member offsets
pc_numa_id = 8
pc_acpi_id = 12
pc_apic_id = 16
pc_logical_id = 20
pc_status = 24
pc_interrupt_stack_top = 32
pc_scheduler_stack_top = 40

; Arguments passed from the kernel
ta_kernel_cr3 = 0x70000
ta_cpu_locals = 0x70008
ta_cpu_locals_size = 0x70010
ta_max_cpus = 0x70018
ta_callback = 0x70020

; LAPIC spurious interrupt vector
spur_vec = 127

; Embed trampoline code in .rodata section
section '.rodata'
thread_smp_trampoline_code_start:

; CPU starts at address 0x71000 in real mode. However, we load cs to 0x7100, so org 0x0000 would
; work
org 0x0000
relative_start:
use16

start16:
	; CPU starts at 0x71000. Set segment registers to 0x7100 so that
	; we can access trampoline data in real mode
	mov ax, 0x7100
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Load GDT
	lgdt [gdt.pointer]

	; Enable protected mode
	mov eax, cr0
	or eax, 1
	mov cr0, eax

use32
	; FASM is a bit dumb. Let's emit far jump manually
	db 66h
	db 0eah
	dd start32
	dw gdt.code

; Make FASM understand that we are, in fact, at 0x71000
bootstrap_code_end:
org 0x71000 + (bootstrap_code_end - relative_start)

start32:
	; Update all non-code segments
	mov ax, gdt.data
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Enable 5level paging if present
	mov eax, 7
	xor ecx, ecx
	cpuid
	test ecx, 1 shl 16
	jz .enabling_paging
	mov eax, cr4
	or eax, 1 shl 12
	mov cr4, eax

.enabling_paging:

	; Load CR3
	mov eax, dword [ta_kernel_cr3]
	mov cr3, eax

	; Enable PAE
	mov eax, cr4
	or eax, 1 shl 5
	mov cr4, eax

	; Set long mode MSR bit
	mov ecx, 0xc0000080
	rdmsr
	or eax, 1 shl 8
	wrmsr

	; Enable paging
	mov eax, cr0
	or eax, 1 shl 31
	mov cr0, eax

	; Jump to the long mode
	jmp gdt.code64:start64

use64
start64:
	; Set non-code descriptors
	mov ax, gdt.data64
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Store highest supported CPUID in esi
	mov eax, 0x80000000
	cpuid
	mov esi, eax

	; Query x2apic support
	mov eax, 1
	xor ecx, ecx
	cpuid
	test ecx, 1 shl 21
	jz .xapic

.x2apic:
	; Check if 0x1f leaf exists
	mov eax, 0x1f
	cmp eax, esi
	jg .use_bh

	; Query x2APIC id and store it in eax
	mov ecx, 0
	cpuid
	mov eax, edx

	jmp .logical_id_lookup

.use_bh:
	; Query x2APIC id and store it in eax
	mov eax, 0xb
	mov ecx, 0
	cpuid
	mov eax, edx

	jmp .logical_id_lookup

.xapic:
	; Query xAPIC id and store it in eax
	mov eax, 1
	mov ecx, 0
	cpuid
	shr ebx, 24
	mov eax, ebx

.logical_id_lookup:
	; Load per cpu array pointer from cpu_locals_pointer
	mov rdi, qword [ta_cpu_locals]

	; Load entry size in rbx
	mov rbx, qword [ta_cpu_locals_size]

	; Iterate over it. rdi will point to the current entry, and rcx tracks the index
	mov rcx, 0

.lookup_loop:
	; If we reached end of per-cpu array, move on to the next entry
	cmp rcx, qword [ta_max_cpus]
	je .lookup_loop_end

	; Load APIC id in edx and compare it with eax. If they are not equal, move to the next entry
	mov edx, dword [rdi + pc_apic_id]
	cmp edx, eax
	jne .lookup_loop_next

	; Alright, here it is. Load logical cpu id in esi
	mov esi, dword [rdi + pc_logical_id]

	; Exit loop
	jmp .bootup_end

.lookup_loop_next:
	; Increment RCX and move rdi to the next entry
	inc rcx
	add rdi, rbx
	jmp .lookup_loop

.lookup_loop_end:
	; If we reached end of the loop, something wrong happened. Hang forever, so that kernel gives up
	; on booting this AP
	hlt
	jmp .lookup_loop_end

.bootup_end:
	; Switch to the scheduler stack
	mov rsp, qword [rdi + pc_scheduler_stack_top]
	; Prepare registers for the call
	mov edi, esi
	; Get pointer to the bootstrap routine
	mov rax, qword [ta_callback]
	jmp rax
	; Hang
	cli
.hang:
	hlt
	jmp .hang


; GDT used to enter protected/long mode
gdt:
; NULL descriptor
.null_descr dq 0
.null = 0
; 32 bit code descriptor
.code_descr gdt_descriptor 0, 0fffffh, 1, 0
.code = 0x8
; 32 bit data descriptor
.data_descr gdt_descriptor 0, 0fffffh, 0, 0
.data = 0x10
; 64 bit code descriptor
.code_descr64 gdt_descriptor 0, 0fffffh, 1, 1
.code64 = 0x18
; 64 bit data descriptor
.data_descr64 gdt_descriptor 0, 0fffffh, 0, 1
.data64 = 0x20
; GDT end label
.end:
; In DS-relative addressing (when DS is set to 0x7100), we need to substract
; 0x71000 from the actual address
.pointer = $ - 0x71000
.pointer32:
	dw gdt.end - gdt - 1
	dd gdt

relative_end = $ - 0x71000

thread_smp_trampoline_code_end = thread_smp_trampoline_code_start + relative_end - relative_start
