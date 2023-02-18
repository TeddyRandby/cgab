	.file	"vm.c"
	.text
	.p2align 4
	.globl	gab_vm_container_cb
	.type	gab_vm_container_cb, @function
gab_vm_container_cb:
.LFB228:
	.cfi_startproc
	movq	%rdx, %rdi
	jmp	free@PLT
	.cfi_endproc
.LFE228:
	.size	gab_vm_container_cb, .-gab_vm_container_cb
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"./include/vector.h"
.LC1:
	.string	"index < self->len"
	.text
	.p2align 4
	.type	v_gab_constant_val_at.part.0, @function
v_gab_constant_val_at.part.0:
.LFB247:
	.cfi_startproc
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	leaq	__PRETTY_FUNCTION__.8(%rip), %rcx
	movl	$76, %edx
	leaq	.LC0(%rip), %rsi
	leaq	.LC1(%rip), %rdi
	call	__assert_fail@PLT
	.cfi_endproc
.LFE247:
	.size	v_gab_constant_val_at.part.0, .-v_gab_constant_val_at.part.0
	.section	.rodata.str1.1
.LC2:
	.string	"./include/object.h"
.LC3:
	.string	"offset < self->len"
	.text
	.p2align 4
	.type	gab_obj_record_get.part.0, @function
gab_obj_record_get.part.0:
.LFB250:
	.cfi_startproc
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	leaq	__PRETTY_FUNCTION__.2(%rip), %rcx
	movl	$405, %edx
	leaq	.LC2(%rip), %rsi
	leaq	.LC3(%rip), %rdi
	call	__assert_fail@PLT
	.cfi_endproc
.LFE250:
	.size	gab_obj_record_get.part.0, .-gab_obj_record_get.part.0
	.p2align 4
	.type	gab_obj_record_set.part.0, @function
gab_obj_record_set.part.0:
.LFB251:
	.cfi_startproc
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	leaq	__PRETTY_FUNCTION__.1(%rip), %rcx
	movl	$400, %edx
	leaq	.LC2(%rip), %rsi
	leaq	.LC3(%rip), %rdi
	call	__assert_fail@PLT
	.cfi_endproc
.LFE251:
	.size	gab_obj_record_set.part.0, .-gab_obj_record_set.part.0
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align 8
.LC4:
	.string	"[\033[32m%.*s\033[0m] Error near \033[34m%s\033[0m:\n\t\342\225\255 \033[31m%04lu \033[0m%.*s\n\t\342\224\202      \033[33m%.*s\033[0m\n\t\342\225\260\342\224\200> "
	.section	.rodata.str1.1
.LC5:
	.string	"\033[33m%s. \033[0m\033[32m"
.LC6:
	.string	"\n\033[0m"
	.section	.rodata.str1.8
	.align 8
.LC7:
	.string	"[\033[32m%.*s\033[0m] Called at:\n\t  \033[31m%04lu \033[0m%.*s\n\t       \033[33m%.*s\033[0m\n"
	.text
	.p2align 4
	.globl	vm_error
	.type	vm_error, @function
vm_error:
.LFB229:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movq	%rdi, %rbx
	subq	$328, %rsp
	.cfi_def_cfa_offset 384
	movq	%rsi, 56(%rsp)
	movl	%edx, 92(%rsp)
	movq	%rcx, 96(%rsp)
	movq	%r8, 176(%rsp)
	movq	%r9, 184(%rsp)
	testb	%al, %al
	je	.L10
	vmovaps	%xmm0, 192(%rsp)
	vmovaps	%xmm1, 208(%rsp)
	vmovaps	%xmm2, 224(%rsp)
	vmovaps	%xmm3, 240(%rsp)
	vmovaps	%xmm4, 256(%rsp)
	vmovaps	%xmm5, 272(%rsp)
	vmovaps	%xmm6, 288(%rsp)
	vmovaps	%xmm7, 304(%rsp)
.L10:
	movl	$1064992, %edi
	movq	%fs:40, %rax
	movq	%rax, 136(%rsp)
	xorl	%eax, %eax
	call	malloc@PLT
	movq	56(%rsp), %r15
	movl	$1064992, %edx
	movq	%r15, %rsi
	movq	%rax, %rdi
	call	memcpy@PLT
	movq	%rax, %r11
	movq	16(%r15), %rax
	leaq	1048608(%r15), %rdx
	leaq	32(%r11), %rsi
	movq	%rax, 8(%rsp)
	subq	%rdx, %rax
	leaq	1048608(%r11,%rax), %rdx
	movq	%rsi, 64(%rsp)
	addq	24(%r15), %rsi
	leaq	32(%r15), %rdi
	movq	%rdx, 16(%r11)
	movq	%rsi, %rdx
	subq	%rdi, %rdx
	movq	%rdx, 24(%r11)
	movzbl	(%r15), %edx
	movq	%rdi, 72(%rsp)
	testb	$2, %dl
	je	.L11
	movq	%rax, %rsi
	sarq	$5, %rsi
	movq	%rsi, 8(%rsp)
	je	.L11
	movq	%r11, 80(%rsp)
	movq	%rbx, 104(%rsp)
	leaq	1048608(%r15,%rax), %r13
	movq	%r13, %r12
	.p2align 4,,10
	.p2align 3
.L28:
	movq	64(%rsp), %rax
	movq	72(%rsp), %rcx
	movq	8(%rsp), %rdx
	addq	16(%r12), %rax
	subq	%rcx, %rax
	movq	80(%rsp), %rcx
	salq	$5, %rdx
	movq	%rax, 1048624(%rcx,%rdx)
	movq	(%r12), %rax
	movq	32(%rax), %rax
	movq	32(%rax), %rax
	movzwl	138(%rax), %edx
	cmpq	24(%rax), %rdx
	jnb	.L50
	movq	16(%rax), %rax
	movabsq	$1125899906842623, %rdi
	andq	(%rax,%rdx,8), %rdi
	call	gab_obj_string_ref@PLT
	movq	%rax, 24(%rsp)
	movq	(%r12), %rax
	movq	8(%r12), %r13
	movq	32(%rax), %rax
	movq	%rdx, 32(%rsp)
	movq	32(%rax), %r15
	subq	40(%r15), %r13
	leaq	-1(%r13), %rbp
	cmpq	72(%r15), %rbp
	jnb	.L51
	movq	64(%r15), %rax
	movq	8(%r15), %rcx
	movq	(%rax,%rbp,8), %rax
	movq	%rax, 16(%rsp)
	decq	%rax
	cmpq	16(%rcx), %rax
	jnb	.L19
	salq	$4, %rax
	addq	8(%rcx), %rax
	movq	(%rax), %r14
	movl	8(%rax), %ebx
	jmp	.L15
	.p2align 4,,10
	.p2align 3
.L17:
	incq	%r14
	decl	%ebx
	je	.L16
.L15:
	movzbl	(%r14), %edi
	call	is_whitespace@PLT
	testb	%al, %al
	jne	.L17
.L16:
	cmpq	96(%r15), %rbp
	jnb	.L52
	movq	88(%r15), %rax
	leaq	gab_token_names(%rip), %rcx
	movzbl	-1(%rax,%r13), %eax
	movq	(%rcx,%rax,8), %rax
	movq	%rax, 48(%rsp)
	cmpq	120(%r15), %rbp
	jnb	.L19
	salq	$4, %rbp
	addq	112(%r15), %rbp
	movq	0(%rbp), %r8
	movslq	%ebx, %r15
	leaq	8(%r15), %rdi
	movq	%r8, 40(%rsp)
	movq	8(%rbp), %rbp
	call	malloc@PLT
	leaq	8(%rax), %rdi
	xorl	%esi, %esi
	movq	%r15, %rdx
	movq	%rax, %r13
	call	memset@PLT
	movq	40(%rsp), %r8
	movq	%rax, %rdi
	movq	%r15, 0(%r13)
	addq	%r8, %rbp
	xorl	%esi, %esi
	xorl	%eax, %eax
	testq	%r15, %r15
	je	.L26
	.p2align 4,,10
	.p2align 3
.L20:
	addq	%r14, %rax
	movzbl	%sil, %edx
	cmpq	%r8, %rax
	jb	.L32
	cmpq	%rbp, %rax
	jb	.L23
.L32:
	incl	%esi
	movzbl	%sil, %eax
	movb	$32, 8(%r13,%rdx)
	cmpq	%r15, %rax
	jb	.L20
.L26:
	movq	56(%rsp), %rcx
	movl	32(%rsp), %edx
	movq	stderr(%rip), %rax
	cmpq	%r12, 16(%rcx)
	je	.L53
	subq	$8, %rsp
	.cfi_def_cfa_offset 392
	pushq	%rdi
	.cfi_def_cfa_offset 400
	movl	%ebx, %r9d
	movq	%rax, %rdi
	pushq	%rbx
	.cfi_def_cfa_offset 408
	leaq	.LC7(%rip), %rsi
	xorl	%eax, %eax
	pushq	%r14
	.cfi_def_cfa_offset 416
	movq	48(%rsp), %r8
	movq	56(%rsp), %rcx
	call	fprintf@PLT
	addq	$32, %rsp
	.cfi_def_cfa_offset 384
.L27:
	subq	$32, %r12
	decq	8(%rsp)
	jne	.L28
	movq	56(%rsp), %rax
	movq	80(%rsp), %r11
	movq	104(%rsp), %rbx
	movzbl	(%rax), %edx
.L11:
	andl	$4, %edx
	jne	.L54
	movq	%r11, %rdx
	leaq	gab_vm_container_cb(%rip), %rsi
	movq	%rbx, %rdi
	call	gab_obj_container_create@PLT
	movabsq	$-1125899906842624, %rdx
	orq	%rdx, %rax
	movq	136(%rsp), %rdx
	subq	%fs:40, %rdx
	jne	.L55
	addq	$328, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L54:
	.cfi_restore_state
	xorl	%edi, %edi
	call	exit@PLT
	.p2align 4,,10
	.p2align 3
.L23:
	incl	%esi
	movzbl	%sil, %eax
	movb	$94, 8(%r13,%rdx)
	cmpq	%r15, %rax
	jb	.L20
	jmp	.L26
	.p2align 4,,10
	.p2align 3
.L53:
	pushq	%rdi
	.cfi_def_cfa_offset 392
	leaq	.LC4(%rip), %rsi
	movq	%rax, %rdi
	pushq	%rbx
	.cfi_def_cfa_offset 400
	xorl	%eax, %eax
	pushq	%r14
	.cfi_def_cfa_offset 408
	pushq	%rbx
	.cfi_def_cfa_offset 416
	movq	48(%rsp), %r9
	movq	80(%rsp), %r8
	movq	56(%rsp), %rcx
	call	fprintf@PLT
	addq	$32, %rsp
	.cfi_def_cfa_offset 384
	movq	%r13, %rdi
	call	free@PLT
	movl	92(%rsp), %eax
	leaq	gab_status_names(%rip), %rdx
	movq	(%rdx,%rax,8), %rdx
	movq	stderr(%rip), %rdi
	leaq	.LC5(%rip), %rsi
	xorl	%eax, %eax
	call	fprintf@PLT
	movq	96(%rsp), %rsi
	leaq	384(%rsp), %rax
	movq	stderr(%rip), %rdi
	movq	%rax, 120(%rsp)
	leaq	112(%rsp), %rdx
	leaq	144(%rsp), %rax
	movl	$32, 112(%rsp)
	movl	$48, 116(%rsp)
	movq	%rax, 128(%rsp)
	call	vfprintf@PLT
	movq	stderr(%rip), %rcx
	movl	$5, %edx
	movl	$1, %esi
	leaq	.LC6(%rip), %rdi
	call	fwrite@PLT
	jmp	.L27
.L19:
	leaq	__PRETTY_FUNCTION__.5(%rip), %rcx
	movl	$76, %edx
	leaq	.LC0(%rip), %rsi
	leaq	.LC1(%rip), %rdi
	call	__assert_fail@PLT
.L52:
	leaq	__PRETTY_FUNCTION__.6(%rip), %rcx
	movl	$76, %edx
	leaq	.LC0(%rip), %rsi
	leaq	.LC1(%rip), %rdi
	call	__assert_fail@PLT
.L55:
	call	__stack_chk_fail@PLT
	.p2align 4,,10
	.p2align 3
.L50:
	leaq	__PRETTY_FUNCTION__.8(%rip), %rcx
	movl	$76, %edx
	leaq	.LC0(%rip), %rsi
	leaq	.LC1(%rip), %rdi
	call	__assert_fail@PLT
.L51:
	leaq	__PRETTY_FUNCTION__.7(%rip), %rcx
	movl	$76, %edx
	leaq	.LC0(%rip), %rsi
	leaq	.LC1(%rip), %rdi
	call	__assert_fail@PLT
	.cfi_endproc
.LFE229:
	.size	vm_error, .-vm_error
	.p2align 4
	.globl	gab_vm_panic
	.type	gab_vm_panic, @function
gab_vm_panic:
.LFB230:
	.cfi_startproc
	movq	%rdx, %rcx
	xorl	%eax, %eax
	movl	$1, %edx
	jmp	vm_error
	.cfi_endproc
.LFE230:
	.size	gab_vm_panic, .-gab_vm_panic
	.p2align 4
	.globl	gab_vm_create
	.type	gab_vm_create, @function
gab_vm_create:
.LFB231:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	leaq	32(%rdi), %rcx
	movl	%esi, %ebp
	pushq	%rbx
	.cfi_def_cfa_offset 24
	.cfi_offset 3, -24
	movl	$1048576, %edx
	movq	%rdi, %rbx
	subq	$8, %rsp
	.cfi_def_cfa_offset 32
	movq	$0, 8(%rdi)
	xorl	%esi, %esi
	movq	%rcx, %rdi
	call	memset@PLT
	movq	%rax, %rcx
	leaq	1048608(%rbx), %rax
	movq	%rax, 16(%rbx)
	movq	%rcx, 24(%rbx)
	movb	%bpl, (%rbx)
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE231:
	.size	gab_vm_create, .-gab_vm_create
	.p2align 4
	.globl	gab_vm_stack_dump
	.type	gab_vm_stack_dump, @function
gab_vm_stack_dump:
.LFB232:
	.cfi_startproc
	ret
	.cfi_endproc
.LFE232:
	.size	gab_vm_stack_dump, .-gab_vm_stack_dump
	.section	.rodata.str1.8
	.align 8
.LC8:
	.string	"\033[32m %03lu\033[0m closure:\033[36m%-20.*s\033[0m%d locals, %d upvalues\n"
	.section	.rodata.str1.1
.LC9:
	.string	"\033[35m%4i \033[0m"
.LC10:
	.string	"\033[33m%4i \033[0m"
	.text
	.p2align 4
	.globl	gab_vm_frame_dump
	.type	gab_vm_frame_dump, @function
gab_vm_frame_dump:
.LFB233:
	.cfi_startproc
	pushq	%r14
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -16
	addq	$1048608, %rsi
	pushq	%r13
	.cfi_def_cfa_offset 24
	.cfi_offset 13, -24
	pushq	%r12
	.cfi_def_cfa_offset 32
	.cfi_offset 12, -32
	pushq	%rbp
	.cfi_def_cfa_offset 40
	.cfi_offset 6, -40
	pushq	%rbx
	.cfi_def_cfa_offset 48
	.cfi_offset 3, -48
	movq	-1048592(%rsi), %r12
	movq	%r12, %rbx
	subq	%rsi, %rbx
	sarq	$5, %rbx
	cmpq	%rbx, %rdx
	jnb	.L75
	movq	%rdx, %rax
	salq	$5, %rax
	subq	%rax, %r12
	movq	(%r12), %rax
	movq	%rdx, %rbp
	movq	32(%rax), %rax
	movq	32(%rax), %rax
	movzwl	138(%rax), %edx
	cmpq	24(%rax), %rdx
	jnb	.L78
	movq	16(%rax), %rax
	movabsq	$1125899906842623, %rdi
	andq	(%rax,%rdx,8), %rdi
	call	gab_obj_string_ref@PLT
	movq	%rax, %rcx
	movq	(%r12), %rax
	subq	%rbp, %rbx
	movq	32(%rax), %rax
	movq	%rbx, %rsi
	movzbl	25(%rax), %r9d
	movzbl	40(%rax), %r8d
	leaq	.LC8(%rip), %rdi
	xorl	%eax, %eax
	call	printf@PLT
	movq	(%r12), %rax
	movq	32(%rax), %rax
	movzbl	41(%rax), %edx
	leal	-1(%rdx), %ebx
	testl	%edx, %edx
	je	.L75
	movslq	%ebx, %rbp
	salq	$3, %rbp
	leaq	.LC10(%rip), %r14
	leaq	.LC9(%rip), %r13
	jmp	.L66
	.p2align 4,,10
	.p2align 3
.L79:
	movq	(%r12), %rax
	movq	32(%rax), %rax
.L66:
	movzbl	40(%rax), %eax
	movl	%ebx, %esi
	movq	%r13, %rdi
	cmpl	%ebx, %eax
	jg	.L77
	movq	%r14, %rdi
.L77:
	xorl	%eax, %eax
	call	printf@PLT
	movq	16(%r12), %rax
	movq	stdout(%rip), %rdi
	movq	(%rax,%rbp), %rsi
	decl	%ebx
	call	gab_val_dump@PLT
	movl	$10, %edi
	call	putchar@PLT
	subq	$8, %rbp
	cmpl	$-1, %ebx
	jne	.L79
.L75:
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 40
	popq	%rbp
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r13
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	ret
.L78:
	.cfi_restore_state
	call	v_gab_constant_val_at.part.0
	.cfi_endproc
.LFE233:
	.size	gab_vm_frame_dump, .-gab_vm_frame_dump
	.section	.rodata.str1.8
	.align 8
.LC11:
	.string	"Frame at %s:\n-----------------\n"
	.section	.rodata.str1.1
.LC12:
	.string	"|%lu|"
.LC13:
	.string	"---------------"
	.text
	.p2align 4
	.globl	dump_frame
	.type	dump_frame, @function
dump_frame:
.LFB234:
	.cfi_startproc
	pushq	%r13
	.cfi_def_cfa_offset 16
	.cfi_offset 13, -16
	xorl	%eax, %eax
	pushq	%r12
	.cfi_def_cfa_offset 24
	.cfi_offset 12, -24
	movq	%rsi, %r12
	movq	%rdx, %rsi
	pushq	%rbp
	.cfi_def_cfa_offset 32
	.cfi_offset 6, -32
	movq	%rdi, %rbp
	leaq	.LC11(%rip), %rdi
	pushq	%rbx
	.cfi_def_cfa_offset 40
	.cfi_offset 3, -40
	leaq	-8(%r12), %rbx
	subq	$8, %rsp
	.cfi_def_cfa_offset 48
	call	printf@PLT
	cmpq	%rbp, %rbx
	jb	.L81
	movq	%r12, %rax
	subq	%rbp, %rax
	subq	$8, %rax
	andq	$-8, %rax
	negq	%rax
	leaq	-16(%r12,%rax), %r13
	leaq	.LC12(%rip), %r12
	.p2align 4,,10
	.p2align 3
.L82:
	movq	%rbx, %rsi
	subq	%rbp, %rsi
	sarq	$3, %rsi
	movq	%r12, %rdi
	xorl	%eax, %eax
	subq	$8, %rbx
	call	printf@PLT
	movq	stdout(%rip), %rdi
	movq	8(%rbx), %rsi
	call	gab_val_dump@PLT
	movl	$10, %edi
	call	putchar@PLT
	cmpq	%r13, %rbx
	jne	.L82
.L81:
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%rbp
	.cfi_def_cfa_offset 24
	popq	%r12
	.cfi_def_cfa_offset 16
	leaq	.LC13(%rip), %rdi
	popq	%r13
	.cfi_def_cfa_offset 8
	jmp	puts@PLT
	.cfi_endproc
.LFE234:
	.size	dump_frame, .-dump_frame
	.section	.rodata.str1.8
	.align 8
.LC14:
	.string	"Stack at %s:\n-----------------\n"
	.text
	.p2align 4
	.globl	dump_stack
	.type	dump_stack, @function
dump_stack:
.LFB235:
	.cfi_startproc
	pushq	%r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	xorl	%eax, %eax
	pushq	%rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	movq	%rdi, %rbp
	addq	$32, %rbp
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	movq	%rsi, %rbx
	leaq	.LC14(%rip), %rdi
	movq	%rdx, %rsi
	subq	$8, %rbx
	call	printf@PLT
	cmpq	%rbp, %rbx
	jb	.L86
	leaq	.LC12(%rip), %r12
	.p2align 4,,10
	.p2align 3
.L87:
	movq	%rbx, %rsi
	subq	%rbp, %rsi
	sarq	$3, %rsi
	movq	%r12, %rdi
	xorl	%eax, %eax
	subq	$8, %rbx
	call	printf@PLT
	movq	stdout(%rip), %rdi
	movq	8(%rbx), %rsi
	call	gab_val_dump@PLT
	movl	$10, %edi
	call	putchar@PLT
	cmpq	%rbp, %rbx
	jnb	.L87
.L86:
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%rbp
	.cfi_def_cfa_offset 16
	leaq	.LC13(%rip), %rdi
	popq	%r12
	.cfi_def_cfa_offset 8
	jmp	puts@PLT
	.cfi_endproc
.LFE235:
	.size	dump_stack, .-dump_stack
	.section	.rodata.str1.1
.LC16:
	.string	""
.LC17:
	.string	"Could not send %V to %V"
.LC18:
	.string	"Found %V"
	.section	.rodata.str1.8
	.align 8
.LC20:
	.string	"Tried to store property %V on %V"
	.section	.rodata.str1.1
.LC21:
	.string	"i < UINT16_MAX"
	.section	.rodata.str1.8
	.align 8
.LC22:
	.string	"Tried to load property %V on %V"
	.section	.rodata.str1.1
.LC23:
	.string	"%V has no property %V"
	.text
	.p2align 4
	.globl	gab_vm_run
	.type	gab_vm_run, @function
gab_vm_run:
.LFB245:
	.cfi_startproc
	leaq	8(%rsp), %r10
	.cfi_def_cfa 10, 0
	andq	$-32, %rsp
	pushq	-8(%r10)
	pushq	%rbp
	movq	%rsp, %rbp
	.cfi_escape 0x10,0x6,0x2,0x76,0
	pushq	%r15
	.cfi_escape 0x10,0xf,0x2,0x76,0x78
	leaq	-1065040(%rbp), %r15
	pushq	%r14
	.cfi_escape 0x10,0xe,0x2,0x76,0x70
	movl	%edx, %r14d
	movl	$1048576, %edx
	pushq	%r13
	.cfi_escape 0x10,0xd,0x2,0x76,0x68
	movq	%rsi, %r13
	xorl	%esi, %esi
	pushq	%r12
	.cfi_escape 0x10,0xc,0x2,0x76,0x60
	movq	%r8, %r12
	pushq	%r10
	.cfi_escape 0xf,0x3,0x76,0x58,0x6
	pushq	%rbx
	.cfi_escape 0x10,0x3,0x2,0x76,0x50
	movl	%ecx, %ebx
	subq	$1065184, %rsp
	movq	%rdi, -1065112(%rbp)
	movq	%r15, %rdi
	movq	%fs:40, %rax
	movq	%rax, -56(%rbp)
	xorl	%eax, %eax
	movq	$0, -1065064(%rbp)
	movq	%r15, -1065120(%rbp)
	call	memset@PLT
	leaq	-16464(%rbp), %rax
	vmovq	%rax, %xmm3
	movq	%rax, -1065128(%rbp)
	movzwl	136(%r13), %eax
	vpinsrq	$1, %r15, %xmm3, %xmm0
	movb	%r14b, -1065072(%rbp)
	vmovdqa	%xmm0, -1065056(%rbp)
	cmpq	24(%r13), %rax
	jnb	.L109
	movq	16(%r13), %rdx
	movabsq	$1125899906842623, %rcx
	andq	(%rdx,%rax,8), %rcx
	movq	32(%rcx), %r8
	movq	-1065120(%rbp), %rax
	movabsq	$-1125899906842624, %rdx
	movq	%rax, -16448(%rbp)
	movq	32(%r8), %rax
	orq	%rcx, %rdx
	movq	40(%rax), %rdi
	leaq	-1065032(%rbp), %rax
	movq	%rcx, -16464(%rbp)
	movq	%rax, -1065048(%rbp)
	movq	%rdx, -1065040(%rbp)
	testb	%bl, %bl
	je	.L98
	leal	-1(%rbx), %edx
	cmpb	$2, %dl
	jbe	.L628
	movl	%ebx, %esi
	shrb	$2, %sil
	movzbl	%sil, %esi
	salq	$5, %rsi
	xorl	%edx, %edx
	.p2align 4,,10
	.p2align 3
.L100:
	vmovdqu	(%r12,%rdx), %ymm0
	vmovdqu	%ymm0, (%rax,%rdx)
	addq	$32, %rdx
	cmpq	%rdx, %rsi
	jne	.L100
	movl	%ebx, %edx
	movl	%ebx, %esi
	andl	$252, %edx
	andl	$-4, %esi
	leaq	(%rax,%rdx,8), %r10
	testb	$3, %bl
	je	.L1049
.L99:
	movq	(%r12,%rdx,8), %rdx
	leal	1(%rsi), %r9d
	movq	%rdx, (%r10)
	cmpb	%bl, %r9b
	jnb	.L101
	movzbl	%r9b, %r9d
	movq	(%r12,%r9,8), %rdx
	addl	$2, %esi
	movq	%rdx, 8(%r10)
	cmpb	%bl, %sil
	jnb	.L101
	movzbl	%sil, %esi
	movq	(%r12,%rsi,8), %rdx
	movq	%rdx, 16(%r10)
.L101:
	movzbl	%bl, %ebx
	salq	$3, %rbx
	addq	%rbx, %rax
	movq	%rax, -1065048(%rbp)
	movq	%rdx, -1065040(%rbp,%rbx)
	movq	%rdi, -16456(%rbp)
	movq	-1065120(%rbp), %rbx
	movq	%rax, %r9
	movzbl	24(%r8), %esi
	subq	%rbx, %r9
	movl	%esi, %edx
	notl	%edx
	addb	41(%r8), %dl
	movzbl	%dl, %edx
	sarq	$3, %r9
	addq	%r9, %rdx
	leaq	-1065072(%rbp), %r14
	cmpq	$131071, %rdx
	jg	.L102
.L626:
	leaq	-16432(%rbp), %rdx
	movq	%rdx, -1065056(%rbp)
	movzbl	%sil, %edx
	movq	%rcx, -16432(%rbp)
	salq	$3, %rdx
	movq	%rax, %rcx
	subq	%rdx, %rcx
	leaq	-8(%rcx), %rdx
	movq	%rdi, -16424(%rbp)
	movb	$1, -16408(%rbp)
	movq	%rdx, -16416(%rbp)
	leal	1(%rsi), %r9d
	movzbl	40(%r8), %r8d
	cmpb	%r8b, %r9b
	jnb	.L209
	movl	%r8d, %edx
	subl	%esi, %edx
	leal	-2(%rdx), %esi
	leal	-1(%rdx), %r10d
	cmpb	$2, %sil
	jbe	.L629
	subl	$5, %edx
	movabsq	$9222246136947933185, %r11
	shrb	$2, %dl
	vmovq	%r11, %xmm6
	leal	1(%rdx), %ecx
	vpbroadcastq	%xmm6, %ymm0
	xorl	%edx, %edx
	.p2align 4,,10
	.p2align 3
.L105:
	movq	%rdx, %r11
	salq	$5, %r11
	incq	%rdx
	vmovdqu	%ymm0, (%rax,%r11)
	cmpb	%cl, %dl
	jb	.L105
	sall	$2, %ecx
	cmpb	%r10b, %cl
	je	.L106
	addl	%ecx, %r9d
	movzbl	%cl, %ecx
	leaq	(%rax,%rcx,8), %rcx
.L104:
	movabsq	$9222246136947933185, %rdx
	leal	1(%r9), %r10d
	movq	%rdx, (%rcx)
	cmpb	%r8b, %r10b
	jnb	.L106
	addl	$2, %r9d
	movq	%rdx, 8(%rcx)
	cmpb	%r8b, %r9b
	jnb	.L106
	movq	%rdx, 16(%rcx)
.L106:
	movzbl	%sil, %esi
	leaq	8(%rax,%rsi,8), %rdx
	movabsq	$9222246136947933185, %rcx
	movq	%rdx, -1065048(%rbp)
	movq	%rcx, (%rax,%rsi,8)
.L209:
	movzbl	(%rdi), %eax
	leaq	1(%rdi), %r12
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	jmp	*(%rdi,%rax,8)
.L536:
	.p2align 4,,10
	.p2align 3
.L537:
	movq	-1065056(%rbp), %rax
	movzbl	%bl, %ebx
	movq	(%rax), %rdx
	movq	-1065048(%rbp), %rax
	subl	$61, %ebx
	leaq	8(%rax), %rcx
	movslq	%ebx, %rbx
	movq	%rcx, -1065048(%rbp)
	movq	40(%rdx,%rbx,8), %rdx
.L1010:
	movq	%rdx, (%rax)
.L1011:
	movzbl	(%r12), %eax
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	movq	(%rdi,%rax,8), %rax
	incq	%r12
	jmp	*%rax
.L527:
	.p2align 4,,10
	.p2align 3
.L528:
	movq	-1065048(%rbp), %rax
	movzbl	%bl, %ebx
	movq	-8(%rax), %rdx
	movq	-1065056(%rbp), %rax
	subl	$43, %ebx
	movq	(%rax), %rax
	movslq	%ebx, %rbx
	movabsq	$1125899906842623, %rdi
	andq	40(%rax,%rbx,8), %rdi
	movq	24(%rdi), %rax
	jmp	.L1010
.L509:
	.p2align 4,,10
	.p2align 3
.L510:
	movq	-1065048(%rbp), %rax
	movzbl	%bl, %ebx
	leaq	-8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	-8(%rax), %rdx
	movq	-1065056(%rbp), %rax
	movq	16(%rax), %rax
	movq	%rdx, -200(%rax,%rbx,8)
	jmp	.L1011
.L500:
	.p2align 4,,10
	.p2align 3
.L501:
	movq	-1065048(%rbp), %rax
	movzbl	%bl, %ebx
	movq	-8(%rax), %rdx
	movq	-1065056(%rbp), %rax
	movq	16(%rax), %rax
	movq	%rdx, -128(%rax,%rbx,8)
	jmp	.L1011
.L491:
	.p2align 4,,10
	.p2align 3
.L492:
	movq	-1065056(%rbp), %rax
	movzbl	%bl, %ebx
	movq	16(%rax), %rdx
	movq	-1065048(%rbp), %rax
	movq	-272(%rdx,%rbx,8), %rdx
	leaq	8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	jmp	.L1010
.L518:
	.p2align 4,,10
	.p2align 3
.L519:
	movq	-1065056(%rbp), %rax
	movzbl	%bl, %ebx
	movq	(%rax), %rax
	subl	$52, %ebx
	movslq	%ebx, %rbx
	movabsq	$1125899906842623, %rdi
	andq	40(%rax,%rbx,8), %rdi
	movq	-1065048(%rbp), %rax
	movq	24(%rdi), %rdx
	leaq	8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	(%rdx), %rdx
	jmp	.L1010
.L616:
	movzbl	(%r12), %eax
	movl	%eax, %r15d
	shrb	%r15b
	testb	$1, %al
	je	.L618
	movq	-1065048(%rbp), %rax
	addb	(%rax), %r15b
.L618:
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %r14
	movzbl	%r15b, %edx
	movq	%r14, %rsi
	vzeroupper
	movzbl	%r15b, %ebx
	call	gab_obj_shape_create_tuple@PLT
	movq	-1065048(%rbp), %rcx
	leaq	0(,%rbx,8), %rdx
	movq	-1065112(%rbp), %rdi
	subq	%rdx, %rcx
	movq	%rax, %rsi
	movq	%rdx, %r13
	movl	$1, %edx
	call	gab_obj_record_create@PLT
	movq	%rax, %rbx
	movq	-1065112(%rbp), %rax
	movzbl	%r15b, %ecx
	leaq	88(%rax), %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	leaq	40(%rbx), %r8
	movq	%rax, %r15
	movq	%rdx, -1065136(%rbp)
	call	gab_gc_iref_many@PLT
	movq	-1065048(%rbp), %rax
	negq	%r13
	addq	%r13, %rax
	leaq	8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movabsq	$-1125899906842624, %rcx
	orq	%rbx, %rcx
	movq	%rcx, (%rax)
	movq	-1065136(%rbp), %rdx
	movq	%r14, %rsi
	movq	%r15, %rdi
	call	gab_gc_dref@PLT
	.p2align 4,,10
	.p2align 3
.L1009:
	movzbl	1(%r12), %eax
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	movq	(%rdi,%rax,8), %rax
	addq	$2, %r12
	jmp	*%rax
.L615:
	movzbl	(%r12), %r15d
	movq	-1065048(%rbp), %r8
	leaq	(%r15,%r15), %rbx
	andl	$510, %ebx
	leaq	0(,%rbx,8), %rax
	movq	-1065112(%rbp), %rdi
	leaq	1(%r12), %r13
	leaq	-1065072(%rbp), %r14
	movq	%rax, %r12
	subq	%rax, %r8
	movq	%r14, %rsi
	movq	%r15, %rdx
	negq	%r12
	movl	$2, %ecx
	vzeroupper
	call	gab_obj_shape_create@PLT
	movq	%rax, %rsi
.L614:
	movq	-1065048(%rbp), %rdx
	movl	$1, %eax
	subq	%rbx, %rax
	movq	-1065112(%rbp), %rdi
	leaq	(%rdx,%rax,8), %rcx
	movl	$2, %edx
	call	gab_obj_record_create@PLT
	movq	%rax, %rbx
	movq	-1065112(%rbp), %rax
	movq	%r15, %rcx
	leaq	88(%rax), %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	leaq	40(%rbx), %r8
	movq	%rax, %r15
	movq	%rdx, -1065136(%rbp)
	call	gab_gc_iref_many@PLT
	addq	-1065048(%rbp), %r12
	leaq	8(%r12), %rax
	movq	%rax, -1065048(%rbp)
	movq	%rbx, %rcx
	movabsq	$-1125899906842624, %rax
	orq	%rax, %rcx
	movq	%rcx, (%r12)
	movq	-1065136(%rbp), %rdx
	movq	%r14, %rsi
	movq	%r15, %rdi
	call	gab_gc_dref@PLT
.L1008:
	movzbl	0(%r13), %eax
	leaq	dispatch_table.4(%rip), %rdi
	leaq	1(%r13), %r12
	movq	%rax, %rbx
	jmp	*(%rdi,%rax,8)
.L613:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movzbl	2(%r12), %r15d
	movq	16(%rdx), %rdx
	leaq	(%r15,%r15), %rbx
	movq	(%rdx,%rax,8), %r9
	andl	$510, %ebx
	movq	%r9, -1065136(%rbp)
	leaq	0(,%rbx,8), %rax
	movq	-1065048(%rbp), %r8
	movq	-1065112(%rbp), %rdi
	leaq	3(%r12), %r13
	leaq	-1065072(%rbp), %r14
	movq	%rax, %r12
	subq	%rax, %r8
	movq	%r14, %rsi
	movq	%r15, %rdx
	negq	%r12
	movl	$2, %ecx
	vzeroupper
	call	gab_obj_shape_create@PLT
	movq	-1065136(%rbp), %r9
	movq	%rax, %rsi
	movq	%r9, 24(%rax)
	jmp	.L614
.L569:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rcx
	movzwl	%ax, %eax
	movq	32(%rcx), %rcx
	movq	32(%rcx), %rcx
	movq	24(%rcx), %rdi
	cmpq	%rdi, %rax
	jnb	.L1005
	movq	16(%rcx), %rcx
	movabsq	$1125899906842623, %rbx
	movq	(%rcx,%rax,8), %rsi
	movbew	2(%r12), %ax
	andq	%rbx, %rsi
	movzwl	%ax, %eax
	leaq	4(%r12), %r13
	cmpq	%rdi, %rax
	jnb	.L1005
	andq	(%rcx,%rax,8), %rbx
	movq	-1065048(%rbp), %rax
	cmpq	$0, 48(%rbx)
	leaq	-8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	-8(%rax), %r15
	jne	.L1050
.L570:
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_obj_block_create@PLT
	movzbl	24(%rax), %ecx
	movq	%rax, %r12
	testb	%cl, %cl
	je	.L706
	xorl	%edx, %edx
	movq	%rbx, -1065136(%rbp)
	movq	%r15, -1065144(%rbp)
	movq	%rdx, %rbx
	jmp	.L589
	.p2align 4,,10
	.p2align 3
.L1053:
	movq	16(%rsi), %rsi
	andl	$2, %edi
	leaq	(%rsi,%rax,8), %rsi
	jne	.L1051
	movq	(%rsi), %rax
.L588:
	movq	%rax, 40(%r12,%rbx,8)
	incq	%rbx
	movzbl	%cl, %eax
	cmpl	%ebx, %eax
	jle	.L1052
.L589:
	movzbl	0(%r13), %edi
	movq	-1065056(%rbp), %rsi
	addq	$2, %r13
	movzbl	-1(%r13), %eax
	testb	$4, %dil
	jne	.L1053
	movq	(%rsi), %rsi
	movq	40(%rsi,%rax,8), %rax
	movq	%rax, 40(%r12,%rbx,8)
	incq	%rbx
	movzbl	%cl, %eax
	cmpl	%ebx, %eax
	jg	.L589
.L1052:
	movq	-1065136(%rbp), %rbx
	movq	-1065144(%rbp), %r15
.L577:
	movq	-1065112(%rbp), %rax
	leaq	-1065072(%rbp), %r14
	leaq	88(%rax), %rdx
	movq	%rax, %rdi
	leaq	40(%r12), %r8
	movq	%r14, %rsi
	movq	%rdx, -1065136(%rbp)
	call	gab_gc_iref_many@PLT
	movq	-1065112(%rbp), %rdi
	movq	-1065136(%rbp), %rdx
	movq	%r15, %rcx
	movq	%r14, %rsi
	call	gab_gc_iref@PLT
	movabsq	$-1125899906842624, %rax
	orq	%rax, %r12
	movq	48(%rbx), %rax
	movq	56(%rbx), %rdi
	incq	%rax
	js	.L591
	vxorpd	%xmm5, %xmm5, %xmm5
	vcvtsi2sdq	%rax, %xmm5, %xmm1
	testq	%rdi, %rdi
	js	.L593
.L1108:
	vxorpd	%xmm4, %xmm4, %xmm4
	vcvtsi2sdq	%rdi, %xmm4, %xmm0
.L594:
	vmulsd	.LC25(%rip), %xmm0, %xmm0
	vcomisd	%xmm0, %xmm1
	ja	.L590
	movq	64(%rbx), %r8
	leaq	-1(%rdi), %r14
.L595:
	movq	%r15, %rax
	andq	%r14, %rax
	movq	$-1, %rsi
.L612:
	leaq	(%rax,%rax,2), %rdx
	leaq	(%r8,%rdx,8), %rdx
	movl	16(%rdx), %ecx
	movq	(%rdx), %rdi
	cmpl	$1, %ecx
	je	.L606
	cmpl	$2, %ecx
	je	.L607
	testl	%ecx, %ecx
	je	.L1054
.L609:
	incq	%rax
	andq	%r14, %rax
	jmp	.L612
.L555:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rcx
	movabsq	$1125899906842623, %rdx
	andq	(%rcx,%rax,8), %rdx
	movq	-1065112(%rbp), %rdi
	movq	%rdx, %r14
	movq	%rdx, %rsi
	vzeroupper
	call	gab_obj_block_create@PLT
	movzbl	25(%r14), %ecx
	movq	%rax, %r15
	testb	%cl, %cl
	je	.L704
	movq	%r12, -1065136(%rbp)
	xorl	%r13d, %r13d
	jmp	.L568
	.p2align 4,,10
	.p2align 3
.L1057:
	movq	16(%rsi), %rsi
	andl	$2, %edx
	leaq	(%rsi,%rax,8), %rsi
	jne	.L1055
	movq	(%rsi), %rax
.L567:
	movq	%rax, 40(%r15,%r13,8)
	incq	%r13
	movzbl	%cl, %eax
	cmpl	%r13d, %eax
	jle	.L1056
.L568:
	movzbl	43(%r14,%r13,2), %edx
	movzbl	44(%r14,%r13,2), %eax
	movq	-1065056(%rbp), %rsi
	testb	$4, %dl
	jne	.L1057
	movq	(%rsi), %rdx
	movq	40(%rdx,%rax,8), %rax
	movq	%rax, 40(%r15,%r13,8)
	incq	%r13
	movzbl	%cl, %eax
	cmpl	%r13d, %eax
	jg	.L568
.L1056:
	movq	-1065136(%rbp), %r12
.L556:
	movq	-1065112(%rbp), %r13
	leaq	-1065072(%rbp), %r14
	leaq	88(%r13), %rbx
	movq	%rbx, %rdx
	movq	%r14, %rsi
	movq	%r13, %rdi
	leaq	40(%r15), %r8
	call	gab_gc_iref_many@PLT
	movq	-1065048(%rbp), %rax
	movq	%r15, %rcx
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movabsq	$-1125899906842624, %rdx
	orq	%rdx, %rcx
	movq	%rcx, (%rax)
	movq	%rbx, %rdx
	movq	%r14, %rsi
	movq	%r13, %rdi
	call	gab_gc_dref@PLT
	jmp	.L1013
.L553:
	movbew	(%r12), %ax
	movzwl	%ax, %eax
	leaq	2(%r12,%rax), %rax
	.p2align 4,,10
	.p2align 3
.L1012:
	leaq	1(%rax), %r12
	movzbl	(%rax), %eax
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	jmp	*(%rdi,%rax,8)
.L552:
	movq	-1065048(%rbp), %rdx
	movabsq	$-9222246136947933185, %rax
	addq	-8(%rdx), %rax
	cmpq	$1, %rax
	setbe	%al
.L1021:
	movbew	(%r12), %dx
	movzbl	%al, %eax
	movzwl	%dx, %edx
	imull	%edx, %eax
	addl	$2, %eax
	cltq
	addq	%r12, %rax
	jmp	.L1012
.L554:
	movbew	(%r12), %ax
	movq	%r12, %rdx
	movzwl	%ax, %eax
	subq	%rax, %rdx
	movzbl	2(%rdx), %eax
	leaq	dispatch_table.4(%rip), %rdi
	leaq	3(%rdx), %r12
	movq	%rax, %rbx
	jmp	*(%rdi,%rax,8)
.L551:
	movq	-1065048(%rbp), %rdx
	movabsq	$-9222246136947933185, %rax
	addq	-8(%rdx), %rax
	cmpq	$1, %rax
	seta	%al
	jmp	.L1021
.L550:
	movq	-1065048(%rbp), %rcx
	movbew	(%r12), %dx
	leaq	-8(%rcx), %rax
	movq	%rax, -1065048(%rbp)
	movabsq	$-9222246136947933185, %rax
	addq	-8(%rcx), %rax
	cmpq	$1, %rax
	movzwl	%dx, %edx
	setbe	%al
.L1031:
	movzbl	%al, %eax
	imull	%edx, %eax
	addl	$2, %eax
	cltq
	addq	%r12, %rax
	jmp	.L1012
.L549:
	movq	-1065048(%rbp), %rcx
	movbew	(%r12), %dx
	leaq	-8(%rcx), %rax
	movq	%rax, -1065048(%rbp)
	movabsq	$-9222246136947933185, %rax
	addq	-8(%rcx), %rax
	cmpq	$1, %rax
	movzwl	%dx, %edx
	seta	%al
	jmp	.L1031
.L545:
	movq	-1065056(%rbp), %rax
	movzbl	(%r12), %edx
	movq	16(%rax), %rax
	movq	-1065064(%rbp), %rbx
	leaq	(%rax,%rdx,8), %r13
	movq	-1065112(%rbp), %rax
	leaq	88(%rax), %r15
	testq	%rbx, %rbx
	je	.L1009
	movq	%r12, -1065136(%rbp)
	movq	%rax, %r14
	jmp	.L546
	.p2align 4,,10
	.p2align 3
.L548:
	movq	(%rax), %rcx
	leaq	32(%rbx), %rax
	movq	%rax, 24(%rbx)
	movq	40(%rbx), %rax
	movq	%rcx, 32(%rbx)
	movq	%rax, -1065064(%rbp)
	leaq	-1065072(%rbp), %r12
	movq	%r15, %rdx
	movq	%r12, %rsi
	movq	%r14, %rdi
	vzeroupper
	call	gab_gc_iref@PLT
	movabsq	$-1125899906842624, %rcx
	orq	%rbx, %rcx
	movq	%r15, %rdx
	movq	%r12, %rsi
	movq	%r14, %rdi
	call	gab_gc_dref@PLT
	movq	-1065064(%rbp), %rbx
	testq	%rbx, %rbx
	je	.L980
.L546:
	movq	24(%rbx), %rax
	cmpq	%r13, %rax
	jnb	.L548
.L980:
	movq	-1065136(%rbp), %r12
	jmp	.L1009
.L544:
	movq	-1065048(%rbp), %rax
	leaq	-8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
.L1025:
	movq	-8(%rax), %rdx
	movq	-1065056(%rbp), %rax
	movzbl	(%r12), %esi
	movq	(%rax), %rcx
	movabsq	$1125899906842623, %rax
	andq	40(%rcx,%rsi,8), %rax
	movq	24(%rax), %rax
	movq	%rdx, (%rax)
	jmp	.L1009
.L543:
	movq	-1065048(%rbp), %rax
	jmp	.L1025
.L542:
	movq	-1065056(%rbp), %rax
	movq	-1065048(%rbp), %rdx
	movq	(%rax), %rcx
	movzbl	(%r12), %eax
	leaq	8(%rdx), %rsi
	movq	40(%rcx,%rax,8), %rax
	movq	%rsi, -1065048(%rbp)
	movq	%rax, (%rdx)
	jmp	.L1009
.L541:
	movq	-1065056(%rbp), %rax
	movzbl	(%r12), %ecx
	movq	(%rax), %rdx
	movabsq	$1125899906842623, %rax
	andq	40(%rdx,%rcx,8), %rax
	movq	24(%rax), %rdx
	movq	-1065048(%rbp), %rax
	movq	(%rdx), %rdx
	leaq	8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rdx, (%rax)
	jmp	.L1009
.L540:
	movq	-1065048(%rbp), %rax
	leaq	-8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
.L1026:
	movq	-8(%rax), %rcx
	movq	-1065056(%rbp), %rax
	movzbl	(%r12), %edx
	movq	16(%rax), %rax
	movq	%rcx, (%rax,%rdx,8)
	jmp	.L1009
.L539:
	movq	-1065048(%rbp), %rax
	jmp	.L1026
.L538:
	movq	-1065056(%rbp), %rax
	movzbl	(%r12), %ecx
	movq	16(%rax), %rdx
	movq	-1065048(%rbp), %rax
	movq	(%rdx,%rcx,8), %rdx
	leaq	8(%rax), %rsi
	movq	%rsi, -1065048(%rbp)
	movq	%rdx, (%rax)
	jmp	.L1009
.L535:
	jmp	.L537
.L534:
	jmp	.L537
.L533:
	jmp	.L537
.L532:
	jmp	.L537
.L531:
	jmp	.L537
.L530:
	jmp	.L537
.L529:
	jmp	.L537
.L526:
	jmp	.L528
.L525:
	jmp	.L528
.L524:
	jmp	.L528
.L523:
	jmp	.L528
.L522:
	jmp	.L528
.L521:
	jmp	.L528
.L520:
	jmp	.L528
.L517:
	jmp	.L519
.L516:
	jmp	.L519
.L515:
	jmp	.L519
.L514:
	jmp	.L519
.L513:
	jmp	.L519
.L512:
	jmp	.L519
.L511:
	jmp	.L519
.L508:
	jmp	.L510
.L507:
	jmp	.L510
.L506:
	jmp	.L510
.L505:
	jmp	.L510
.L504:
	jmp	.L510
.L503:
	jmp	.L510
.L502:
	jmp	.L510
.L499:
	jmp	.L501
.L498:
	jmp	.L501
.L497:
	jmp	.L501
.L496:
	jmp	.L501
.L495:
	jmp	.L501
.L494:
	jmp	.L501
.L493:
	jmp	.L501
.L490:
	jmp	.L492
.L489:
	jmp	.L492
.L488:
	jmp	.L492
.L487:
	jmp	.L492
.L486:
	jmp	.L492
.L485:
	jmp	.L492
.L484:
	jmp	.L492
.L483:
	movzbl	(%r12), %eax
	salq	$3, %rax
	subq	%rax, -1065048(%rbp)
	jmp	.L1009
.L482:
	subq	$8, -1065048(%rbp)
	jmp	.L1011
.L479:
	movq	-1065048(%rbp), %rax
	movq	-16(%rax), %rdi
	cmpq	%rdi, -8(%rax)
	je	.L1058
	movabsq	$9222246136947933186, %rdi
	movq	%rdi, -8(%rax)
	jmp	.L1011
.L473:
	movq	-1065048(%rbp), %rbx
	movabsq	$9222246136947933184, %rdx
	movq	-8(%rbx), %rax
	andn	%rdx, %rax, %rdx
	jne	.L702
	movabsq	$9222246136947933185, %rdx
	cmpq	%rdx, %rax
	je	.L475
	addq	$3, %rdx
	cmpq	%rdx, %rax
	je	.L475
	movq	%rax, %rcx
	orq	$1, %rcx
	decq	%rdx
	cmpq	%rdx, %rcx
	je	.L703
	movabsq	$-1125899906842624, %rdx
	andn	%rdx, %rax, %rdx
	je	.L1059
.L476:
	movl	-1065200(%rbp), %edi
	cmpl	$9, %edi
	je	.L477
	jbe	.L1060
	cmpl	$10, %edi
	je	.L475
	leal	-13(%rdi), %edx
	cmpl	$1, %edx
	ja	.L474
	jmp	.L475
.L471:
	movq	-1065048(%rbp), %rdx
	movabsq	$-9222246136947933185, %rax
	addq	-8(%rdx), %rax
	cmpq	$2, %rax
	movabsq	$9222246136947933186, %rax
	adcq	$0, %rax
	movq	%rax, -8(%rdx)
	jmp	.L1011
.L469:
	movq	-1065048(%rbp), %rdx
	movabsq	$9222246136947933184, %rax
	movq	-8(%rdx), %r8
	andn	%rax, %r8, %rax
	je	.L1029
	vmovq	%r8, %xmm6
	vxorpd	.LC24(%rip), %xmm6, %xmm0
	vmovsd	%xmm0, -8(%rdx)
	jmp	.L1011
.L468:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933186, %rdi
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	%rdi, (%rax)
	jmp	.L1011
.L467:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933187, %rdi
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	%rdi, (%rax)
	jmp	.L1011
.L466:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933188, %rdi
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	%rdi, (%rax)
	jmp	.L1011
.L465:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933185, %rdi
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	%rdi, (%rax)
	jmp	.L1011
.L462:
	movq	-1065048(%rbp), %rsi
	movabsq	$-9222246136947933185, %rdx
	addq	-8(%rsi), %rdx
	movzwl	(%r12), %ecx
	leaq	2(%r12), %rax
	cmpq	$1, %rdx
	jbe	.L1061
	rolw	$8, %cx
	movzwl	%cx, %edx
	addq	%rdx, %rax
	jmp	.L1012
.L459:
	movq	-1065048(%rbp), %rcx
	movabsq	$-9222246136947933185, %rax
	addq	-8(%rcx), %rax
	movzwl	(%r12), %esi
	leaq	2(%r12), %rdx
	cmpq	$1, %rax
	jbe	.L1062
	subq	$8, %rcx
	movq	%rcx, -1065048(%rbp)
.L461:
	movzbl	(%rdx), %eax
	leaq	dispatch_table.4(%rip), %rdi
	leaq	1(%rdx), %r12
	movq	%rax, %rbx
	jmp	*(%rdi,%rax,8)
.L458:
	movq	-1065048(%rbp), %rax
	movq	-1065112(%rbp), %r15
	leaq	-8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	-8(%rax), %rsi
	movq	%r15, %rdi
	vzeroupper
	call	gab_val_to_string@PLT
	movq	%rax, %rbx
	movq	-1065048(%rbp), %rax
	movq	%r15, %rdi
	movq	-8(%rax), %rsi
	leaq	-8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	call	gab_val_to_string@PLT
	movabsq	$1125899906842623, %rcx
	andq	%rcx, %rbx
	andq	%rcx, %rax
	movq	%rbx, %rdx
	movq	%rax, %rsi
	movq	%r15, %rdi
	call	gab_obj_string_concat@PLT
	movq	-1065048(%rbp), %rdx
	leaq	8(%rdx), %rcx
	movq	%rcx, -1065048(%rbp)
	movabsq	$-1125899906842624, %rcx
	orq	%rcx, %rax
	movq	%rax, (%rdx)
	jmp	.L1011
.L457:
	movq	-1065048(%rbp), %rax
	vmovdqu	-16(%rax), %xmm7
	vpalignr	$8, %xmm7, %xmm7, %xmm0
	vmovdqu	%xmm0, -16(%rax)
	jmp	.L1011
.L456:
	movq	-1065048(%rbp), %rax
	leaq	8(%rax), %rcx
	movq	-8(%rax), %rdx
	movq	%rcx, -1065048(%rbp)
	jmp	.L1010
.L455:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rcx
	movq	-1065048(%rbp), %rdx
	leaq	8(%rdx), %rsi
	movq	%rsi, -1065048(%rbp)
	cmpq	24(%rcx), %rax
	jnb	.L1005
	movq	16(%rcx), %rcx
	movq	(%rcx,%rax,8), %rax
	movq	%rax, (%rdx)
.L1013:
	movzbl	2(%r12), %eax
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	movq	(%rdi,%rax,8), %rax
	addq	$3, %r12
	jmp	*%rax
.L454:
	jmp	.L1011
.L446:
	movq	-1065056(%rbp), %rcx
	movbew	(%r12), %ax
	movq	(%rcx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	leaq	12(%r12), %rsi
	movq	(%rdx,%rax,8), %r9
	movq	-1065048(%rbp), %rax
	movq	-16(%rax), %r8
	movq	-8(%rax), %r10
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r8, %rax
	je	.L1063
.L447:
	movq	%rsi, 8(%rcx)
.L1028:
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %rsi
	leaq	.LC18(%rip), %rcx
	movl	$21, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
.L90:
	movq	-56(%rbp), %rdx
	subq	%fs:40, %rdx
	jne	.L1064
	leaq	-48(%rbp), %rsp
	popq	%rbx
	popq	%r10
	.cfi_remember_state
	.cfi_def_cfa 10, 0
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	leaq	-8(%r10), %rsp
	.cfi_def_cfa 7, 8
	ret
.L430:
	.cfi_restore_state
	movq	-1065056(%rbp), %rdi
	movbew	(%r12), %ax
	movq	(%rdi), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	leaq	2(%r12), %rsi
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movq	(%rdx,%rax,8), %r9
	movq	-1065048(%rbp), %rax
	movq	-16(%rax), %r8
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r8, %rax
	jne	.L431
	movabsq	$1125899906842623, %r10
	andq	%r8, %r10
	cmpl	$9, 12(%r10)
	jne	.L431
	movq	24(%r10), %rdx
	movq	40(%rdx), %rcx
	testq	%rcx, %rcx
	je	.L434
	xorl	%eax, %eax
	jmp	.L433
	.p2align 4,,10
	.p2align 3
.L435:
	incq	%rax
	cmpq	%rcx, %rax
	je	.L434
	cmpq	$65535, %rax
	je	.L337
.L433:
	cmpq	48(%rdx,%rax,8), %r9
	jne	.L435
	movbew	%ax, 2(%r12)
	movq	24(%r10), %rax
	movb	$121, -1(%r12)
	movq	%rax, 4(%r12)
.L92:
	movq	-1065056(%rbp), %rcx
	movbew	(%r12), %ax
	movq	(%rcx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movq	4(%r12), %rdi
	movq	(%rdx,%rax,8), %r9
	movq	-1065048(%rbp), %rdx
	movabsq	$1125899906842623, %r15
	movq	-16(%rdx), %r8
	movq	-8(%rdx), %r10
	movabsq	$-1125899906842624, %rdx
	andq	%r15, %rdi
	movzwl	2(%r12), %eax
	leaq	12(%r12), %rsi
	andn	%rdx, %r8, %rdx
	jne	.L447
	andq	%r8, %r15
	cmpl	$9, 12(%r15)
	jne	.L447
	movq	24(%r15), %rdx
	cmpq	%rdi, %rdx
	jne	.L1065
	rolw	$8, %ax
	movzwl	%ax, %ebx
.L443:
	movq	%r10, -1065144(%rbp)
	movzwl	%ax, %r13d
	cmpq	32(%r15), %rbx
	jnb	.L355
	movq	-1065112(%rbp), %rax
	addq	$4, %r13
	leaq	88(%rax), %rdx
	movq	%rdx, -1065136(%rbp)
	movq	8(%r15,%r13,8), %rcx
	leaq	-1065072(%rbp), %r14
	movq	%r14, %rsi
	movq	%rax, %rdi
	vzeroupper
	call	gab_gc_dref@PLT
	cmpq	32(%r15), %rbx
	jnb	.L445
	movq	-1065144(%rbp), %r10
	movq	%r10, 8(%r15,%r13,8)
.L1015:
	movq	-1065048(%rbp), %rax
	movq	-1065136(%rbp), %rdx
	leaq	-8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%r10, -16(%rax)
	movq	-1065112(%rbp), %rdi
	movq	%r10, %rcx
	movq	%r14, %rsi
	call	gab_gc_iref@PLT
.L1016:
	movzbl	12(%r12), %eax
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	movq	(%rdi,%rax,8), %rax
	addq	$13, %r12
	jmp	*%rax
.L431:
	movq	%rsi, 8(%rdi)
	jmp	.L1028
.L412:
	movq	-1065056(%rbp), %rcx
	movbew	(%r12), %ax
	movq	(%rcx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movq	(%rdx,%rax,8), %rsi
	movq	-1065048(%rbp), %rax
	movq	-8(%rax), %r8
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r8, %rax
	jne	.L413
	movabsq	$1125899906842623, %rdi
	andq	%r8, %rdi
	cmpl	$9, 12(%rdi)
	jne	.L413
	movq	24(%rdi), %rdx
	xorl	%eax, %eax
	movq	40(%rdx), %rcx
	testq	%rcx, %rcx
	jne	.L415
	jmp	.L1023
	.p2align 4,,10
	.p2align 3
.L417:
	incq	%rax
	cmpq	%rcx, %rax
	je	.L1023
	cmpq	$65535, %rax
	je	.L337
.L415:
	cmpq	48(%rdx,%rax,8), %rsi
	jne	.L417
	movzbl	%ah, %edx
.L416:
	movb	%al, 3(%r12)
	movb	%dl, 2(%r12)
	movabsq	$-1125899906842624, %rax
	orq	24(%rdi), %rax
	movq	%rax, 4(%r12)
	movb	$118, -1(%r12)
.L94:
	movq	-1065056(%rbp), %rcx
	movbew	(%r12), %ax
	movq	(%rcx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	-1065048(%rbp), %r10
	movq	16(%rdx), %rdx
	movq	4(%r12), %rdi
	movq	-8(%r10), %r9
	movq	(%rdx,%rax,8), %r8
	movabsq	$1125899906842623, %rax
	movabsq	$-1125899906842624, %rdx
	andq	%rax, %rdi
	movzwl	2(%r12), %esi
	andn	%rdx, %r9, %rdx
	jne	.L419
	andq	%r9, %rax
	cmpl	$9, 12(%rax)
	jne	.L419
	cmpq	%rdi, 24(%rax)
	je	.L421
	movb	$119, -1(%r12)
.L93:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rcx
	movzwl	%ax, %eax
	movq	32(%rcx), %rcx
	movq	32(%rcx), %rcx
	cmpq	24(%rcx), %rax
	jnb	.L1005
	movq	-1065048(%rbp), %r9
	movq	16(%rcx), %rcx
	movq	-8(%r9), %r8
	movq	(%rcx,%rax,8), %rsi
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r8, %rax
	je	.L1066
.L423:
	addq	$12, %r12
	movq	%r12, 8(%rdx)
	jmp	.L1028
.L419:
	addq	$12, %r12
	movq	%r12, 8(%rcx)
	leaq	-1065072(%rbp), %rsi
	leaq	.LC22(%rip), %rcx
.L1030:
	movq	-1065112(%rbp), %rdi
	movl	$21, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L413:
	addq	$2, %r12
	movq	%r12, 8(%rcx)
	jmp	.L1028
.L390:
	movzbl	(%r12), %eax
	testb	$1, %al
	je	.L391
	movq	-1065048(%rbp), %rdx
	movq	-1065112(%rbp), %rdi
	shrb	%al
	addb	(%rdx), %al
	leaq	88(%rdi), %rbx
	leaq	-1065072(%rbp), %r14
	jmp	.L389
.L386:
	movzbl	(%r12), %eax
	movzbl	1(%r12), %r8d
	leaq	2(%r12), %rdx
	testb	$1, %al
	je	.L387
	movq	-1065048(%rbp), %r12
	shrb	%al
	addb	(%r12), %al
	movl	%eax, %r15d
.L388:
	movq	-1065056(%rbp), %rcx
	subq	$8, %rsp
	movq	(%rcx), %rsi
	movq	16(%rcx), %rax
	movq	32(%rsi), %rcx
	movq	-1065112(%rbp), %rbx
	movq	32(%rcx), %rcx
	subq	%rax, %r12
	subq	40(%rcx), %rdx
	pushq	%rax
	sarq	$3, %r12
	movzbl	%r12b, %r9d
	movq	%rbx, %rdi
	movzbl	%r15b, %ecx
	vzeroupper
	call	gab_obj_effect_create@PLT
	movabsq	$-1125899906842624, %rdx
	orq	%rdx, %rax
	movq	%rax, %r13
	movq	-1065056(%rbp), %rax
	movq	%rbx, %rdi
	movq	16(%rax), %r8
	leaq	88(%rbx), %rbx
	leaq	-1065072(%rbp), %r14
	movq	%r12, %rcx
	movq	%rbx, %rdx
	movq	%r14, %rsi
	movq	%rdi, %r12
	call	gab_gc_iref_many@PLT
	movq	-1065048(%rbp), %rax
	movq	%r13, %rcx
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movq	%r13, (%rax)
	movq	%rbx, %rdx
	movq	%r14, %rsi
	movq	%r12, %rdi
	call	gab_gc_dref@PLT
	popq	%rcx
	movq	-1065048(%rbp), %rdx
	leal	1(%r15), %eax
	popq	%rsi
.L389:
	movzbl	%al, %edi
	leaq	0(,%rdi,8), %rcx
	movq	%rdx, %rsi
	subq	%rcx, %rsi
	movq	-1065064(%rbp), %r12
	movq	-1065056(%rbp), %rcx
	movabsq	$-1125899906842624, %r13
	movq	16(%rcx), %r15
	testq	%r12, %r12
	je	.L393
	movq	%rdi, -1065136(%rbp)
	movb	%al, -1065144(%rbp)
	movq	%rsi, -1065152(%rbp)
	movq	%rdx, -1065160(%rbp)
	jmp	.L392
	.p2align 4,,10
	.p2align 3
.L394:
	movq	(%rdx), %rcx
	leaq	32(%r12), %rdx
	movq	%rdx, 24(%r12)
	movq	40(%r12), %rdx
	movq	%rcx, 32(%r12)
	movq	%rdx, -1065064(%rbp)
	movq	-1065112(%rbp), %rdi
	movq	%rbx, %rdx
	movq	%r14, %rsi
	vzeroupper
	call	gab_gc_iref@PLT
	movq	%r12, %rcx
	movq	-1065112(%rbp), %rdi
	orq	%r13, %rcx
	movq	%rbx, %rdx
	movq	%r14, %rsi
	call	gab_gc_dref@PLT
	movq	-1065064(%rbp), %r12
	testq	%r12, %r12
	je	.L1022
.L392:
	movq	24(%r12), %rdx
	cmpq	%r15, %rdx
	jnb	.L394
.L1022:
	movq	-1065056(%rbp), %rcx
	movq	-1065136(%rbp), %rdi
	movzbl	-1065144(%rbp), %eax
	movq	-1065152(%rbp), %rsi
	movq	-1065160(%rbp), %rdx
	movq	16(%rcx), %r15
.L393:
	movzbl	24(%rcx), %r8d
	testb	%al, %al
	sete	%r9b
	cmpb	%al, %r8b
	je	.L716
	cmpb	$-1, %r8b
	je	.L716
	cmpb	%al, %r8b
	jnb	.L398
	movl	%r8d, %eax
	addl	%r9d, %r8d
.L397:
	leal	-1(%rax), %r10d
	testb	%al, %al
	je	.L401
.L399:
	cmpb	$3, %r10b
	jbe	.L402
	movl	$1, %r11d
	subq	%rdi, %r11
	leaq	(%rdx,%r11,8), %rdi
	movq	%r15, %rdx
	subq	%rdi, %rdx
	cmpq	$16, %rdx
	ja	.L1067
.L402:
	leal	1(%r10), %edi
	movzbl	%dil, %edi
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L405:
	movq	(%rsi,%rax,8), %rdx
	movq	%rdx, (%r15,%rax,8)
	incq	%rax
	cmpq	%rax, %rdi
	jne	.L405
.L406:
	leal	1(%r10), %eax
	movzbl	%al, %eax
	leaq	(%r15,%rax,8), %r15
.L401:
	leal	-1(%r9), %esi
	testb	%r9b, %r9b
	je	.L407
.L621:
	cmpb	$2, %sil
	jbe	.L694
	movl	%r9d, %edx
	shrb	$2, %dl
	movzbl	%dl, %edx
	movabsq	$9222246136947933185, %rdi
	salq	$5, %rdx
	vmovq	%rdi, %xmm7
	movq	%r15, %rax
	addq	%r15, %rdx
	vpbroadcastq	%xmm7, %ymm0
	.p2align 4,,10
	.p2align 3
.L409:
	vmovdqu	%ymm0, (%rax)
	addq	$32, %rax
	cmpq	%rdx, %rax
	jne	.L409
	movl	%r9d, %edx
	andl	$-4, %edx
	movq	%r9, %rax
	movl	%esi, %edi
	andl	$252, %eax
	subl	%edx, %edi
	andl	$3, %r9d
	leaq	(%r15,%rax,8), %rax
	je	.L410
.L408:
	movabsq	$9222246136947933185, %rdx
	movq	%rdx, (%rax)
	testb	%dil, %dil
	je	.L410
	movq	%rdx, 8(%rax)
	cmpb	$1, %dil
	je	.L410
	movq	%rdx, 16(%rax)
.L410:
	leal	1(%rsi), %eax
	movzbl	%al, %eax
	leaq	(%r15,%rax,8), %r15
.L407:
	movq	-1065128(%rbp), %rdi
	movzbl	%r8b, %r8d
	leaq	-32(%rcx), %rax
	movq	%r8, (%r15)
	movq	%r15, -1065048(%rbp)
	movq	%rax, -1065056(%rbp)
	cmpq	%rdi, %rax
	je	.L1068
	movq	-24(%rcx), %rax
	jmp	.L1012
.L377:
	movq	-1065056(%rbp), %rax
	movzbl	(%r12), %edx
	movq	16(%rax), %rcx
	movq	-1065048(%rbp), %rdi
	movq	(%rcx,%rdx,8), %rsi
	incq	%r12
	leaq	8(%rdi), %r8
	movq	%r12, 8(%rax)
	movq	%r8, -1065048(%rbp)
	movq	%rsi, (%rdi)
	movabsq	$1125899906842623, %rdx
	andq	%rdx, %rsi
	movzbl	26(%rsi), %r9d
	movzbl	24(%rsi), %r10d
	movl	%r9d, %edx
	subl	%r10d, %r9d
	movl	%r10d, %ecx
	testw	%r9w, %r9w
	jle	.L378
	movq	-1065128(%rbp), %rbx
	movq	%rax, %r9
	subq	%rbx, %r9
	cmpq	$16320, %r9
	jg	.L379
	movq	-1065120(%rbp), %rbx
	movl	%edx, %r9d
	subq	%rbx, %r8
	subl	%ecx, %r9d
	sarq	$3, %r8
	movzbl	%r9b, %r9d
	addq	%r9, %r8
	cmpq	$131071, %r8
	jg	.L379
.L378:
	leaq	32(%rax), %r8
	movq	%r8, -1065056(%rbp)
	movq	32(%rsi), %r8
	movq	%r8, 32(%rax)
	movq	32(%r8), %r8
	movq	32(%r8), %r9
	movq	40(%rsi), %r8
	addq	40(%r9), %r8
	movq	%r8, 40(%rax)
	movb	$-1, 56(%rax)
	movq	%rdi, 48(%rax)
	movzbl	%cl, %r8d
	movzbl	%dl, %eax
	movzbl	25(%rsi), %ebx
	subq	%r8, %rax
	leal	-1(%rbx), %r11d
	leaq	(%rdi,%rax,8), %r10
	cmpb	$-3, %r11b
	ja	.L1069
	movzbl	%bl, %r9d
	cmpb	$2, %r11b
	jbe	.L693
	movl	%ebx, %r8d
	shrb	$2, %r8b
	movzbl	%r8b, %r8d
	movabsq	$9222246136947933185, %r12
	salq	$5, %r8
	vmovq	%r12, %xmm4
	movq	%r10, %rax
	addq	%r10, %r8
	vpbroadcastq	%xmm4, %ymm0
	.p2align 4,,10
	.p2align 3
.L383:
	vmovdqu	%ymm0, (%rax)
	addq	$32, %rax
	cmpq	%r8, %rax
	jne	.L383
	movl	%ebx, %eax
	andl	$-4, %eax
	movq	%rbx, %r8
	movl	%r11d, %r12d
	andl	$252, %r8d
	subl	%eax, %r12d
	andl	$3, %ebx
	leaq	(%r10,%r8,8), %r8
	je	.L381
.L382:
	movabsq	$9222246136947933185, %rax
	movq	%rax, (%r8)
	testb	%r12b, %r12b
	je	.L381
	movq	%rax, 8(%r8)
	cmpb	$1, %r12b
	je	.L381
	movq	%rax, 16(%r8)
.L381:
	leal	1(%r11), %eax
	movzbl	%al, %eax
	leaq	(%r10,%rax,8), %rax
	movq	%r9, (%rax)
	movq	%rax, -1065048(%rbp)
	movzbl	%cl, %eax
	subl	%eax, %edx
	movslq	%edx, %rdx
	salq	$3, %rdx
	addq	$48, %rsi
	vzeroupper
	call	memcpy@PLT
	movq	-1065056(%rbp), %rax
	movq	8(%rax), %rax
	jmp	.L1012
.L358:
	movq	-1065048(%rbp), %rax
	movabsq	$-1125899906842624, %rdx
	movq	(%rax), %rcx
	movq	-8(%rax), %r9
	movbew	(%r12), %r8w
	movzbl	2(%r12), %esi
	movzbl	3(%r12), %edi
	movzbl	%cl, %r13d
	andn	%rdx, %r9, %rdx
	je	.L1070
.L359:
	movl	$1, %edx
	subq	%r13, %rdx
	leaq	(%rax,%rdx,8), %rdx
	movzwl	%r8w, %eax
	movq	%rdx, -1065048(%rbp)
	leaq	2(%r12,%rax), %rax
	jmp	.L1012
.L343:
	movq	-1065048(%rbp), %rax
	movq	-16(%rax), %r8
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r8, %rax
	jne	.L344
	movabsq	$1125899906842623, %rax
	andq	%r8, %rax
	cmpl	$9, 12(%rax)
	jne	.L344
	movb	$100, -1(%r12)
.L95:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movabsq	$1125899906842623, %rsi
	movq	(%rdx,%rax,8), %r15
	movq	-1065048(%rbp), %rax
	andq	%rsi, %r15
	movq	-16(%rax), %rbx
	movq	-8(%rax), %r13
	movabsq	$9222246136947933184, %rax
	movzbl	4(%r12), %r14d
	movq	5(%r12), %rcx
	andn	%rax, %rbx, %rax
	je	.L1071
	movl	$15, -1065196(%rbp)
.L346:
	movq	%rcx, -1065136(%rbp)
	movl	-1065196(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movq	-1065136(%rbp), %rcx
	movq	%rax, %rdx
.L347:
	cmpb	%r14b, 24(%r15)
	jne	.L156
	cmpq	%rdx, %rcx
	jne	.L156
	movabsq	$1125899906842623, %rax
	andq	%rax, %rdx
	movq	40(%rdx), %rcx
	xorl	%eax, %eax
	testq	%rcx, %rcx
	jne	.L351
	jmp	.L352
	.p2align 4,,10
	.p2align 3
.L353:
	incq	%rax
	cmpq	%rcx, %rax
	je	.L352
	cmpq	$65535, %rax
	je	.L337
.L351:
	cmpq	48(%rdx,%rax,8), %r13
	jne	.L353
	movq	-1065048(%rbp), %rsi
	movabsq	$1125899906842623, %rdx
	leaq	-8(%rsi), %rcx
	andq	%rbx, %rdx
	movq	%rcx, -1065048(%rbp)
	cmpq	32(%rdx), %rax
	jnb	.L355
	cltq
	movq	40(%rdx,%rax,8), %rax
	movq	%rax, -16(%rsi)
.L357:
	movq	$1, (%rcx)
	jmp	.L1017
.L344:
	movq	-1065056(%rbp), %rax
	addq	$21, %r12
	movq	%r12, 8(%rax)
	jmp	.L1028
.L323:
	movq	-1065048(%rbp), %rax
	movq	-24(%rax), %r9
	movq	-16(%rax), %r8
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r9, %rax
	jne	.L324
	movabsq	$1125899906842623, %rax
	andq	%r9, %rax
	cmpl	$9, 12(%rax)
	jne	.L324
	movq	24(%rax), %rdx
	movq	40(%rdx), %rcx
	testq	%rcx, %rcx
	je	.L327
	xorl	%eax, %eax
	jmp	.L326
	.p2align 4,,10
	.p2align 3
.L328:
	incq	%rax
	cmpq	%rcx, %rax
	je	.L327
	cmpq	$65535, %rax
	je	.L337
.L326:
	cmpq	48(%rdx,%rax,8), %r8
	jne	.L328
	movb	$98, -1(%r12)
.L96:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movzbl	4(%r12), %r8d
	movq	(%rdx,%rax,8), %rdx
	movq	-1065048(%rbp), %rax
	movq	5(%r12), %rcx
	movq	-24(%rax), %r15
	movq	-8(%rax), %r13
	movq	-16(%rax), %r14
	movabsq	$9222246136947933184, %rax
	andn	%rax, %r15, %rax
	je	.L1072
	movl	$15, -1065192(%rbp)
.L330:
	movq	%rdx, -1065152(%rbp)
	movq	%rcx, -1065144(%rbp)
	movb	%r8b, -1065136(%rbp)
	movq	-1065112(%rbp), %rdi
	movl	-1065192(%rbp), %esi
	vzeroupper
	call	gab_type@PLT
	movq	%rax, %rdi
	movzbl	-1065136(%rbp), %r8d
	movabsq	$1125899906842623, %rax
	movq	-1065144(%rbp), %rcx
	movq	-1065152(%rbp), %rdx
	andq	%rdi, %rax
.L331:
	movq	40(%rax), %rsi
	xorl	%ebx, %ebx
	testq	%rsi, %rsi
	jne	.L336
	jmp	.L156
	.p2align 4,,10
	.p2align 3
.L338:
	incq	%rbx
	cmpq	%rsi, %rbx
	je	.L156
	cmpq	$65535, %rbx
	je	.L337
.L336:
	cmpq	48(%rax,%rbx,8), %r14
	jne	.L338
	movabsq	$1125899906842623, %rax
	andq	%rax, %rdx
	cmpb	%r8b, 24(%rdx)
	jne	.L156
	cmpq	%rdi, %rcx
	jne	.L156
	andq	%rax, %r15
	cmpq	32(%r15), %rbx
	jnb	.L355
	movq	-1065112(%rbp), %rdi
	movslq	%ebx, %rax
	addq	$4, %rax
	leaq	88(%rdi), %rdx
	movq	%rax, -1065144(%rbp)
	movq	%rdx, -1065136(%rbp)
	movq	8(%r15,%rax,8), %rcx
	leaq	-1065072(%rbp), %r14
	movq	%r14, %rsi
	vzeroupper
	call	gab_gc_dref@PLT
	cmpq	32(%r15), %rbx
	movq	-1065136(%rbp), %rdx
	movq	-1065144(%rbp), %rax
	jnb	.L445
	movq	%r13, 8(%r15,%rax,8)
	movq	-1065048(%rbp), %rax
	movq	-1065112(%rbp), %rdi
	leaq	-16(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%r13, -24(%rax)
	movq	%r13, %rcx
	movq	%r14, %rsi
	call	gab_gc_iref@PLT
	movq	-1065048(%rbp), %rax
	movq	$1, (%rax)
	jmp	.L1017
.L324:
	movq	-1065056(%rbp), %rax
	addq	$21, %r12
	movq	%r12, 8(%rax)
	leaq	-1065072(%rbp), %rsi
	leaq	.LC20(%rip), %rcx
	jmp	.L1030
.L316:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rcx
	movabsq	$1125899906842623, %rsi
	movq	(%rcx,%rax,8), %r13
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rdx
	movq	-16(%rax), %rax
	andq	%rsi, %r13
	movzbl	4(%r12), %ebx
	movq	5(%r12), %r14
	andn	%rdx, %rax, %rdx
	je	.L1073
	movl	$15, -1065188(%rbp)
.L317:
	movl	-1065188(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
.L318:
	cmpb	%bl, 24(%r13)
	jne	.L156
	cmpq	%rax, %r14
	jne	.L156
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933186, %rdx
	movq	-16(%rax), %rdi
	movabsq	$9222246136947933187, %rsi
	cmpq	%rdi, -8(%rax)
	cmove	%rsi, %rdx
	leaq	-8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rdx, -16(%rax)
	jmp	.L1018
.L313:
	movq	-1065048(%rbp), %rax
	movabsq	$-1125899906842624, %rcx
	movq	-16(%rax), %rsi
	movq	%rsi, %rdx
	andq	%rcx, %rdx
	cmpq	%rcx, %rdx
	je	.L1074
	.p2align 4,,10
	.p2align 3
.L156:
	movb	$79, -1(%r12)
	movl	$79, %ebx
.L91:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movzbl	2(%r12), %ecx
	movabsq	$1125899906842623, %r13
	andq	(%rdx,%rax,8), %r13
	movl	%ecx, %eax
	shrb	%al
	andl	$1, %ecx
	movq	-1065048(%rbp), %rdx
	je	.L110
	addb	(%rdx), %al
.L110:
	movzbl	%al, %eax
	salq	$3, %rax
	subq	%rax, %rdx
	movq	-8(%rdx), %r11
	movabsq	$9222246136947933184, %rax
	andn	%rax, %r11, %rax
	je	.L1075
	movl	$15, -1065164(%rbp)
.L111:
	movl	-1065164(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movq	%rax, %r11
.L112:
	movq	56(%r13), %rax
	movq	64(%r13), %r8
	leaq	-1(%rax), %rdi
	movq	%rdi, %rsi
	andq	%r11, %rsi
	movq	%rsi, %rax
	movq	$-1, %r9
.L122:
	leaq	(%rax,%rax,2), %rdx
	leaq	(%r8,%rdx,8), %rcx
	movl	16(%rcx), %edx
	movq	(%rcx), %r10
	cmpl	$1, %edx
	je	.L116
	cmpl	$2, %edx
	je	.L117
	testl	%edx, %edx
	je	.L1076
.L119:
	incq	%rax
	andq	%rdi, %rax
	jmp	.L122
.L309:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rcx
	movq	-16(%rax), %rdx
	andn	%rcx, %rdx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rcx, %r8, %rcx
	je	.L1033
	vmovq	%rdx, %xmm0
	vmovq	%r8, %xmm3
	vcomisd	%xmm3, %xmm0
	movabsq	$9222246136947933187, %rdx
	movabsq	$9222246136947933186, %rsi
	cmovb	%rsi, %rdx
	leaq	-8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rdx, -16(%rax)
	jmp	.L1018
.L305:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rcx
	movq	-16(%rax), %rdx
	andn	%rcx, %rdx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rcx, %r8, %rcx
	je	.L1033
	vmovq	%rdx, %xmm0
	vmovq	%r8, %xmm6
	vcomisd	%xmm6, %xmm0
	movabsq	$9222246136947933187, %rdx
	movabsq	$9222246136947933186, %rsi
	cmovbe	%rsi, %rdx
	leaq	-8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rdx, -16(%rax)
	jmp	.L1018
.L301:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rdx
	movq	-16(%rax), %rcx
	andn	%rdx, %rcx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rdx, %r8, %rdx
	je	.L1033
	vmovq	%rcx, %xmm7
	vmovq	%r8, %xmm0
	vcomisd	%xmm7, %xmm0
	movabsq	$9222246136947933187, %rdx
	movabsq	$9222246136947933186, %rcx
	cmovb	%rcx, %rdx
	leaq	-8(%rax), %rsi
	movq	%rsi, -1065048(%rbp)
	movq	%rdx, -16(%rax)
	jmp	.L1018
.L297:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rdx
	movq	-16(%rax), %rcx
	andn	%rdx, %rcx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rdx, %r8, %rdx
	je	.L1033
	vmovq	%rcx, %xmm4
	vmovq	%r8, %xmm0
	vcomisd	%xmm4, %xmm0
	movabsq	$9222246136947933187, %rdx
	movabsq	$9222246136947933186, %rcx
	cmovbe	%rcx, %rdx
	leaq	-8(%rax), %rsi
	movq	%rsi, -1065048(%rbp)
	movq	%rdx, -16(%rax)
	jmp	.L1018
.L289:
	movq	-1065048(%rbp), %rcx
	movabsq	$9222246136947933184, %rdx
	movq	-16(%rcx), %rax
	andn	%rdx, %rax, %rsi
	je	.L156
	movq	-8(%rcx), %r8
	andn	%rdx, %r8, %rdx
	je	.L1033
	vmovsd	.LC19(%rip), %xmm0
	vmovq	%rax, %xmm1
	vcomisd	%xmm0, %xmm1
	leaq	-8(%rcx), %rdx
	movq	%rdx, -1065048(%rbp)
	jnb	.L291
	vcvttsd2siq	%xmm1, %rax
.L292:
	vmovq	%r8, %xmm1
	vcomisd	%xmm0, %xmm1
	jnb	.L293
	vcvttsd2siq	%xmm1, %rdx
.L294:
	shrx	%rdx, %rax, %rax
	testq	%rax, %rax
	js	.L295
	vxorpd	%xmm3, %xmm3, %xmm3
	vcvtsi2sdq	%rax, %xmm3, %xmm0
	jmp	.L296
.L281:
	movq	-1065048(%rbp), %rcx
	movabsq	$9222246136947933184, %rdx
	movq	-16(%rcx), %rax
	andn	%rdx, %rax, %rsi
	je	.L156
	movq	-8(%rcx), %r8
	andn	%rdx, %r8, %rdx
	je	.L1033
	vmovsd	.LC19(%rip), %xmm0
	vmovq	%rax, %xmm1
	vcomisd	%xmm0, %xmm1
	leaq	-8(%rcx), %rdx
	movq	%rdx, -1065048(%rbp)
	jnb	.L283
	vcvttsd2siq	%xmm1, %rax
.L284:
	vmovq	%r8, %xmm1
	vcomisd	%xmm0, %xmm1
	jnb	.L285
	vcvttsd2siq	%xmm1, %rdx
.L286:
	shlx	%rdx, %rax, %rax
	testq	%rax, %rax
	js	.L287
	vxorpd	%xmm7, %xmm7, %xmm7
	vcvtsi2sdq	%rax, %xmm7, %xmm0
	jmp	.L296
.L273:
	movq	-1065048(%rbp), %rcx
	movabsq	$9222246136947933184, %rax
	movq	-16(%rcx), %rdx
	andn	%rax, %rdx, %rsi
	je	.L156
	movq	-8(%rcx), %r8
	andn	%rax, %r8, %rax
	je	.L1033
	vmovsd	.LC19(%rip), %xmm0
	vmovq	%r8, %xmm1
	vcomisd	%xmm0, %xmm1
	leaq	-8(%rcx), %rax
	movq	%rax, -1065048(%rbp)
	jnb	.L275
	vcvttsd2siq	%xmm1, %rax
.L276:
	vmovq	%rdx, %xmm1
	vcomisd	%xmm0, %xmm1
	jnb	.L277
	vcvttsd2siq	%xmm1, %rdx
.L278:
	andq	%rdx, %rax
	js	.L279
	vxorpd	%xmm5, %xmm5, %xmm5
	vcvtsi2sdq	%rax, %xmm5, %xmm0
	jmp	.L296
.L265:
	movq	-1065048(%rbp), %rcx
	movabsq	$9222246136947933184, %rax
	movq	-16(%rcx), %rdx
	andn	%rax, %rdx, %rsi
	je	.L156
	movq	-8(%rcx), %r8
	andn	%rax, %r8, %rax
	je	.L1033
	vmovsd	.LC19(%rip), %xmm0
	vmovq	%r8, %xmm1
	vcomisd	%xmm0, %xmm1
	leaq	-8(%rcx), %rax
	movq	%rax, -1065048(%rbp)
	jnb	.L267
	vcvttsd2siq	%xmm1, %rax
.L268:
	vmovq	%rdx, %xmm1
	vcomisd	%xmm0, %xmm1
	jnb	.L269
	vcvttsd2siq	%xmm1, %rdx
.L270:
	orq	%rdx, %rax
	js	.L271
	vxorpd	%xmm2, %xmm2, %xmm2
	vcvtsi2sdq	%rax, %xmm2, %xmm0
	jmp	.L296
.L257:
	movq	-1065048(%rbp), %rcx
	movabsq	$9222246136947933184, %rdx
	movq	-16(%rcx), %rax
	andn	%rdx, %rax, %rsi
	je	.L156
	movq	-8(%rcx), %r8
	andn	%rdx, %r8, %rdx
	je	.L1033
	vmovsd	.LC19(%rip), %xmm0
	vmovq	%rax, %xmm1
	vcomisd	%xmm0, %xmm1
	leaq	-8(%rcx), %rdx
	movq	%rdx, -1065048(%rbp)
	jnb	.L259
	vcvttsd2siq	%xmm1, %rax
.L260:
	vmovq	%r8, %xmm1
	vcomisd	%xmm0, %xmm1
	jnb	.L261
	vcvttsd2siq	%xmm1, %rsi
.L262:
	xorl	%edx, %edx
	divq	%rsi
	testq	%rdx, %rdx
	js	.L263
	vxorpd	%xmm6, %xmm6, %xmm6
	vcvtsi2sdq	%rdx, %xmm6, %xmm0
	.p2align 4,,10
	.p2align 3
.L296:
	movq	$1, -8(%rcx)
	vmovsd	%xmm0, -16(%rcx)
	jmp	.L1017
.L255:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rcx
	movq	-16(%rax), %rdx
	andn	%rcx, %rdx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rcx, %r8, %rcx
	je	.L1033
	leaq	-8(%rax), %rcx
	vmovq	%rdx, %xmm3
	vmovq	%r8, %xmm5
	vdivsd	%xmm5, %xmm3, %xmm0
	movq	%rcx, -1065048(%rbp)
	jmp	.L1014
.L253:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rcx
	movq	-16(%rax), %rdx
	andn	%rcx, %rdx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rcx, %r8, %rcx
	je	.L1033
	leaq	-8(%rax), %rcx
	vmovq	%rdx, %xmm7
	vmovq	%r8, %xmm6
	vmulsd	%xmm6, %xmm7, %xmm0
	movq	%rcx, -1065048(%rbp)
	jmp	.L1014
.L251:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rcx
	movq	-16(%rax), %rdx
	andn	%rcx, %rdx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rcx, %r8, %rcx
	je	.L1033
	leaq	-8(%rax), %rcx
	vmovq	%rdx, %xmm5
	vmovq	%r8, %xmm4
	movq	%rcx, -1065048(%rbp)
	vsubsd	%xmm4, %xmm5, %xmm0
	jmp	.L1014
.L249:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933184, %rcx
	movq	-16(%rax), %rdx
	andn	%rcx, %rdx, %rsi
	je	.L156
	movq	-8(%rax), %r8
	andn	%rcx, %r8, %rcx
	je	.L1033
	leaq	-8(%rax), %rcx
	vmovq	%rdx, %xmm7
	vmovq	%r8, %xmm6
	movq	%rcx, -1065048(%rbp)
	vaddsd	%xmm6, %xmm7, %xmm0
.L1014:
	vmovsd	%xmm0, -16(%rax)
.L1018:
	movq	$1, -8(%rax)
.L1017:
	movzbl	21(%r12), %eax
	leaq	dispatch_table.4(%rip), %rdi
	movq	%rax, %rbx
	movq	(%rdi,%rax,8), %rax
	addq	$22, %r12
	jmp	*%rax
.L223:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rcx
	movabsq	$1125899906842623, %rdx
	andq	(%rcx,%rax,8), %rdx
	movzbl	2(%r12), %eax
	movq	-1065048(%rbp), %rsi
	movl	%eax, %ebx
	shrb	%bl
	testb	$1, %al
	je	.L224
	addb	(%rsi), %bl
.L224:
	movzbl	%bl, %ecx
	leaq	0(,%rcx,8), %r8
	movq	$-8, %r14
	movzbl	3(%r12), %eax
	subq	%r8, %r14
	movq	(%rsi,%r14), %r13
	movb	%al, -1065201(%rbp)
	movabsq	$9222246136947933184, %rax
	movzbl	4(%r12), %r15d
	movq	5(%r12), %r10
	andn	%rax, %r13, %rax
	je	.L1077
	movl	$15, -1065184(%rbp)
.L225:
	movq	%r10, -1065160(%rbp)
	movq	%rdx, -1065152(%rbp)
	movq	%r8, -1065144(%rbp)
	movq	%rcx, -1065136(%rbp)
	movl	-1065184(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movq	-1065160(%rbp), %r10
	movq	-1065152(%rbp), %rdx
	movq	-1065144(%rbp), %r8
	movq	-1065136(%rbp), %rcx
.L226:
	cmpb	%r15b, 24(%rdx)
	jne	.L156
	cmpq	%rax, %r10
	jne	.L156
	movq	-1065056(%rbp), %rsi
	addq	$21, %r12
	movq	%r12, 8(%rsi)
	movabsq	$1125899906842623, %rdi
	andq	%r13, %rdi
	movzbl	26(%rdi), %eax
	movzbl	24(%rdi), %r9d
	movzbl	%bl, %r10d
	movl	%eax, %edx
	subl	%r10d, %eax
	movzbl	%r9b, %r10d
	subl	%r10d, %eax
	movq	-1065048(%rbp), %r11
	testw	%ax, %ax
	jle	.L231
	movq	-1065128(%rbp), %r11
	movq	%rsi, %r10
	subq	%r11, %r10
	cmpq	$16320, %r10
	jg	.L379
	movq	-1065048(%rbp), %r11
	movq	-1065120(%rbp), %r15
	movq	%r11, %r10
	subq	%r15, %r10
	sarq	$3, %r10
	movzwl	%ax, %eax
	addq	%r10, %rax
	cmpq	$131071, %rax
	jg	.L379
.L231:
	leaq	32(%rsi), %rax
	movq	%rax, -1065056(%rbp)
	movq	32(%rdi), %rax
	addq	%r11, %r14
	movq	%rax, 32(%rsi)
	movq	32(%rax), %rax
	movq	32(%rax), %r10
	movq	40(%rdi), %rax
	addq	40(%r10), %rax
	movq	%rax, 40(%rsi)
	movzbl	-1065201(%rbp), %eax
	movq	%r14, 48(%rsi)
	movb	%al, 56(%rsi)
	movq	%r11, %r10
	movzbl	%r9b, %esi
	movzbl	%dl, %eax
	subq	%rsi, %rax
	subq	%r8, %r10
	movzbl	25(%rdi), %r8d
	leaq	0(,%rax,8), %r15
	testb	%bl, %bl
	leaq	(%r14,%r15), %rsi
	sete	%r12b
	cmpb	%bl, %r8b
	je	.L710
	cmpb	$-1, %r8b
	je	.L710
	cmpb	%bl, %r8b
	jnb	.L236
	movl	%r8d, %ebx
	addl	%r12d, %r8d
.L235:
	leal	-1(%rbx), %r13d
	testb	%bl, %bl
	je	.L239
.L237:
	cmpb	$3, %r13b
	jbe	.L240
	subq	$16, %r15
	cmpq	$16, %r15
	ja	.L1078
.L240:
	leal	1(%r13), %r11d
	movzbl	%r11b, %r11d
	xorl	%eax, %eax
.L243:
	movq	(%r10,%rax,8), %rcx
	movq	%rcx, (%rsi,%rax,8)
	incq	%rax
	cmpq	%r11, %rax
	jne	.L243
.L244:
	leal	1(%r13), %eax
	movzbl	%al, %eax
	leaq	(%rsi,%rax,8), %rsi
.L239:
	leal	-1(%r12), %r10d
	testb	%r12b, %r12b
	je	.L245
.L619:
	cmpb	$2, %r10b
	jbe	.L671
	movl	%r12d, %ecx
	shrb	$2, %cl
	movzbl	%cl, %ecx
	movabsq	$9222246136947933185, %r11
	salq	$5, %rcx
	vmovq	%r11, %xmm7
	movq	%rsi, %rax
	addq	%rsi, %rcx
	vpbroadcastq	%xmm7, %ymm0
.L247:
	vmovdqu	%ymm0, (%rax)
	addq	$32, %rax
	cmpq	%rax, %rcx
	jne	.L247
	movl	%r12d, %ecx
	andl	$-4, %ecx
	movq	%r12, %rax
	movl	%r10d, %r11d
	andl	$252, %eax
	subl	%ecx, %r11d
	andl	$3, %r12d
	leaq	(%rsi,%rax,8), %rax
	je	.L248
.L246:
	movabsq	$9222246136947933185, %rcx
	movq	%rcx, (%rax)
	testb	%r11b, %r11b
	je	.L248
	movq	%rcx, 8(%rax)
	cmpb	$1, %r11b
	je	.L248
	movq	%rcx, 16(%rax)
.L248:
	leal	1(%r10), %eax
	movzbl	%al, %eax
	leaq	(%rsi,%rax,8), %rsi
.L245:
	movzbl	%r8b, %r8d
	movq	%r8, (%rsi)
	subl	%r9d, %edx
	movq	%rsi, -1065048(%rbp)
	movslq	%edx, %rdx
	leaq	48(%rdi), %rsi
	salq	$3, %rdx
	movq	%r14, %rdi
	vzeroupper
	call	memcpy@PLT
	movq	-1065056(%rbp), %rax
	leaq	dispatch_table.4(%rip), %rsi
	movq	8(%rax), %rax
	leaq	1(%rax), %r12
	movzbl	(%rax), %eax
	movq	%rax, %rbx
	jmp	*(%rsi,%rax,8)
.L200:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movabsq	$1125899906842623, %r14
	andq	(%rdx,%rax,8), %r14
	movzbl	2(%r12), %edx
	movq	-1065048(%rbp), %rax
	movl	%edx, %ebx
	shrb	%bl
	andl	$1, %edx
	je	.L201
	addb	(%rax), %bl
.L201:
	movzbl	%bl, %r15d
	leaq	0(,%r15,8), %rsi
	subq	%rsi, %rax
	movq	-8(%rax), %r13
	movabsq	$9222246136947933184, %rax
	movzbl	3(%r12), %ecx
	movzbl	4(%r12), %edx
	movq	5(%r12), %r8
	andn	%rax, %r13, %rax
	je	.L1079
	movl	$15, -1065180(%rbp)
.L202:
	movq	%r8, -1065152(%rbp)
	movb	%dl, -1065144(%rbp)
	movb	%cl, -1065136(%rbp)
	movl	-1065180(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movq	-1065152(%rbp), %r8
	movzbl	-1065144(%rbp), %edx
	movzbl	-1065136(%rbp), %ecx
.L203:
	cmpb	%dl, 24(%r14)
	jne	.L156
	cmpq	%rax, %r8
	jne	.L156
	movq	-1065056(%rbp), %rsi
	addq	$21, %r12
	movabsq	$1125899906842623, %rax
	movq	%r12, 8(%rsi)
	andq	%rax, %r13
	movq	32(%r13), %r8
	cmpb	$0, 42(%r8)
	movzbl	24(%r8), %r12d
	je	.L207
	leaq	32(%rsi), %rax
	movq	%rax, -1065056(%rbp)
	movq	32(%r8), %rax
	subl	%r12d, %ebx
	movq	40(%rax), %rax
	movzbl	%bl, %edx
	movb	%cl, 56(%rsi)
	movq	%rax, 40(%rsi)
	movb	%dl, -1065136(%rbp)
	movq	%r13, 32(%rsi)
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %r14
	movq	%r14, %rsi
	vzeroupper
	movzbl	%dl, %ebx
	call	gab_obj_shape_create_tuple@PLT
	movq	-1065048(%rbp), %rcx
	leaq	0(,%rbx,8), %rdx
	movq	-1065112(%rbp), %rdi
	subq	%rdx, %rcx
	movq	%rax, %rsi
	movq	%rdx, %r15
	movl	$1, %edx
	call	gab_obj_record_create@PLT
	movq	-1065112(%rbp), %rdi
	movabsq	$-1125899906842624, %rdx
	orq	%rdx, %rax
	movq	-1065048(%rbp), %r8
	movzbl	-1065136(%rbp), %ecx
	movq	%rax, %rbx
	negq	%r15
	leaq	88(%rdi), %rax
	movq	%rax, %rdx
	addq	%r15, %r8
	movq	%r14, %rsi
	movq	%rax, -1065136(%rbp)
	call	gab_gc_iref_many@PLT
	movq	-1065048(%rbp), %rdx
	movq	-1065112(%rbp), %rdi
	addq	%r15, %rdx
	leaq	8(%rdx), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rbx, (%rdx)
	movq	-1065136(%rbp), %rdx
	movq	%r14, %rsi
	movq	%rbx, %rcx
	call	gab_gc_dref@PLT
	movq	-1065048(%rbp), %rsi
	leal	1(%r12), %eax
	movzbl	%al, %eax
	salq	$3, %rax
	movq	%rsi, %rdx
	subq	%rax, %rdx
	movq	-1065056(%rbp), %rdi
	leaq	-8(%rdx), %rax
	movq	%rax, 16(%rdi)
	movq	32(%r13), %rax
	movzbl	24(%rax), %edx
	movzbl	40(%rax), %r9d
	leal	1(%rdx), %r8d
	cmpb	%r9b, %r8b
	jnb	.L1020
	movl	%r9d, %eax
	subl	%edx, %eax
	leal	-2(%rax), %r10d
	leal	-1(%rax), %r11d
	cmpb	$2, %r10b
	jbe	.L662
	leal	-5(%rax), %edx
	movabsq	$9222246136947933185, %rcx
	shrb	$2, %dl
	vmovq	%rcx, %xmm4
	incl	%edx
	xorl	%eax, %eax
	vpbroadcastq	%xmm4, %ymm0
.L211:
	movq	%rax, %rcx
	salq	$5, %rcx
	incq	%rax
	vmovdqu	%ymm0, (%rsi,%rcx)
	cmpb	%dl, %al
	jb	.L211
	sall	$2, %edx
	movzbl	%dl, %eax
	addl	%edx, %r8d
	leaq	(%rsi,%rax,8), %rcx
	cmpb	%r11b, %dl
	je	.L212
.L210:
	movabsq	$9222246136947933185, %rdx
	leal	1(%r8), %eax
	movq	%rdx, (%rcx)
	cmpb	%r9b, %al
	jnb	.L212
	leal	2(%r8), %eax
	movq	%rdx, 8(%rcx)
	cmpb	%r9b, %al
	jnb	.L212
	movq	%rdx, 16(%rcx)
.L212:
	movzbl	%r10b, %r10d
	leaq	8(,%r10,8), %rax
	leaq	(%rsi,%rax), %rdx
	movabsq	$9222246136947933185, %rcx
	movq	%rdx, -1065048(%rbp)
	movq	%rcx, -8(%rsi,%rax)
.L1020:
	movq	8(%rdi), %rdi
	jmp	.L209
.L173:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movabsq	$1125899906842623, %r14
	andq	(%rdx,%rax,8), %r14
	movzbl	2(%r12), %eax
	movq	-1065048(%rbp), %rsi
	movl	%eax, %ecx
	shrb	%cl
	testb	$1, %al
	je	.L174
	addb	(%rsi), %cl
.L174:
	movzbl	%cl, %r15d
	salq	$3, %r15
	movq	$-8, %rdx
	subq	%r15, %rdx
	movq	%rdx, %r13
	movq	(%rsi,%rdx), %rdx
	movabsq	$9222246136947933184, %rax
	movzbl	3(%r12), %ebx
	movzbl	4(%r12), %r8d
	movq	5(%r12), %r9
	movq	13(%r12), %r10
	andn	%rax, %rdx, %rax
	je	.L1080
	movl	$15, -1065172(%rbp)
.L175:
	movb	%cl, -1065160(%rbp)
	movq	%r10, -1065152(%rbp)
	movq	%r9, -1065144(%rbp)
	movb	%r8b, -1065136(%rbp)
	movl	-1065172(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movzbl	-1065160(%rbp), %ecx
	movq	-1065152(%rbp), %r10
	movq	-1065144(%rbp), %r9
	movzbl	-1065136(%rbp), %r8d
	movq	%rax, %rdx
.L176:
	cmpb	%r8b, 24(%r14)
	jne	.L156
	cmpq	%rdx, %r9
	jne	.L156
	movq	64(%r14), %r11
	movq	-1065056(%rbp), %rdx
	leaq	(%r10,%r10,2), %rax
	leaq	(%r11,%rax,8), %rax
	leaq	21(%r12), %rsi
	movq	8(%rax), %rax
	movq	%rsi, 8(%rdx)
	movq	-1065048(%rbp), %rsi
	movq	-1065112(%rbp), %rdi
	addq	%r13, %rsi
	leal	1(%rcx), %edx
	leaq	-1065072(%rbp), %r14
	movabsq	$1125899906842623, %rcx
	andq	%rcx, %rax
	movzbl	%dl, %edx
	movq	%rsi, %rcx
	movq	%r14, %rsi
	vzeroupper
	call	*24(%rax)
	movq	-1065048(%rbp), %rcx
	leaq	(%rcx,%r13), %rdx
	cmpb	$1, %bl
	je	.L708
	cmpb	$-1, %bl
	je	.L708
	xorl	%esi, %esi
	testb	%bl, %bl
	jne	.L1081
.L182:
	movq	%rsi, (%rdx)
	movq	%rdx, -1065048(%rbp)
	leaq	dispatch_table.4(%rip), %rsi
	addq	$22, %r12
	movzbl	-1(%r12), %eax
	movq	%rax, %rbx
	movq	(%rsi,%rax,8), %rax
	jmp	*%rax
.L186:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movabsq	$1125899906842623, %rcx
	andq	(%rdx,%rax,8), %rcx
	movzbl	2(%r12), %edx
	movq	-1065048(%rbp), %rsi
	movl	%edx, %eax
	shrb	%al
	andl	$1, %edx
	movzbl	%al, %r15d
	je	.L188
	addl	(%rsi), %r15d
	movl	%r15d, %eax
.L188:
	movzbl	%al, %eax
	leaq	0(,%rax,8), %r14
	movq	$-8, %rdx
	subq	%r14, %rdx
	movq	%rdx, %r13
	movq	(%rsi,%rdx), %rdx
	movabsq	$9222246136947933184, %rax
	movzbl	3(%r12), %ebx
	movzbl	4(%r12), %r8d
	movq	5(%r12), %r9
	andn	%rax, %rdx, %rax
	je	.L1082
	movl	$15, -1065176(%rbp)
.L189:
	movq	%rdx, -1065160(%rbp)
	movq	%r9, -1065152(%rbp)
	movb	%r8b, -1065144(%rbp)
	movq	%rcx, -1065136(%rbp)
	movl	-1065176(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movq	-1065160(%rbp), %rdx
	movq	-1065152(%rbp), %r9
	movzbl	-1065144(%rbp), %r8d
	movq	-1065136(%rbp), %rcx
.L190:
	cmpb	%r8b, 24(%rcx)
	jne	.L156
	cmpq	%rax, %r9
	jne	.L156
	movq	-1065056(%rbp), %rax
	addq	$21, %r12
	movq	-1065048(%rbp), %rcx
	movq	%r12, 8(%rax)
	subq	%r14, %rcx
	movq	-1065112(%rbp), %rdi
	movq	%r14, %r12
	movabsq	$1125899906842623, %rax
	leaq	-1065072(%rbp), %r14
	andq	%rdx, %rax
	negq	%r12
	movzbl	%r15b, %edx
	movq	%r14, %rsi
	vzeroupper
	call	*24(%rax)
	movq	-1065048(%rbp), %rcx
	leaq	(%rcx,%r13), %rdx
	cmpb	$1, %bl
	je	.L709
	cmpb	$-1, %bl
	je	.L709
	xorl	%edi, %edi
	testb	%bl, %bl
	jne	.L1083
.L196:
	movq	-1065056(%rbp), %rax
	movq	%rdi, (%rdx)
	movq	8(%rax), %rax
	movq	%rdx, -1065048(%rbp)
	jmp	.L1012
.L149:
	movq	-1065056(%rbp), %rdx
	movbew	(%r12), %ax
	movq	(%rdx), %rdx
	movzwl	%ax, %eax
	movq	32(%rdx), %rdx
	movq	32(%rdx), %rdx
	cmpq	24(%rdx), %rax
	jnb	.L1005
	movq	16(%rdx), %rdx
	movabsq	$1125899906842623, %r13
	andq	(%rdx,%rax,8), %r13
	movzbl	2(%r12), %edx
	movq	-1065048(%rbp), %rax
	movl	%edx, %ebx
	shrb	%bl
	andl	$1, %edx
	je	.L150
	addb	(%rax), %bl
.L150:
	movzbl	%bl, %r14d
	leaq	0(,%r14,8), %rsi
	subq	%rsi, %rax
	movq	-8(%rax), %rsi
	movabsq	$9222246136947933184, %rax
	movzbl	3(%r12), %r15d
	movzbl	4(%r12), %ecx
	movq	5(%r12), %r8
	movq	13(%r12), %rdx
	andn	%rax, %rsi, %rax
	je	.L1084
	movl	$15, -1065168(%rbp)
.L151:
	movq	%rdx, -1065152(%rbp)
	movq	%r8, -1065144(%rbp)
	movb	%cl, -1065136(%rbp)
	movl	-1065168(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
	movq	-1065152(%rbp), %rdx
	movq	-1065144(%rbp), %r8
	movzbl	-1065136(%rbp), %ecx
	movq	%rax, %rsi
.L152:
	cmpb	%cl, 24(%r13)
	jne	.L156
	cmpq	%rsi, %r8
	jne	.L156
	leaq	(%rdx,%rdx,2), %rax
	movq	64(%r13), %rdx
	addq	$21, %r12
	leaq	(%rdx,%rax,8), %rax
	movq	-1065056(%rbp), %rdx
	movabsq	$1125899906842623, %r13
	andq	8(%rax), %r13
	movq	%r12, 8(%rdx)
	movq	32(%r13), %rsi
	cmpb	$0, 42(%rsi)
	movzbl	24(%rsi), %r12d
	je	.L157
	leaq	32(%rdx), %rax
	movq	%rax, -1065056(%rbp)
	movq	32(%rsi), %rax
	subl	%r12d, %ebx
	movq	40(%rax), %rax
	movq	%r13, 32(%rdx)
	movq	%rax, 40(%rdx)
	movb	%r15b, 56(%rdx)
	movzbl	%bl, %edx
	movb	%dl, -1065136(%rbp)
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %r14
	movq	%r14, %rsi
	vzeroupper
	movzbl	%dl, %ebx
	call	gab_obj_shape_create_tuple@PLT
	movq	-1065048(%rbp), %rcx
	leaq	0(,%rbx,8), %rdx
	movq	-1065112(%rbp), %rdi
	subq	%rdx, %rcx
	movq	%rax, %rsi
	movq	%rdx, %r15
	movl	$1, %edx
	call	gab_obj_record_create@PLT
	movq	-1065112(%rbp), %rdi
	movabsq	$-1125899906842624, %rdx
	orq	%rdx, %rax
	movq	-1065048(%rbp), %r8
	movzbl	-1065136(%rbp), %ecx
	movq	%rax, %rbx
	negq	%r15
	leaq	88(%rdi), %rax
	movq	%rax, %rdx
	addq	%r15, %r8
	movq	%r14, %rsi
	movq	%rax, -1065136(%rbp)
	call	gab_gc_iref_many@PLT
	movq	-1065048(%rbp), %rdx
	movq	-1065112(%rbp), %rdi
	addq	%r15, %rdx
	leaq	8(%rdx), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rbx, (%rdx)
	movq	-1065136(%rbp), %rdx
	movq	%r14, %rsi
	movq	%rbx, %rcx
	call	gab_gc_dref@PLT
	movq	-1065048(%rbp), %rsi
	leal	1(%r12), %eax
	movzbl	%al, %eax
	salq	$3, %rax
	movq	%rsi, %rdx
	subq	%rax, %rdx
	movq	-1065056(%rbp), %rdi
	leaq	-8(%rdx), %rax
	movq	%rax, 16(%rdi)
	movq	32(%r13), %rax
	movzbl	24(%rax), %edx
	movzbl	40(%rax), %r9d
	leal	1(%rdx), %r8d
	cmpb	%r9b, %r8b
	jnb	.L1019
	movl	%r9d, %eax
	subl	%edx, %eax
	leal	-2(%rax), %r10d
	leal	-1(%rax), %r11d
	cmpb	$2, %r10b
	jbe	.L640
	leal	-5(%rax), %edx
	movabsq	$9222246136947933185, %rcx
	shrb	$2, %dl
	vmovq	%rcx, %xmm3
	incl	%edx
	xorl	%eax, %eax
	vpbroadcastq	%xmm3, %ymm0
.L161:
	movq	%rax, %rcx
	salq	$5, %rcx
	incq	%rax
	vmovdqu	%ymm0, (%rsi,%rcx)
	cmpb	%dl, %al
	jb	.L161
	sall	$2, %edx
	movzbl	%dl, %eax
	addl	%edx, %r8d
	leaq	(%rsi,%rax,8), %rcx
	cmpb	%r11b, %dl
	je	.L162
.L160:
	movabsq	$9222246136947933185, %rdx
	leal	1(%r8), %eax
	movq	%rdx, (%rcx)
	cmpb	%r9b, %al
	jnb	.L162
	leal	2(%r8), %eax
	movq	%rdx, 8(%rcx)
	cmpb	%r9b, %al
	jnb	.L162
	movq	%rdx, 16(%rcx)
.L162:
	movzbl	%r10b, %r10d
	leaq	8(,%r10,8), %rax
	leaq	(%rsi,%rax), %rdx
	movabsq	$9222246136947933185, %rcx
	movq	%rdx, -1065048(%rbp)
	movq	%rcx, -8(%rsi,%rax)
.L1019:
	movq	8(%rdi), %rcx
.L159:
	movzbl	(%rcx), %eax
	leaq	dispatch_table.4(%rip), %rdi
	leaq	1(%rcx), %r12
	movq	%rax, %rbx
	jmp	*(%rdi,%rax,8)
	.p2align 4,,10
	.p2align 3
.L1055:
	movq	-1065064(%rbp), %rbx
	xorl	%r12d, %r12d
	testq	%rbx, %rbx
	jne	.L559
	jmp	.L1085
	.p2align 4,,10
	.p2align 3
.L562:
	movq	40(%rbx), %rax
	movq	%rbx, %r12
	testq	%rax, %rax
	je	.L561
	movq	%rax, %rbx
.L559:
	cmpq	24(%rbx), %rsi
	jb	.L562
	cmpq	%rsi, 24(%rbx)
	je	.L1086
	movq	-1065112(%rbp), %rdi
	call	gab_obj_upvalue_create@PLT
	movq	%rbx, 40(%rax)
	testq	%r12, %r12
	je	.L560
.L565:
	movq	%rax, 40(%r12)
.L566:
	movabsq	$-1125899906842624, %rdi
	movzbl	25(%r14), %ecx
	orq	%rdi, %rax
	jmp	.L567
	.p2align 4,,10
	.p2align 3
.L1051:
	movq	-1065064(%rbp), %r14
	xorl	%r15d, %r15d
	testq	%r14, %r14
	jne	.L580
	jmp	.L1087
	.p2align 4,,10
	.p2align 3
.L583:
	movq	40(%r14), %rax
	movq	%r14, %r15
	testq	%rax, %rax
	je	.L582
	movq	%rax, %r14
.L580:
	cmpq	24(%r14), %rsi
	jb	.L583
	cmpq	%rsi, 24(%r14)
	je	.L1088
	movq	-1065112(%rbp), %rdi
	call	gab_obj_upvalue_create@PLT
	movq	%r14, 40(%rax)
	testq	%r15, %r15
	je	.L581
.L586:
	movq	%rax, 40(%r15)
.L587:
	movabsq	$-1125899906842624, %rdi
	movzbl	24(%r12), %ecx
	orq	%rdi, %rax
	jmp	.L588
.L102:
	movq	-1065112(%rbp), %rdi
	leaq	.LC16(%rip), %rcx
	movl	$26, %edx
	movq	%r14, %rsi
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L1049:
	vextracti128	$0x1, %ymm0, %xmm0
	vpextrq	$1, %xmm0, %rdx
	jmp	.L101
.L1073:
	movabsq	$9222246136947933185, %rdx
	cmpq	%rdx, %rax
	je	.L318
	addq	$3, %rdx
	cmpq	%rdx, %rax
	je	.L318
	movq	%rax, %rcx
	orq	$1, %rcx
	decq	%rdx
	cmpq	%rdx, %rcx
	je	.L677
	movabsq	$-1125899906842624, %rdx
	andn	%rdx, %rax, %rdx
	je	.L1089
.L319:
	movl	-1065188(%rbp), %edi
	cmpl	$9, %edi
	je	.L320
	jbe	.L1090
	cmpl	$10, %edi
	je	.L318
	leal	-13(%rdi), %edx
	cmpl	$1, %edx
	ja	.L317
	jmp	.L318
.L1072:
	movabsq	$9222246136947933185, %rax
	cmpq	%rax, %r15
	je	.L681
	addq	$3, %rax
	cmpq	%rax, %r15
	je	.L682
	movq	%r15, %rsi
	orq	$1, %rsi
	decq	%rax
	cmpq	%rax, %rsi
	je	.L683
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r15, %rax
	je	.L1091
.L332:
	movl	-1065192(%rbp), %eax
	cmpl	$9, %eax
	je	.L333
	jbe	.L1092
	cmpl	$10, %eax
	je	.L335
	subl	$13, %eax
	cmpl	$1, %eax
	ja	.L330
.L335:
	movabsq	$1125899906842623, %rax
	andq	%r15, %rax
	movq	%r15, %rdi
	jmp	.L331
.L1084:
	movabsq	$9222246136947933185, %rax
	cmpq	%rax, %rsi
	je	.L152
	addq	$3, %rax
	cmpq	%rax, %rsi
	je	.L152
	movq	%rsi, %rdi
	orq	$1, %rdi
	decq	%rax
	cmpq	%rax, %rdi
	je	.L639
	movabsq	$-1125899906842624, %rax
	andn	%rax, %rsi, %rax
	je	.L1093
.L153:
	movl	-1065168(%rbp), %eax
	cmpl	$9, %eax
	je	.L154
	jbe	.L1094
	cmpl	$10, %eax
	je	.L152
	subl	$13, %eax
	cmpl	$1, %eax
	ja	.L151
	jmp	.L152
.L1077:
	movabsq	$9222246136947933185, %rsi
	movq	%r13, %rax
	cmpq	%rsi, %r13
	je	.L226
	addq	$3, %rsi
	cmpq	%rsi, %r13
	je	.L226
	movq	%r13, %rsi
	orq	$1, %rsi
	movabsq	$9222246136947933187, %rax
	cmpq	%rax, %rsi
	je	.L668
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r13, %rax
	je	.L1095
.L227:
	movl	-1065184(%rbp), %edi
	cmpl	$9, %edi
	je	.L228
	jbe	.L1096
	movl	%edi, %esi
	movq	%r13, %rax
	cmpl	$10, %edi
	je	.L226
	subl	$13, %esi
	cmpl	$1, %esi
	ja	.L225
	jmp	.L226
.L1071:
	movabsq	$9222246136947933185, %rax
	movq	%rbx, %rdx
	cmpq	%rax, %rbx
	je	.L347
	addq	$3, %rax
	cmpq	%rax, %rbx
	je	.L347
	orq	$1, %rdx
	decq	%rax
	cmpq	%rax, %rdx
	je	.L688
	movabsq	$-1125899906842624, %rax
	andn	%rax, %rbx, %rax
	je	.L1097
.L348:
	movl	-1065196(%rbp), %eax
	cmpl	$9, %eax
	je	.L349
	movq	%rbx, %rdx
	jbe	.L1098
	cmpl	$10, %eax
	je	.L347
	subl	$13, %eax
	cmpl	$1, %eax
	ja	.L346
	jmp	.L347
.L1075:
	movabsq	$9222246136947933185, %rax
	cmpq	%rax, %r11
	je	.L112
	addq	$3, %rax
	cmpq	%rax, %r11
	je	.L112
	movq	%r11, %rdx
	orq	$1, %rdx
	decq	%rax
	cmpq	%rax, %rdx
	je	.L631
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r11, %rax
	je	.L1099
.L113:
	movl	-1065164(%rbp), %eax
	cmpl	$9, %eax
	je	.L114
	jbe	.L1100
	cmpl	$10, %eax
	je	.L112
	subl	$13, %eax
	cmpl	$1, %eax
	ja	.L111
	jmp	.L112
.L1079:
	movabsq	$9222246136947933185, %rsi
	movq	%r13, %rax
	cmpq	%rsi, %r13
	je	.L203
	addq	$3, %rsi
	cmpq	%rsi, %r13
	je	.L203
	movq	%r13, %rsi
	orq	$1, %rsi
	movabsq	$9222246136947933187, %rax
	cmpq	%rax, %rsi
	je	.L659
	movabsq	$-1125899906842624, %rax
	andn	%rax, %r13, %rax
	je	.L1101
.L204:
	movl	-1065180(%rbp), %edi
	cmpl	$9, %edi
	je	.L205
	jbe	.L1102
	movl	%edi, %esi
	movq	%r13, %rax
	cmpl	$10, %edi
	je	.L203
	subl	$13, %esi
	cmpl	$1, %esi
	ja	.L202
	jmp	.L203
.L1080:
	movabsq	$9222246136947933185, %rax
	cmpq	%rax, %rdx
	je	.L176
	addq	$3, %rax
	cmpq	%rax, %rdx
	je	.L176
	movq	%rdx, %rsi
	orq	$1, %rsi
	decq	%rax
	cmpq	%rax, %rsi
	je	.L645
	movabsq	$-1125899906842624, %rax
	andn	%rax, %rdx, %rax
	je	.L1103
.L177:
	movl	-1065172(%rbp), %eax
	cmpl	$9, %eax
	je	.L178
	jbe	.L1104
	cmpl	$10, %eax
	je	.L176
	subl	$13, %eax
	cmpl	$1, %eax
	ja	.L175
	jmp	.L176
.L1082:
	movabsq	$9222246136947933185, %rsi
	movq	%rdx, %rax
	cmpq	%rsi, %rdx
	je	.L190
	addq	$3, %rsi
	cmpq	%rsi, %rdx
	je	.L190
	movq	%rdx, %rsi
	orq	$1, %rsi
	movabsq	$9222246136947933187, %rax
	cmpq	%rax, %rsi
	je	.L651
	movabsq	$-1125899906842624, %rax
	andn	%rax, %rdx, %rax
	je	.L1105
.L191:
	movl	-1065176(%rbp), %edi
	cmpl	$9, %edi
	je	.L192
	movq	%rdx, %rax
	jbe	.L1106
	cmpl	$10, %edi
	je	.L190
	leal	-13(%rdi), %esi
	cmpl	$1, %esi
	ja	.L189
	jmp	.L190
.L702:
	movl	$15, -1065200(%rbp)
.L474:
	movl	-1065200(%rbp), %esi
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_type@PLT
.L475:
	movq	%rax, -8(%rbx)
	jmp	.L1011
.L387:
	shrb	%al
	movq	-1065048(%rbp), %r12
	movl	%eax, %r15d
	jmp	.L388
.L391:
	movq	-1065112(%rbp), %rdi
	movq	-1065048(%rbp), %rdx
	shrb	%al
	leaq	88(%rdi), %rbx
	leaq	-1065072(%rbp), %r14
	jmp	.L389
.L1050:
	movq	56(%rbx), %rax
	movq	64(%rbx), %r10
	leaq	-1(%rax), %rdi
	movq	%r15, %rax
	andq	%rdi, %rax
	movq	$-1, %r8
.L576:
	leaq	(%rax,%rax,2), %rcx
	leaq	(%r10,%rcx,8), %rcx
	movq	(%rcx), %r9
	movl	16(%rcx), %ecx
	cmpl	$1, %ecx
	je	.L571
	cmpl	$2, %ecx
	je	.L572
	testl	%ecx, %ecx
	je	.L1107
.L574:
	incq	%rax
	andq	%rdi, %rax
	jmp	.L576
.L1062:
	movw	%si, -1065136(%rbp)
	movbew	-1065136(%rbp), %si
	movzwl	%si, %eax
	addq	%rax, %rdx
	jmp	.L461
.L1061:
	subq	$8, %rsi
	movq	%rsi, -1065048(%rbp)
	jmp	.L1012
.L591:
	movq	%rax, %rdx
	shrq	%rdx
	andl	$1, %eax
	orq	%rax, %rdx
	vxorpd	%xmm6, %xmm6, %xmm6
	vcvtsi2sdq	%rdx, %xmm6, %xmm1
	vaddsd	%xmm1, %xmm1, %xmm1
	testq	%rdi, %rdi
	jns	.L1108
.L593:
	movq	%rdi, %rax
	movq	%rdi, %rdx
	shrq	%rax
	andl	$1, %edx
	orq	%rdx, %rax
	vxorpd	%xmm2, %xmm2, %xmm2
	vcvtsi2sdq	%rax, %xmm2, %xmm0
	vaddsd	%xmm0, %xmm0, %xmm0
	jmp	.L594
.L1074:
	movabsq	$1125899906842623, %rcx
	andq	%rcx, %rsi
	cmpl	$1, 12(%rsi)
	jne	.L156
	movq	-8(%rax), %r8
	movq	%r8, %rbx
	andq	%rdx, %rbx
	cmpq	%rdx, %rbx
	je	.L1109
.L314:
	movq	-1065056(%rbp), %rax
	addq	$21, %r12
	movq	%r12, 8(%rax)
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %rsi
	leaq	.LC18(%rip), %rcx
	movl	$22, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L1063:
	movabsq	$1125899906842623, %r13
	andq	%r8, %r13
	cmpl	$9, 12(%r13)
	jne	.L447
	movq	24(%r13), %rax
	movq	40(%rax), %rdx
	testq	%rdx, %rdx
	je	.L450
	xorl	%ebx, %ebx
	jmp	.L449
	.p2align 4,,10
	.p2align 3
.L451:
	incq	%rbx
	cmpq	%rdx, %rbx
	je	.L450
	cmpq	$65535, %rbx
	je	.L337
.L449:
	cmpq	48(%rax,%rbx,8), %r9
	jne	.L451
	cmpq	32(%r13), %rbx
	jnb	.L355
	movq	-1065112(%rbp), %rax
	movslq	%ebx, %r15
	leaq	88(%rax), %rdx
	addq	$4, %r15
	movq	%r10, -1065144(%rbp)
	movq	%rdx, -1065136(%rbp)
	movq	8(%r13,%r15,8), %rcx
	leaq	-1065072(%rbp), %r14
	movq	%r14, %rsi
	movq	%rax, %rdi
	vzeroupper
	call	gab_gc_dref@PLT
	cmpq	32(%r13), %rbx
	jnb	.L445
	movq	-1065144(%rbp), %r10
	movq	%r10, 8(%r13,%r15,8)
	jmp	.L1015
.L1058:
	leaq	-8(%rax), %rdx
	movabsq	$9222246136947933187, %rsi
	movq	%rdx, -1065048(%rbp)
	movq	%rsi, -16(%rax)
	jmp	.L1011
.L1070:
	movabsq	$1125899906842623, %rdx
	andq	%r9, %rdx
	movl	12(%rdx), %r10d
	testl	%r10d, %r10d
	jne	.L359
	movq	-1065056(%rbp), %rdx
	leaq	0(,%r13,8), %r10
	movq	16(%rdx), %r11
	movq	%rax, %r8
	movl	%ecx, %r14d
	subq	%r10, %r8
	movzbl	%dil, %edx
	decb	%r14b
	movzbl	%sil, %ebx
	leaq	(%r11,%rdx,8), %rdx
	sete	%r10b
	cmpb	%sil, %r14b
	je	.L715
	cmpb	$-1, %sil
	je	.L715
	cmpb	%r14b, %sil
	jnb	.L364
	movl	%esi, %r14d
	addl	%r10d, %esi
.L363:
	leal	-1(%r14), %ecx
	testb	%r14b, %r14b
	je	.L367
.L365:
	cmpb	$3, %cl
	jbe	.L368
	movl	$1, %r15d
	subq	%r13, %r15
	leaq	(%rax,%r15,8), %r13
	movq	%rdx, %rax
	subq	%r13, %rax
	cmpq	$16, %rax
	ja	.L1110
.L368:
	leal	1(%rcx), %r14d
	movzbl	%r14b, %r14d
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L371:
	movq	(%r8,%rax,8), %r13
	movq	%r13, (%rdx,%rax,8)
	incq	%rax
	cmpq	%rax, %r14
	jne	.L371
.L372:
	leal	1(%rcx), %eax
	movzbl	%al, %eax
	leaq	(%rdx,%rax,8), %rdx
.L367:
	leal	-1(%r10), %r13d
	testb	%r10b, %r10b
	je	.L373
.L620:
	cmpb	$2, %r13b
	jbe	.L692
	movl	%r10d, %ecx
	shrb	$2, %cl
	movzbl	%cl, %ecx
	movabsq	$9222246136947933185, %r14
	salq	$5, %rcx
	vmovq	%r14, %xmm2
	movq	%rdx, %rax
	addq	%rdx, %rcx
	vpbroadcastq	%xmm2, %ymm0
	.p2align 4,,10
	.p2align 3
.L375:
	vmovdqu	%ymm0, (%rax)
	addq	$32, %rax
	cmpq	%rax, %rcx
	jne	.L375
	movl	%r10d, %r14d
	andl	$-4, %r14d
	movq	%r10, %rax
	movl	%r13d, %ecx
	andl	$252, %eax
	subl	%r14d, %ecx
	andl	$3, %r10d
	leaq	(%rdx,%rax,8), %rax
	je	.L376
.L374:
	movabsq	$9222246136947933185, %r10
	movq	%r10, (%rax)
	testb	%cl, %cl
	je	.L376
	movq	%r10, 8(%rax)
	cmpb	$1, %cl
	je	.L376
	movq	%r10, 16(%rax)
.L376:
	leal	1(%r13), %eax
	movzbl	%al, %eax
	leaq	(%rdx,%rax,8), %rdx
.L373:
	movzbl	%dil, %eax
	addl	%ebx, %eax
	movzbl	%sil, %esi
	cltq
	movq	%rsi, (%rdx)
	movq	%r9, (%r11,%rax,8)
	movq	%r8, -1065048(%rbp)
	leaq	dispatch_table.4(%rip), %rsi
	addq	$5, %r12
	movzbl	-1(%r12), %eax
	movq	%rax, %rbx
	movq	(%rsi,%rax,8), %rax
	jmp	*%rax
.L1066:
	movabsq	$1125899906842623, %rdi
	andq	%r8, %rdi
	cmpl	$9, 12(%rdi)
	jne	.L423
	movq	24(%rdi), %rdx
	xorl	%eax, %eax
	movq	40(%rdx), %rcx
	testq	%rcx, %rcx
	jne	.L425
	jmp	.L1024
	.p2align 4,,10
	.p2align 3
.L427:
	incq	%rax
	cmpq	%rcx, %rax
	je	.L1024
	cmpq	$65535, %rax
	je	.L337
.L425:
	cmpq	48(%rdx,%rax,8), %rsi
	jne	.L427
	cmpq	32(%rdi), %rax
	jnb	.L355
	cltq
	movq	40(%rdi,%rax,8), %rax
.L426:
	movq	%rax, -8(%r9)
	jmp	.L1016
.L1087:
	movq	-1065112(%rbp), %rdi
	call	gab_obj_upvalue_create@PLT
	movq	$0, 40(%rax)
.L581:
	movq	%rax, -1065064(%rbp)
	jmp	.L587
.L1085:
	movq	-1065112(%rbp), %rdi
	call	gab_obj_upvalue_create@PLT
	movq	$0, 40(%rax)
.L560:
	movq	%rax, -1065064(%rbp)
	jmp	.L566
.L1060:
	cmpl	$7, %edi
	jne	.L474
	jmp	.L475
.L1092:
	cmpl	$7, %eax
	jne	.L330
	jmp	.L335
.L1102:
	movq	%r13, %rax
	cmpl	$7, %edi
	jne	.L202
	jmp	.L203
.L1090:
	cmpl	$7, %edi
	jne	.L317
	jmp	.L318
.L1106:
	cmpl	$7, %edi
	jne	.L189
	jmp	.L190
.L1094:
	cmpl	$7, %eax
	jne	.L151
	jmp	.L152
.L1098:
	cmpl	$7, %eax
	jne	.L346
	jmp	.L347
.L1104:
	cmpl	$7, %eax
	jne	.L175
	jmp	.L176
.L1096:
	movq	%r13, %rax
	cmpl	$7, %edi
	jne	.L225
	jmp	.L226
.L1100:
	cmpl	$7, %eax
	jne	.L111
	jmp	.L112
	.p2align 4,,10
	.p2align 3
.L716:
	leal	(%rax,%r9), %r8d
	jmp	.L397
	.p2align 4,,10
	.p2align 3
.L1076:
	cmpq	$-1, %r9
	je	.L120
	leaq	(%r9,%r9,2), %rax
	leaq	(%r8,%rax,8), %rcx
	cmpl	$1, 16(%rcx)
	jne	.L120
.L121:
	movq	8(%rcx), %rax
	movabsq	$9222246136947933185, %rdx
	cmpq	%rdx, %rax
	je	.L120
	cmpq	$0, 48(%r13)
	jne	.L1111
.L635:
	movl	$65535, %esi
.L132:
	movzbl	24(%r13), %edx
	movq	%r11, 5(%r12)
	movb	%dl, 4(%r12)
	movabsq	$9222246136947933189, %rdx
	movq	%rsi, 13(%r12)
	andn	%rdx, %rax, %rdx
	je	.L1112
	movabsq	$-1125899906842624, %rdx
	andn	%rdx, %rax, %rdx
	je	.L1113
.L145:
	movq	-1065112(%rbp), %rdi
	movq	%rax, %r8
	leaq	-1065072(%rbp), %rsi
	leaq	.LC18(%rip), %rcx
	movl	$23, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
	.p2align 4,,10
	.p2align 3
.L117:
	cmpq	$-1, %r9
	cmove	%rax, %r9
	jmp	.L119
.L116:
	cmpq	%r11, %r10
	jne	.L119
	jmp	.L121
	.p2align 4,,10
	.p2align 3
.L1054:
	cmpq	$-1, %rsi
	je	.L610
	leaq	(%rsi,%rsi,2), %rax
	leaq	(%r8,%rax,8), %rdx
	movl	16(%rdx), %eax
	testl	%eax, %eax
	je	.L610
.L611:
	movq	%r15, (%rdx)
	movq	%r12, 8(%rdx)
	movl	$1, 16(%rdx)
	incb	24(%rbx)
	movq	-1065048(%rbp), %rax
	leaq	8(%rax), %rdx
	movq	%rdx, -1065048(%rbp)
	movabsq	$-1125899906842624, %rdx
	orq	%rdx, %rbx
	movq	%rbx, (%rax)
	jmp	.L1008
.L607:
	cmpq	$-1, %rsi
	cmove	%rax, %rsi
	jmp	.L609
.L606:
	cmpq	%rdi, %r15
	jne	.L609
	jmp	.L611
	.p2align 4,,10
	.p2align 3
.L337:
	leaq	__PRETTY_FUNCTION__.3(%rip), %rcx
	movl	$363, %edx
	leaq	.LC2(%rip), %rsi
	leaq	.LC21(%rip), %rdi
	vzeroupper
	call	__assert_fail@PLT
.L1067:
	movl	%eax, %edi
	shrb	$2, %dil
	movzbl	%dil, %edi
	salq	$5, %rdi
	xorl	%edx, %edx
	.p2align 4,,10
	.p2align 3
.L403:
	vmovdqu	(%rsi,%rdx), %ymm2
	vmovdqu	%ymm2, (%r15,%rdx)
	addq	$32, %rdx
	cmpq	%rdx, %rdi
	jne	.L403
	testb	$3, %al
	je	.L406
	movl	%eax, %edx
	andl	$252, %eax
	salq	$3, %rax
	addq	%rax, %rsi
	movq	(%rsi), %rdi
	addq	%r15, %rax
	movq	%rdi, (%rax)
	andl	$-4, %edx
	movl	%r10d, %edi
	subb	%dl, %dil
	je	.L406
	movq	8(%rsi), %rdx
	movq	%rdx, 8(%rax)
	cmpb	$1, %dil
	je	.L406
	movq	16(%rsi), %rdx
	movq	%rdx, 16(%rax)
	jmp	.L406
.L120:
	movabsq	$9222246136947933188, %rsi
	andq	%rdi, %rsi
	movq	%rsi, %rax
	movq	$-1, %r10
.L130:
	leaq	(%rax,%rax,2), %rdx
	leaq	(%r8,%rdx,8), %rcx
	movl	16(%rcx), %edx
	movq	(%rcx), %r9
	cmpl	$1, %edx
	je	.L124
	cmpl	$2, %edx
	je	.L125
	testl	%edx, %edx
	je	.L1114
.L127:
	incq	%rax
	andq	%rdi, %rax
	jmp	.L130
.L124:
	movabsq	$9222246136947933188, %rdx
	cmpq	%rdx, %r9
	jne	.L127
.L129:
	movq	8(%rcx), %rax
	movabsq	$9222246136947933185, %rdx
	cmpq	%rdx, %rax
	je	.L128
	cmpq	$0, 48(%r13)
	je	.L635
	leaq	(%rsi,%rsi,2), %rdx
	leaq	(%r8,%rdx,8), %rcx
	movl	16(%rcx), %edx
	movq	$-1, %r9
	cmpl	$1, %edx
	je	.L133
.L1116:
	cmpl	$2, %edx
	je	.L134
	testl	%edx, %edx
	je	.L1115
.L136:
	incq	%rsi
	andq	%rdi, %rsi
	leaq	(%rsi,%rsi,2), %rdx
	leaq	(%r8,%rdx,8), %rcx
	movl	16(%rcx), %edx
	cmpl	$1, %edx
	jne	.L1116
.L133:
	movabsq	$9222246136947933188, %rdx
	cmpq	%rdx, (%rcx)
	jne	.L136
	jmp	.L132
	.p2align 4,,10
	.p2align 3
.L1114:
	cmpq	$-1, %r10
	je	.L128
	leaq	(%r10,%r10,2), %rax
	leaq	(%r8,%rax,8), %rcx
	cmpl	$1, 16(%rcx)
	je	.L129
.L128:
	movq	-1065056(%rbp), %rax
	addq	$4, %r12
	movq	%r12, 8(%rax)
	movq	-1065112(%rbp), %rdi
	movabsq	$-1125899906842624, %r8
	leaq	-1065072(%rbp), %rsi
	movq	%r11, %r9
	orq	%r13, %r8
	leaq	.LC17(%rip), %rcx
	movl	$29, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L125:
	cmpq	$-1, %r10
	cmove	%rax, %r10
	jmp	.L127
.L398:
	movl	%r8d, %r9d
	subl	%eax, %r9d
	leal	-1(%rax), %r10d
	testb	%al, %al
	jne	.L399
	leal	-1(%r9), %esi
	jmp	.L621
.L610:
	incq	48(%rbx)
	jmp	.L611
.L1107:
	cmpq	$-1, %r8
	je	.L570
	leaq	(%r8,%r8,2), %rcx
	cmpl	$1, 16(%r10,%rcx,8)
	movq	%r8, %rax
	jne	.L570
.L575:
	cmpw	$-1, %ax
	je	.L570
	movq	%r13, 8(%rdx)
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %rsi
	leaq	.LC16(%rip), %rcx
	movl	$28, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
	.p2align 4,,10
	.p2align 3
.L572:
	cmpq	$-1, %r8
	cmove	%rax, %r8
	jmp	.L574
.L571:
	cmpq	%r9, %r15
	jne	.L574
	jmp	.L575
	.p2align 4,,10
	.p2align 3
.L1065:
	movq	40(%rdx), %rax
	testq	%rax, %rax
	je	.L450
	xorl	%ebx, %ebx
	jmp	.L440
	.p2align 4,,10
	.p2align 3
.L442:
	incq	%rbx
	cmpq	%rax, %rbx
	je	.L450
	cmpq	$65535, %rbx
	je	.L337
.L440:
	cmpq	48(%rdx,%rbx,8), %r9
	jne	.L442
	movb	$122, -1(%r12)
	movl	%ebx, %eax
	jmp	.L443
.L1023:
	movl	$-1, %eax
	movl	$-1, %edx
	jmp	.L416
.L1024:
	movabsq	$9222246136947933185, %rax
	jmp	.L426
.L98:
	movq	%rdi, -16456(%rbp)
	movzbl	24(%r8), %esi
	jmp	.L626
.L261:
	vsubsd	%xmm0, %xmm1, %xmm0
	vcvttsd2siq	%xmm0, %rsi
	btcq	$63, %rsi
	jmp	.L262
.L259:
	vsubsd	%xmm0, %xmm1, %xmm1
	vcvttsd2siq	%xmm1, %rax
	btcq	$63, %rax
	jmp	.L260
.L285:
	vsubsd	%xmm0, %xmm1, %xmm0
	vcvttsd2siq	%xmm0, %rdx
	btcq	$63, %rdx
	jmp	.L286
.L283:
	vsubsd	%xmm0, %xmm1, %xmm1
	vcvttsd2siq	%xmm1, %rax
	btcq	$63, %rax
	jmp	.L284
.L590:
	leaq	(%rdi,%rdi), %rcx
	addq	%rcx, %rdi
	salq	$4, %rdi
	movl	$1, %esi
	movq	%rcx, -1065136(%rbp)
	call	calloc@PLT
	movq	%rax, -1065088(%rbp)
	movq	-1065136(%rbp), %rcx
	movq	%rax, %r8
	movq	56(%rbx), %rax
	movq	%rcx, -1065096(%rbp)
	movq	$0, -1065104(%rbp)
	movq	$0, 48(%rbx)
	movq	64(%rbx), %r9
	leaq	-1(%rcx), %r14
	testq	%rax, %rax
	je	.L605
	leaq	(%rax,%rax,2), %rax
	leaq	(%r9,%rax,8), %rdx
	movq	%rdx, -1065136(%rbp)
	movq	%r13, -1065144(%rbp)
	movq	%r9, %rsi
	movq	%r12, %rdi
	jmp	.L604
	.p2align 4,,10
	.p2align 3
.L597:
	addq	$24, %rsi
	cmpq	%rsi, -1065136(%rbp)
	je	.L1117
.L604:
	cmpl	$1, 16(%rsi)
	jne	.L597
	movq	(%rsi), %r11
	movq	$-1, %r12
	movq	%r11, %rax
	andq	%r14, %rax
.L603:
	leaq	(%rax,%rax,2), %rdx
	leaq	(%r8,%rdx,8), %r10
	movl	16(%r10), %edx
	movq	(%r10), %r13
	cmpl	$1, %edx
	je	.L598
	cmpl	$2, %edx
	je	.L599
	testl	%edx, %edx
	je	.L1118
.L601:
	incq	%rax
	andq	%r14, %rax
	jmp	.L603
.L598:
	cmpq	%r13, %r11
	jne	.L601
.L602:
	vmovq	%r11, %xmm4
	vpinsrq	$1, 8(%rsi), %xmm4, %xmm0
	movl	$1, 16(%r10)
	vmovdqu	%xmm0, (%r10)
	incq	48(%rbx)
	jmp	.L597
.L1118:
	cmpq	$-1, %r12
	je	.L602
	leaq	(%r12,%r12,2), %rax
	leaq	(%r8,%rax,8), %r10
	jmp	.L602
.L599:
	cmpq	$-1, %r12
	cmove	%rax, %r12
	jmp	.L601
.L1117:
	movq	-1065144(%rbp), %r13
	movq	%rdi, %r12
.L605:
	movq	%r9, %rdi
	movq	%rcx, -1065144(%rbp)
	movq	%r8, -1065136(%rbp)
	call	free@PLT
	movq	-1065136(%rbp), %r8
	movq	-1065144(%rbp), %rcx
	movq	%r8, 64(%rbx)
	movq	%rcx, 56(%rbx)
	jmp	.L595
.L269:
	vsubsd	%xmm0, %xmm1, %xmm0
	vcvttsd2siq	%xmm0, %rdx
	btcq	$63, %rdx
	jmp	.L270
.L267:
	vsubsd	%xmm0, %xmm1, %xmm1
	vcvttsd2siq	%xmm1, %rax
	btcq	$63, %rax
	jmp	.L268
.L628:
	movq	%rax, %r10
	xorl	%esi, %esi
	xorl	%edx, %edx
	jmp	.L99
.L293:
	vsubsd	%xmm0, %xmm1, %xmm0
	vcvttsd2siq	%xmm0, %rdx
	btcq	$63, %rdx
	jmp	.L294
.L291:
	vsubsd	%xmm0, %xmm1, %xmm1
	vcvttsd2siq	%xmm1, %rax
	btcq	$63, %rax
	jmp	.L292
.L277:
	vsubsd	%xmm0, %xmm1, %xmm0
	vcvttsd2siq	%xmm0, %rdx
	btcq	$63, %rdx
	jmp	.L278
.L275:
	vsubsd	%xmm0, %xmm1, %xmm1
	vcvttsd2siq	%xmm1, %rax
	btcq	$63, %rax
	jmp	.L276
.L421:
	cmpw	$-1, %si
	je	.L696
	movw	%si, -1065136(%rbp)
	movbew	-1065136(%rbp), %si
	movzwl	%si, %edx
	movzwl	%si, %esi
	cmpq	32(%rax), %rsi
	jnb	.L355
	movq	40(%rax,%rdx,8), %rax
.L422:
	movq	%rax, -8(%r10)
	jmp	.L1016
.L1069:
	movabsq	$9222246136947933185, %rax
	movq	%rax, (%r10)
	movl	$1, %r9d
	xorl	%r11d, %r11d
	jmp	.L381
.L287:
	movq	%rax, %rdx
	shrq	%rdx
	andl	$1, %eax
	orq	%rax, %rdx
	vxorpd	%xmm2, %xmm2, %xmm2
	vcvtsi2sdq	%rdx, %xmm2, %xmm0
	vaddsd	%xmm0, %xmm0, %xmm0
	jmp	.L296
.L263:
	movq	%rdx, %rax
	shrq	%rax
	andl	$1, %edx
	orq	%rdx, %rax
	vxorpd	%xmm5, %xmm5, %xmm5
	vcvtsi2sdq	%rax, %xmm5, %xmm0
	vaddsd	%xmm0, %xmm0, %xmm0
	jmp	.L296
.L295:
	movq	%rax, %rdx
	shrq	%rdx
	andl	$1, %eax
	orq	%rax, %rdx
	vxorpd	%xmm7, %xmm7, %xmm7
	vcvtsi2sdq	%rdx, %xmm7, %xmm0
	vaddsd	%xmm0, %xmm0, %xmm0
	jmp	.L296
.L271:
	movq	%rax, %rdx
	shrq	%rdx
	andl	$1, %eax
	orq	%rax, %rdx
	vxorpd	%xmm4, %xmm4, %xmm4
	vcvtsi2sdq	%rdx, %xmm4, %xmm0
	vaddsd	%xmm0, %xmm0, %xmm0
	jmp	.L296
.L279:
	movq	%rax, %rdx
	shrq	%rdx
	andl	$1, %eax
	orq	%rax, %rdx
	vxorpd	%xmm3, %xmm3, %xmm3
	vcvtsi2sdq	%rdx, %xmm3, %xmm0
	vaddsd	%xmm0, %xmm0, %xmm0
	jmp	.L296
.L207:
	movq	-1065128(%rbp), %rdi
	movq	%rsi, %rdx
	subq	%rdi, %rdx
	movzbl	41(%r8), %eax
	cmpq	$16320, %rdx
	jg	.L379
	movq	-1065048(%rbp), %rdx
	movq	-1065120(%rbp), %r11
	movq	%rdx, %rdi
	decl	%eax
	subq	%r11, %rdi
	subl	%r12d, %eax
	sarq	$3, %rdi
	movzbl	%al, %eax
	addq	%rdi, %rax
	cmpq	$131071, %rax
	jg	.L379
	leaq	32(%rsi), %rax
	movq	%rax, -1065056(%rbp)
	movq	32(%r8), %rax
	movq	%r13, 32(%rsi)
	movq	40(%rax), %rdi
	movb	%cl, 56(%rsi)
	movq	%rdi, 40(%rsi)
	cmpb	%r12b, %bl
	jnb	.L214
	movl	%r12d, %r11d
	subl	%ebx, %r11d
	leal	-1(%r11), %r10d
	cmpb	$2, %r10b
	jbe	.L663
	leal	-4(%r11), %ecx
	movabsq	$9222246136947933185, %r9
	shrb	$2, %cl
	vmovq	%r9, %xmm5
	incl	%ecx
	xorl	%eax, %eax
	vpbroadcastq	%xmm5, %ymm0
.L216:
	movq	%rax, %r9
	salq	$5, %r9
	incq	%rax
	vmovdqu	%ymm0, (%rdx,%r9)
	cmpb	%cl, %al
	jb	.L216
	sall	$2, %ecx
	movzbl	%cl, %eax
	addl	%ecx, %ebx
	leaq	(%rdx,%rax,8), %rax
	cmpb	%r11b, %cl
	je	.L217
.L215:
	movabsq	$9222246136947933185, %rcx
	leal	1(%rbx), %r9d
	movq	%rcx, (%rax)
	cmpb	%r12b, %r9b
	jnb	.L217
	addl	$2, %ebx
	movq	%rcx, 8(%rax)
	cmpb	%r12b, %bl
	jnb	.L217
	movq	%rcx, 16(%rax)
.L217:
	movzbl	%r10b, %eax
	salq	$3, %rax
	leaq	(%rdx,%rax), %rcx
	leaq	8(%rdx,%rax), %rdx
	movabsq	$9222246136947933185, %rax
	movq	%rdx, -1065048(%rbp)
	movq	%rax, (%rcx)
	movzbl	%r12b, %r15d
.L218:
	salq	$3, %r15
	movq	%rdx, %rax
	subq	%r15, %rax
	subq	$8, %rax
	movq	%rax, 48(%rsi)
	leal	1(%r12), %r9d
	movzbl	40(%r8), %r10d
	cmpb	%r10b, %r9b
	jnb	.L209
	movl	%r10d, %eax
	subl	%r12d, %eax
	leal	-2(%rax), %r8d
	leal	-1(%rax), %r11d
	cmpb	$2, %r8b
	jbe	.L664
	subl	$5, %eax
	movabsq	$9222246136947933185, %rsi
	shrb	$2, %al
	vmovq	%rsi, %xmm6
	incl	%eax
	xorl	%ecx, %ecx
	vpbroadcastq	%xmm6, %ymm0
.L221:
	movq	%rcx, %rsi
	salq	$5, %rsi
	incq	%rcx
	vmovdqu	%ymm0, (%rdx,%rsi)
	cmpb	%al, %cl
	jb	.L221
	sall	$2, %eax
	movzbl	%al, %ecx
	addl	%eax, %r9d
	leaq	(%rdx,%rcx,8), %rcx
	cmpb	%r11b, %al
	je	.L222
.L220:
	movabsq	$9222246136947933185, %rsi
	leal	1(%r9), %eax
	movq	%rsi, (%rcx)
	cmpb	%r10b, %al
	jnb	.L222
	leal	2(%r9), %eax
	movq	%rsi, 8(%rcx)
	cmpb	%r10b, %al
	jnb	.L222
	movq	%rsi, 16(%rcx)
.L222:
	movzbl	%r8b, %r8d
	leaq	8(,%r8,8), %rax
	leaq	(%rdx,%rax), %rcx
	movabsq	$9222246136947933185, %rsi
	movq	%rcx, -1065048(%rbp)
	movq	%rsi, -8(%rdx,%rax)
	jmp	.L209
.L157:
	movq	-1065128(%rbp), %rdi
	movq	%rdx, %rcx
	subq	%rdi, %rcx
	movzbl	41(%rsi), %eax
	cmpq	$16320, %rcx
	jg	.L379
	movq	-1065048(%rbp), %rdi
	movq	-1065120(%rbp), %r11
	movq	%rdi, %rcx
	decl	%eax
	subq	%r11, %rcx
	subl	%r12d, %eax
	sarq	$3, %rcx
	movzbl	%al, %eax
	addq	%rcx, %rax
	cmpq	$131071, %rax
	jg	.L379
	leaq	32(%rdx), %rax
	movq	%rax, -1065056(%rbp)
	movq	32(%rsi), %rax
	movq	%r13, 32(%rdx)
	movq	40(%rax), %rcx
	movb	%r15b, 56(%rdx)
	movq	%rcx, 40(%rdx)
	cmpb	%r12b, %bl
	jnb	.L164
	movl	%r12d, %r9d
	subl	%ebx, %r9d
	leal	-1(%r9), %eax
	cmpb	$2, %al
	jbe	.L641
	leal	-4(%r9), %r8d
	movabsq	$9222246136947933185, %r10
	shrb	$2, %r8b
	vmovq	%r10, %xmm4
	incl	%r8d
	xorl	%eax, %eax
	vpbroadcastq	%xmm4, %ymm0
.L166:
	movq	%rax, %r10
	salq	$5, %r10
	incq	%rax
	vmovdqu	%ymm0, (%rdi,%r10)
	cmpb	%r8b, %al
	jb	.L166
	sall	$2, %r8d
	movzbl	%r8b, %eax
	addl	%r8d, %ebx
	leaq	(%rdi,%rax,8), %rax
	cmpb	%r9b, %r8b
	je	.L167
.L165:
	movabsq	$9222246136947933185, %r8
	leal	1(%rbx), %r10d
	movq	%r8, (%rax)
	cmpb	%r12b, %r10b
	jnb	.L167
	addl	$2, %ebx
	movq	%r8, 8(%rax)
	cmpb	%r12b, %bl
	jnb	.L167
	movq	%r8, 16(%rax)
.L167:
	movzbl	%r9b, %eax
	salq	$3, %rax
	leaq	(%rdi,%rax), %r8
	movabsq	$9222246136947933185, %rbx
	movq	%r8, -1065048(%rbp)
	movq	%rbx, -8(%rdi,%rax)
	movzbl	%r12b, %r14d
.L168:
	salq	$3, %r14
	movq	%r8, %rax
	subq	%r14, %rax
	subq	$8, %rax
	movq	%rax, 48(%rdx)
	leal	1(%r12), %edi
	movzbl	40(%rsi), %r10d
	cmpb	%r10b, %dil
	jnb	.L159
	movl	%r10d, %eax
	subl	%r12d, %eax
	leal	-2(%rax), %r9d
	leal	-1(%rax), %r11d
	cmpb	$2, %r9b
	jbe	.L643
	subl	$5, %eax
	movabsq	$9222246136947933185, %rsi
	shrb	$2, %al
	vmovq	%rsi, %xmm3
	incl	%eax
	xorl	%edx, %edx
	vpbroadcastq	%xmm3, %ymm0
.L171:
	movq	%rdx, %rsi
	salq	$5, %rsi
	incq	%rdx
	vmovdqu	%ymm0, (%r8,%rsi)
	cmpb	%al, %dl
	jb	.L171
	sall	$2, %eax
	movzbl	%al, %edx
	addl	%eax, %edi
	leaq	(%r8,%rdx,8), %rdx
	cmpb	%r11b, %al
	je	.L172
.L170:
	movabsq	$9222246136947933185, %rsi
	leal	1(%rdi), %eax
	movq	%rsi, (%rdx)
	cmpb	%r10b, %al
	jnb	.L172
	leal	2(%rdi), %eax
	movq	%rsi, 8(%rdx)
	cmpb	%r10b, %al
	jnb	.L172
	movq	%rsi, 16(%rdx)
.L172:
	movzbl	%r9b, %eax
	leaq	8(%r8,%rax,8), %rdx
	movabsq	$9222246136947933185, %rdi
	movq	%rdx, -1065048(%rbp)
	movq	%rdi, (%r8,%rax,8)
	jmp	.L159
.L629:
	movq	%rax, %rcx
	jmp	.L104
.L1088:
	movabsq	$-1125899906842624, %rax
	orq	%r14, %rax
	jmp	.L588
.L1112:
	movzbl	%ah, %ebx
.L144:
	movzbl	%bl, %eax
	leaq	dispatch_table.4(%rip), %rdi
	movb	%bl, -1(%r12)
	jmp	*(%rdi,%rax,8)
.L1086:
	movabsq	$-1125899906842624, %rax
	orq	%rbx, %rax
	jmp	.L567
.L1033:
	addq	$21, %r12
.L1029:
	movq	-1065056(%rbp), %rax
	movq	-1065112(%rbp), %rdi
	movq	%r12, 8(%rax)
	leaq	-1065072(%rbp), %rsi
	leaq	.LC18(%rip), %rcx
	movl	$20, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L379:
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %rsi
	leaq	.LC16(%rip), %rcx
	movl	$26, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L1111:
	movq	$-1, %rcx
.L142:
	leaq	(%rsi,%rsi,2), %rdx
	leaq	(%r8,%rdx,8), %r9
	movl	16(%r9), %edx
	cmpl	$1, %edx
	je	.L138
	cmpl	$2, %edx
	je	.L139
	testl	%edx, %edx
	je	.L1119
.L141:
	incq	%rsi
	andq	%rdi, %rsi
	jmp	.L142
.L708:
	movq	%rax, (%rdx)
	movl	$1, %esi
	addq	$8, %rdx
	jmp	.L182
.L709:
	movq	%rax, (%rdx)
	movl	$1, %edi
	addq	$8, %rdx
	jmp	.L196
.L715:
	leal	(%r14,%r10), %esi
	jmp	.L363
.L205:
	movabsq	$1125899906842623, %rsi
	andq	%r13, %rsi
	movabsq	$-1125899906842624, %rax
	orq	24(%rsi), %rax
	jmp	.L203
.L192:
	movabsq	$1125899906842623, %rsi
	andq	%rdx, %rsi
	movabsq	$-1125899906842624, %rax
	orq	24(%rsi), %rax
	jmp	.L190
.L320:
	movabsq	$1125899906842623, %rdx
	andq	%rax, %rdx
	movabsq	$-1125899906842624, %rax
	orq	24(%rdx), %rax
	jmp	.L318
.L114:
	movabsq	$1125899906842623, %rax
	andq	%r11, %rax
	movabsq	$-1125899906842624, %r11
	orq	24(%rax), %r11
	jmp	.L112
.L349:
	movabsq	$1125899906842623, %rax
	andq	%rbx, %rax
	movabsq	$-1125899906842624, %rdx
	orq	24(%rax), %rdx
	jmp	.L347
.L178:
	movabsq	$1125899906842623, %rsi
	andq	%rdx, %rsi
	movabsq	$-1125899906842624, %rax
	orq	24(%rsi), %rax
	movq	%rax, %rdx
	jmp	.L176
.L228:
	movabsq	$1125899906842623, %rsi
	andq	%r13, %rsi
	movabsq	$-1125899906842624, %rax
	orq	24(%rsi), %rax
	jmp	.L226
.L477:
	movabsq	$1125899906842623, %rdx
	andq	%rax, %rdx
	movabsq	$-1125899906842624, %rax
	orq	24(%rdx), %rax
	jmp	.L475
.L154:
	movabsq	$1125899906842623, %rax
	andq	%rsi, %rax
	movabsq	$-1125899906842624, %rsi
	orq	24(%rax), %rsi
	jmp	.L152
.L333:
	movabsq	$1125899906842623, %rsi
	movq	%r15, %rax
	andq	%rsi, %rax
	movq	24(%rax), %rax
	movabsq	$-1125899906842624, %rdi
	orq	%rax, %rdi
	andq	%rsi, %rax
	jmp	.L331
.L710:
	leal	(%r12,%rbx), %r8d
	jmp	.L235
	.p2align 4,,10
	.p2align 3
.L1083:
	leal	-2(%rbx), %edi
	movq	%rax, (%rdx)
	leaq	8(%rdx), %rsi
	leal	-1(%rbx), %r8d
	cmpb	$2, %dil
	jbe	.L655
	leal	-5(%rbx), %eax
	movabsq	$9222246136947933185, %r9
	shrb	$2, %al
	vmovq	%r9, %xmm5
	incl	%eax
	addq	%r12, %rcx
	xorl	%edx, %edx
	vpbroadcastq	%xmm5, %ymm0
.L198:
	movq	%rdx, %r9
	salq	$5, %r9
	incq	%rdx
	vmovdqu	%ymm0, (%rcx,%r9)
	cmpb	%al, %dl
	jb	.L198
	sall	$2, %eax
	cmpb	%r8b, %al
	je	.L199
	movzbl	%al, %edx
	leaq	(%rsi,%rdx,8), %rdx
	subl	%eax, %edi
.L197:
	movabsq	$9222246136947933185, %rax
	movq	%rax, (%rdx)
	testb	%dil, %dil
	je	.L199
	movq	%rax, 8(%rdx)
	cmpb	$1, %dil
	je	.L199
	movq	%rax, 16(%rdx)
.L199:
	leal	-1(%rbx), %eax
	movzbl	%al, %eax
	movzbl	%bl, %edi
	leaq	(%rsi,%rax,8), %rdx
	jmp	.L196
.L1081:
	leal	-2(%rbx), %edi
	movq	%rax, (%rdx)
	leaq	8(%rdx), %rsi
	leal	-1(%rbx), %r9d
	cmpb	$2, %dil
	jbe	.L647
	leal	-5(%rbx), %eax
	movabsq	$9222246136947933185, %r8
	shrb	$2, %al
	vmovq	%r8, %xmm4
	incl	%eax
	subq	%r15, %rcx
	xorl	%edx, %edx
	vpbroadcastq	%xmm4, %ymm0
.L184:
	movq	%rdx, %r8
	salq	$5, %r8
	incq	%rdx
	vmovdqu	%ymm0, (%rcx,%r8)
	cmpb	%al, %dl
	jb	.L184
	sall	$2, %eax
	cmpb	%r9b, %al
	je	.L185
	movzbl	%al, %edx
	leaq	(%rsi,%rdx,8), %rdx
	subl	%eax, %edi
.L183:
	movabsq	$9222246136947933185, %rax
	movq	%rax, (%rdx)
	testb	%dil, %dil
	je	.L185
	movq	%rax, 8(%rdx)
	cmpb	$1, %dil
	je	.L185
	movq	%rax, 16(%rdx)
.L185:
	leal	-1(%rbx), %eax
	movzbl	%al, %eax
	leaq	(%rsi,%rax,8), %rdx
	movzbl	%bl, %esi
	jmp	.L182
.L647:
	movq	%rsi, %rdx
	jmp	.L183
.L655:
	movq	%rsi, %rdx
	jmp	.L197
.L694:
	movl	%esi, %edi
	movq	%r15, %rax
	jmp	.L408
.L1115:
	movl	$65535, %esi
	cmpq	$-1, %r9
	je	.L132
	leaq	(%r9,%r9,2), %rdx
	cmpl	$1, 16(%r8,%rdx,8)
	cmove	%r9, %rsi
	jmp	.L132
.L1109:
	andq	%r8, %rcx
	cmpl	$1, 12(%rcx)
	movq	%rcx, %rdx
	jne	.L314
	subq	$16, %rax
	movq	%rax, -1065048(%rbp)
	movq	-1065112(%rbp), %rdi
	vzeroupper
	call	gab_obj_string_concat@PLT
	movq	-1065048(%rbp), %rdx
	orq	%rbx, %rax
	leaq	8(%rdx), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rax, (%rdx)
	movq	$1, 8(%rdx)
	jmp	.L1017
.L134:
	cmpq	$-1, %r9
	cmove	%rsi, %r9
	jmp	.L136
.L582:
	movq	-1065112(%rbp), %rdi
	call	gab_obj_upvalue_create@PLT
	movq	$0, 40(%rax)
	jmp	.L586
.L561:
	movq	-1065112(%rbp), %rdi
	call	gab_obj_upvalue_create@PLT
	movq	$0, 40(%rax)
	jmp	.L565
.L1113:
	movabsq	$1125899906842623, %rdx
	andq	%rax, %rdx
	movl	12(%rdx), %edx
	cmpl	$5, %edx
	je	.L1120
	cmpl	$4, %edx
	jne	.L145
	addl	$2, %ebx
	jmp	.L144
.L1110:
	movl	%r14d, %r13d
	shrb	$2, %r13b
	movzbl	%r13b, %r13d
	salq	$5, %r13
	xorl	%eax, %eax
.L369:
	vmovdqu	(%r8,%rax), %ymm5
	vmovdqu	%ymm5, (%rdx,%rax)
	addq	$32, %rax
	cmpq	%r13, %rax
	jne	.L369
	testb	$3, %r14b
	je	.L372
	movq	%r14, %rax
	andl	$252, %eax
	salq	$3, %rax
	movl	%r14d, %r13d
	leaq	(%r8,%rax), %r14
	movq	(%r14), %r15
	addq	%rdx, %rax
	movq	%r15, (%rax)
	andl	$-4, %r13d
	movl	%ecx, %r15d
	subb	%r13b, %r15b
	je	.L372
	movq	8(%r14), %r13
	movq	%r13, 8(%rax)
	cmpb	$1, %r15b
	je	.L372
	movq	16(%r14), %r13
	movq	%r13, 16(%rax)
	jmp	.L372
.L352:
	movq	-1065048(%rbp), %rax
	movabsq	$9222246136947933185, %rsi
	leaq	-8(%rax), %rcx
	movq	%rcx, -1065048(%rbp)
	movq	%rsi, -16(%rax)
	jmp	.L357
.L706:
	xorl	%ecx, %ecx
	jmp	.L577
.L704:
	xorl	%ecx, %ecx
	jmp	.L556
.L1119:
	movl	$65535, %esi
	cmpq	$-1, %rcx
	je	.L132
	leaq	(%rcx,%rcx,2), %rdx
	cmpl	$1, 16(%r8,%rdx,8)
	cmove	%rcx, %rsi
	jmp	.L132
.L139:
	cmpq	$-1, %rcx
	cmove	%rsi, %rcx
	jmp	.L141
.L138:
	cmpq	(%r9), %r11
	jne	.L141
	jmp	.L132
	.p2align 4,,10
	.p2align 3
.L214:
	cmpb	%bl, %r12b
	jnb	.L218
	subl	%r12d, %ebx
	leal	-1(%rbx), %eax
	movzbl	%al, %eax
	notq	%rax
	leaq	(%rdx,%rax,8), %rdx
	movq	%rdx, -1065048(%rbp)
	movzbl	%r12b, %r15d
	jmp	.L218
.L696:
	movabsq	$9222246136947933185, %rax
	jmp	.L422
.L681:
	movq	%r15, %rdi
	movl	$1, %eax
	jmp	.L331
.L164:
	cmpb	%bl, %r12b
	jnb	.L642
	subl	%r12d, %ebx
	leal	-1(%rbx), %eax
	movzbl	%al, %eax
	notq	%rax
	leaq	(%rdi,%rax,8), %r8
	movq	%r8, -1065048(%rbp)
	movzbl	%r12b, %r14d
	jmp	.L168
.L682:
	movq	%r15, %rdi
	movl	$4, %eax
	jmp	.L331
.L364:
	movl	%esi, %r10d
	subl	%r14d, %r10d
	subl	$2, %ecx
	testb	%r14b, %r14b
	jne	.L365
	leal	-1(%r10), %r13d
	jmp	.L620
.L1078:
	subq	%rcx, %rax
	movl	%ebx, %ecx
	shrb	$2, %cl
	movzbl	%cl, %ecx
	leaq	-8(%r11,%rax,8), %r11
	salq	$5, %rcx
	xorl	%eax, %eax
.L241:
	vmovdqu	(%r10,%rax), %ymm7
	vmovdqu	%ymm7, (%r11,%rax)
	addq	$32, %rax
	cmpq	%rcx, %rax
	jne	.L241
	movq	%rbx, %rax
	movl	%ebx, %ecx
	andl	$252, %eax
	salq	$3, %rax
	andl	$-4, %ecx
	movl	%r13d, %r11d
	addq	%rax, %r10
	subl	%ecx, %r11d
	addq	%rsi, %rax
	andl	$3, %ebx
	je	.L244
	movq	(%r10), %rcx
	movq	%rcx, (%rax)
	testb	%r11b, %r11b
	je	.L244
	movq	8(%r10), %rcx
	movq	%rcx, 8(%rax)
	cmpb	$1, %r11b
	je	.L244
	movq	16(%r10), %rcx
	movq	%rcx, 16(%rax)
	jmp	.L244
.L677:
	movl	$16, -1065188(%rbp)
	jmp	.L317
.L631:
	movl	$16, -1065164(%rbp)
	jmp	.L111
.L668:
	movl	$16, -1065184(%rbp)
	jmp	.L225
.L703:
	movl	$16, -1065200(%rbp)
	jmp	.L474
.L659:
	movl	$16, -1065180(%rbp)
	jmp	.L202
.L645:
	movl	$16, -1065172(%rbp)
	jmp	.L175
.L639:
	movl	$16, -1065168(%rbp)
	jmp	.L151
.L688:
	movl	$16, -1065196(%rbp)
	jmp	.L346
.L683:
	movl	$16, -1065192(%rbp)
	jmp	.L330
.L651:
	movl	$16, -1065176(%rbp)
	jmp	.L189
.L1097:
	movq	%rsi, %rax
	andq	%rbx, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065196(%rbp)
	jmp	.L348
.L1091:
	movabsq	$1125899906842623, %rax
	andq	%r15, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065192(%rbp)
	jmp	.L332
.L1103:
	movabsq	$1125899906842623, %rax
	andq	%rdx, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065172(%rbp)
	jmp	.L177
.L1093:
	movabsq	$1125899906842623, %rax
	andq	%rsi, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065168(%rbp)
	jmp	.L153
.L1059:
	movabsq	$1125899906842623, %rdx
	andq	%rax, %rdx
	movl	12(%rdx), %esi
	movl	%esi, -1065200(%rbp)
	jmp	.L476
.L1099:
	movabsq	$1125899906842623, %rax
	andq	%r11, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065164(%rbp)
	jmp	.L113
.L1095:
	movabsq	$1125899906842623, %rax
	andq	%r13, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065184(%rbp)
	jmp	.L227
.L236:
	movl	%r8d, %r12d
	subl	%ebx, %r12d
	leal	-1(%rbx), %r13d
	testb	%bl, %bl
	jne	.L237
	leal	-1(%r12), %r10d
	jmp	.L619
.L1101:
	movabsq	$1125899906842623, %rax
	andq	%r13, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065180(%rbp)
	jmp	.L204
.L1105:
	movabsq	$1125899906842623, %rax
	andq	%rdx, %rax
	movl	12(%rax), %eax
	movl	%eax, -1065176(%rbp)
	jmp	.L191
.L1120:
	incl	%ebx
	jmp	.L144
.L1089:
	andq	%rax, %rsi
	movl	12(%rsi), %edi
	movl	%edi, -1065188(%rbp)
	jmp	.L319
.L1068:
	movq	-8(%r15), %rcx
	movq	-1065112(%rbp), %rdi
	movq	%rbx, %rdx
	movq	%r14, %rsi
	vzeroupper
	call	gab_gc_iref@PLT
	movq	-1065048(%rbp), %rax
	movq	-8(%rax), %rax
	jmp	.L90
.L450:
	movq	%rsi, 8(%rcx)
.L1027:
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %rsi
	leaq	.LC23(%rip), %rcx
	movl	$27, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L640:
	movq	%rsi, %rcx
	jmp	.L160
.L692:
	movl	%r13d, %ecx
	movq	%rdx, %rax
	jmp	.L374
.L662:
	movq	%rsi, %rcx
	jmp	.L210
.L1005:
	vzeroupper
.L109:
	call	v_gab_constant_val_at.part.0
.L434:
	movq	%rsi, 8(%rdi)
	jmp	.L1027
.L641:
	movq	%rdi, %rax
	jmp	.L165
.L643:
	movq	%r8, %rdx
	jmp	.L170
.L663:
	movq	%rdx, %rax
	jmp	.L215
.L327:
	movq	-1065112(%rbp), %rdi
	leaq	-1065072(%rbp), %rsi
	movq	%r9, %r8
	leaq	.LC18(%rip), %rcx
	movl	$27, %edx
	xorl	%eax, %eax
	vzeroupper
	call	vm_error
	jmp	.L90
.L664:
	movq	%rdx, %rcx
	jmp	.L220
.L671:
	movl	%r10d, %r11d
	movq	%rsi, %rax
	jmp	.L246
.L445:
	call	gab_obj_record_set.part.0
.L1064:
	call	__stack_chk_fail@PLT
.L642:
	movq	%rdi, %r8
	jmp	.L168
.L355:
	vzeroupper
	call	gab_obj_record_get.part.0
.L693:
	movl	%r11d, %r12d
	movq	%r10, %r8
	jmp	.L382
	.cfi_endproc
.LFE245:
	.size	gab_vm_run, .-gab_vm_run
	.section	.rodata
	.align 16
	.type	__PRETTY_FUNCTION__.1, @object
	.size	__PRETTY_FUNCTION__.1, 19
__PRETTY_FUNCTION__.1:
	.string	"gab_obj_record_set"
	.align 16
	.type	__PRETTY_FUNCTION__.2, @object
	.size	__PRETTY_FUNCTION__.2, 19
__PRETTY_FUNCTION__.2:
	.string	"gab_obj_record_get"
	.align 16
	.type	__PRETTY_FUNCTION__.3, @object
	.size	__PRETTY_FUNCTION__.3, 19
__PRETTY_FUNCTION__.3:
	.string	"gab_obj_shape_find"
	.section	.data.rel.ro.local,"aw"
	.align 32
	.type	dispatch_table.4, @object
	.size	dispatch_table.4, 2048
dispatch_table.4:
	.quad	.L454
	.quad	.L457
	.quad	.L456
	.quad	.L479
	.quad	.L458
	.quad	.L467
	.quad	.L468
	.quad	.L465
	.quad	.L466
	.quad	.L471
	.quad	.L469
	.quad	.L482
	.quad	.L483
	.quad	.L459
	.quad	.L462
	.quad	.L473
	.quad	.L493
	.quad	.L494
	.quad	.L495
	.quad	.L496
	.quad	.L497
	.quad	.L498
	.quad	.L499
	.quad	.L500
	.quad	.L501
	.quad	.L502
	.quad	.L503
	.quad	.L504
	.quad	.L505
	.quad	.L506
	.quad	.L507
	.quad	.L508
	.quad	.L509
	.quad	.L510
	.quad	.L484
	.quad	.L485
	.quad	.L486
	.quad	.L487
	.quad	.L488
	.quad	.L489
	.quad	.L490
	.quad	.L491
	.quad	.L492
	.quad	.L520
	.quad	.L521
	.quad	.L522
	.quad	.L523
	.quad	.L524
	.quad	.L525
	.quad	.L526
	.quad	.L527
	.quad	.L528
	.quad	.L511
	.quad	.L512
	.quad	.L513
	.quad	.L514
	.quad	.L515
	.quad	.L516
	.quad	.L517
	.quad	.L518
	.quad	.L519
	.quad	.L529
	.quad	.L530
	.quad	.L531
	.quad	.L532
	.quad	.L533
	.quad	.L534
	.quad	.L535
	.quad	.L536
	.quad	.L537
	.quad	.L390
	.quad	.L386
	.quad	.L538
	.quad	.L539
	.quad	.L540
	.quad	.L541
	.quad	.L543
	.quad	.L544
	.quad	.L542
	.quad	.L91
	.quad	.L149
	.quad	.L173
	.quad	.L313
	.quad	.L249
	.quad	.L251
	.quad	.L255
	.quad	.L253
	.quad	.L257
	.quad	.L297
	.quad	.L301
	.quad	.L305
	.quad	.L309
	.quad	.L316
	.quad	.L273
	.quad	.L265
	.quad	.L281
	.quad	.L289
	.quad	.L323
	.quad	.L96
	.quad	.L343
	.quad	.L95
	.quad	.L186
	.quad	.L200
	.quad	.L223
	.quad	.L615
	.quad	.L613
	.quad	.L616
	.quad	.L545
	.quad	.L455
	.quad	.L550
	.quad	.L549
	.quad	.L552
	.quad	.L551
	.quad	.L553
	.quad	.L554
	.quad	.L358
	.quad	.L377
	.quad	.L412
	.quad	.L94
	.quad	.L93
	.quad	.L430
	.quad	.L92
	.quad	.L446
	.quad	.L569
	.quad	.L555
	.zero	1048
	.section	.rodata
	.align 8
	.type	__PRETTY_FUNCTION__.5, @object
	.size	__PRETTY_FUNCTION__.5, 14
__PRETTY_FUNCTION__.5:
	.string	"v_s_i8_val_at"
	.align 8
	.type	__PRETTY_FUNCTION__.6, @object
	.size	__PRETTY_FUNCTION__.6, 12
__PRETTY_FUNCTION__.6:
	.string	"v_u8_val_at"
	.align 8
	.type	__PRETTY_FUNCTION__.7, @object
	.size	__PRETTY_FUNCTION__.7, 13
__PRETTY_FUNCTION__.7:
	.string	"v_u64_val_at"
	.align 16
	.type	__PRETTY_FUNCTION__.8, @object
	.size	__PRETTY_FUNCTION__.8, 22
__PRETTY_FUNCTION__.8:
	.string	"v_gab_constant_val_at"
	.section	.rodata.str1.1
.LC26:
	.string	"LET"
.LC27:
	.string	"THEN"
.LC28:
	.string	"ELSE"
.LC29:
	.string	"DO"
.LC30:
	.string	"FOR"
.LC31:
	.string	"MATCH"
.LC32:
	.string	"IN"
.LC33:
	.string	"IS"
.LC34:
	.string	"END"
.LC35:
	.string	"DEF"
.LC36:
	.string	"RETURN"
.LC37:
	.string	"YIELD"
.LC38:
	.string	"LOOP"
.LC39:
	.string	"UNTIL"
.LC40:
	.string	"IMPL"
.LC41:
	.string	"PLUS"
.LC42:
	.string	"MINUS"
.LC43:
	.string	"STAR"
.LC44:
	.string	"SLASH"
.LC45:
	.string	"PERCENT"
.LC46:
	.string	"COMMA"
.LC47:
	.string	"COLON"
.LC48:
	.string	"AMPERSAND"
.LC49:
	.string	"DOLLAR"
.LC50:
	.string	"SYMBOL"
.LC51:
	.string	"PROPERTY"
.LC52:
	.string	"MESSAGE"
.LC53:
	.string	"DOT"
.LC54:
	.string	"DOT_DOT"
.LC55:
	.string	"EQUAL"
.LC56:
	.string	"EQUAL_EQUAL"
.LC57:
	.string	"QUESTION"
.LC58:
	.string	"BANG"
.LC59:
	.string	"AT"
.LC60:
	.string	"COLON_EQUAL"
.LC61:
	.string	"LESSER"
.LC62:
	.string	"LESSER_EQUAL"
.LC63:
	.string	"LESSER_LESSER"
.LC64:
	.string	"GREATER"
.LC65:
	.string	"GREATER_EQUAL"
.LC66:
	.string	"GREATER_GREATER"
.LC67:
	.string	"ARROW"
.LC68:
	.string	"FAT_ARROW"
.LC69:
	.string	"AND"
.LC70:
	.string	"OR"
.LC71:
	.string	"NOT"
.LC72:
	.string	"LBRACE"
.LC73:
	.string	"RBRACE"
.LC74:
	.string	"LBRACK"
.LC75:
	.string	"RBRACK"
.LC76:
	.string	"LPAREN"
.LC77:
	.string	"RPAREN"
.LC78:
	.string	"PIPE"
.LC79:
	.string	"SEMICOLON"
.LC80:
	.string	"IDENTIFIER"
.LC81:
	.string	"STRING"
.LC82:
	.string	"INTERPOLATION"
.LC83:
	.string	"INTERPOLATION_END"
.LC84:
	.string	"NUMBER"
.LC85:
	.string	"FALSE"
.LC86:
	.string	"TRUE"
.LC87:
	.string	"NIL"
.LC88:
	.string	"NEWLINE"
.LC89:
	.string	"EOF"
.LC90:
	.string	"ERROR"
	.section	.data.rel.ro.local
	.align 32
	.type	gab_token_names, @object
	.size	gab_token_names, 520
gab_token_names:
	.quad	.LC26
	.quad	.LC27
	.quad	.LC28
	.quad	.LC29
	.quad	.LC30
	.quad	.LC31
	.quad	.LC32
	.quad	.LC33
	.quad	.LC34
	.quad	.LC35
	.quad	.LC36
	.quad	.LC37
	.quad	.LC38
	.quad	.LC39
	.quad	.LC40
	.quad	.LC41
	.quad	.LC42
	.quad	.LC43
	.quad	.LC44
	.quad	.LC45
	.quad	.LC46
	.quad	.LC47
	.quad	.LC48
	.quad	.LC49
	.quad	.LC50
	.quad	.LC51
	.quad	.LC52
	.quad	.LC53
	.quad	.LC54
	.quad	.LC55
	.quad	.LC56
	.quad	.LC57
	.quad	.LC58
	.quad	.LC59
	.quad	.LC60
	.quad	.LC61
	.quad	.LC62
	.quad	.LC63
	.quad	.LC64
	.quad	.LC65
	.quad	.LC66
	.quad	.LC67
	.quad	.LC68
	.quad	.LC69
	.quad	.LC70
	.quad	.LC71
	.quad	.LC72
	.quad	.LC73
	.quad	.LC74
	.quad	.LC75
	.quad	.LC76
	.quad	.LC77
	.quad	.LC78
	.quad	.LC79
	.quad	.LC80
	.quad	.LC81
	.quad	.LC82
	.quad	.LC83
	.quad	.LC84
	.quad	.LC85
	.quad	.LC86
	.quad	.LC87
	.quad	.LC88
	.quad	.LC89
	.quad	.LC90
	.section	.rodata.str1.1
.LC91:
	.string	"OK"
.LC92:
	.string	"A fatal error occurred"
	.section	.rodata.str1.8
	.align 8
.LC93:
	.string	"Unexpected character in string literal"
	.section	.rodata.str1.1
.LC94:
	.string	"Unrecognized token"
.LC95:
	.string	"Unexpected token"
	.section	.rodata.str1.8
	.align 8
.LC96:
	.string	"Functions cannot have more than 255 locals"
	.align 8
.LC97:
	.string	"Functions cannot capture more than 255 locals"
	.align 8
.LC98:
	.string	"Functions cannot have more than 16 parameters"
	.align 8
.LC99:
	.string	"Function calls cannot have more than 16 arguments"
	.align 8
.LC100:
	.string	"Functions cannot return more than 16 values"
	.section	.rodata.str1.1
.LC101:
	.string	"Expected fewer expressions"
	.section	.rodata.str1.8
	.align 8
.LC102:
	.string	"'{}' expressions cannot initialize more than 255 properties"
	.align 8
.LC103:
	.string	"'let' expressions cannot declare more than 16 locals"
	.align 8
.LC104:
	.string	"Variables cannot be referenced before they are initialized"
	.align 8
.LC105:
	.string	"A local with this name already exists"
	.align 8
.LC106:
	.string	"The expression is not assignable"
	.align 8
.LC107:
	.string	"'do' blocks need a corresponding 'end'"
	.section	.rodata.str1.1
.LC108:
	.string	"Variables must be initialized"
	.section	.rodata.str1.8
	.align 8
.LC109:
	.string	"Identifier could not be resolved"
	.align 8
.LC110:
	.string	"A message definition should specify a receiver"
	.section	.rodata.str1.1
.LC111:
	.string	"Expected a number"
.LC112:
	.string	"Expected a record"
.LC113:
	.string	"Expected a string"
.LC114:
	.string	"Expected a callable value"
	.section	.rodata.str1.8
	.align 8
.LC115:
	.string	"Could not call a function with the wrong number of arguments"
	.section	.rodata.str1.1
.LC116:
	.string	"A runtime assertion failed"
.LC117:
	.string	"Reached maximum call depth"
.LC118:
	.string	"Property does not exist"
.LC119:
	.string	"Implementation already exists"
.LC120:
	.string	"Implementation does not exist"
	.section	.data.rel.ro.local
	.align 32
	.type	gab_status_names, @object
	.size	gab_status_names, 240
gab_status_names:
	.quad	.LC91
	.quad	.LC92
	.quad	.LC93
	.quad	.LC94
	.quad	.LC95
	.quad	.LC96
	.quad	.LC97
	.quad	.LC98
	.quad	.LC99
	.quad	.LC100
	.quad	.LC101
	.quad	.LC102
	.quad	.LC103
	.quad	.LC104
	.quad	.LC105
	.quad	.LC106
	.quad	.LC107
	.quad	.LC108
	.quad	.LC109
	.quad	.LC110
	.quad	.LC111
	.quad	.LC112
	.quad	.LC113
	.quad	.LC114
	.quad	.LC115
	.quad	.LC116
	.quad	.LC117
	.quad	.LC118
	.quad	.LC119
	.quad	.LC120
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC19:
	.long	0
	.long	1138753536
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC24:
	.long	0
	.long	-2147483648
	.long	0
	.long	0
	.section	.rodata.cst8
	.align 8
.LC25:
	.long	0
	.long	1072168960
	.ident	"GCC: (GNU) 12.2.0"
	.section	.note.GNU-stack,"",@progbits
