// Trampoline for bsdthread_create(PTHREAD_START_CUSTOM).
// Kernel enters here with:
//   x0 = user pthread pointer, x1 = thread port, x2 = func,
//   x3 = func arg, x4 = stack base, x5 = flags, SP = stack base (16-aligned).
.globl _thread_tramp
_thread_tramp:
	mov	x19, x0			// pthread
	mov	x20, x4			// stack base
	mov	x21, x2			// func
	mov	x22, x3			// arg

	// Decisive marker, no syscalls involved: set thread_done = 77.
	adrp	x9, _thread_done@PAGE
	add	x9, x9, _thread_done@PAGEOFF
	mov	x10, #77
	str	x10, [x9]

	// Marker 1: prove the new thread actually starts executing here.
	adr	x1, entered_msg
	mov	x0, #0			// console fd (first open in this process)
	mov	x2, #15
	mov	x16, #4			// write
	svc	#0x80

	mov	x0, x22
	blr	x21

	// Marker 2: the start routine returned.
	adr	x1, returned_msg
	mov	x0, #0
	mov	x2, #16
	mov	x16, #4
	svc	#0x80

	// bsdthread_terminate(stackaddr, freesize, port, sema)
	mov	x0, x20
	mov	x1, #0x40000		// stack size we mapped
	mov	x2, #0
	mov	x3, #0
	mov	x16, #361
	svc	#0x80
1:	b	1b

entered_msg:
	.ascii	"THREAD-ENTERED\n"
returned_msg:
	.ascii	"THREAD-RETURNED\n"
