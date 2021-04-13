/*
 * Copyright (c) 2014, Sound
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

// On gcc Release, fortify is enabled, and longjmp checks the returning stack is
// valid by comparing the return stack address. But based on how coroutine works
// with alternate stacks, this check incorrectly gets tripped. Disable fortify
// for this file.

#undef _FORTIFY_SOURCE

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "tinycoroutine.h"

#ifdef __GNUC__
#define DEF_FUNC_NO_INLINE(ret,name,params) ret __attribute__ ((noinline)) name params
#elif _MSC_VER
#define DEF_FUNC_NO_INLINE(ret,name,params) __declspec(noinline) ret name params
#else
#define DEF_FUNC_NO_INLINE(ret,name,params) ret name params
#endif

#if defined(__APPLE__) || defined(__linux__)
#define setjmp _setjmp
#define longjmp _longjmp
#endif

void tinyco_init(struct tinyco_t *context,tinyco_alloc_func_t alloc,tinyco_free_func_t release)
{
	context->context_count = 0;
	context->context_root.next = &context->context_root;
	context->context_root.prev = &context->context_root;
	context->current_context = &context->context_root;
	context->alloc = alloc ? alloc : malloc;
	context->release = release ? release : free;
}

struct tinyco_spawn_data_t
{
	struct tinyco_t *context;
	tinyco_func_t entry;
	void *param;
	struct tinyco_context_list_t *context_list;
	struct tinyco_context_t ret_context;
};

static void tinyco_spawn_func(struct tinyco_spawn_data_t *data)
{
	// save a copy of tinyco_spawn_data_t to stack variables
	struct tinyco_t *context = data->context;
	tinyco_func_t entry = data->entry;
	void *param = data->param;

	// return to the tinyco_spawn() function
	tinyco_context_swap(&data->ret_context,&data->context_list->context);

	// data is invalid at this point

	// enter the function
	entry(param);

	tinyco_exit(context,0);
}

void tinyco_spawn(struct tinyco_t *context,tinyco_func_t entry,void *param,void *stack,size_t stack_size)
{
	struct tinyco_context_list_t *ctx = (struct tinyco_context_list_t *)context->alloc(sizeof(struct tinyco_context_list_t));
	struct tinyco_spawn_data_t data;

	data.context = context;
	data.entry = entry;
	data.param = param;
	data.context_list = ctx;

	tinyco_context_create(&ctx->context,(tinyco_func_t)tinyco_spawn_func,&data,stack,stack_size,NULL);

	// swap into the new context temporarily so we can save tinyco_spawn_data_t into stack variables
	tinyco_context_swap(&ctx->context, &data.ret_context);

	// add to list
	ctx->next = &context->context_root;
	ctx->prev = context->context_root.prev;
	context->context_root.prev->next = ctx;
	context->context_root.prev = ctx;

	context->context_count++;
}

void tinyco_exec(struct tinyco_t *context)
{
	while(tinyco_yield(context) != 0);
}

int tinyco_yield(struct tinyco_t *context)
{
	struct tinyco_context_list_t *prev_context = context->current_context;
	context->current_context = context->current_context->next;

	tinyco_context_swap(&context->current_context->context,&prev_context->context);

	return context->context_count;
}

void tinyco_exit(struct tinyco_t *context,int exitCode)
{
	struct tinyco_context_list_t *next;

	// unlink list
	context->current_context->prev->next = context->current_context->next;
	context->current_context->next->prev = context->current_context->prev;

	next = context->current_context->next;

	context->release(context->current_context);
	context->context_count--;

	context->current_context = next;
	tinyco_context_swap(&next->context,NULL);
}

DEF_FUNC_NO_INLINE(static void, tinyco_entry, (struct tinyco_context_t * volatile coro))
{
	jmp_buf ret;
	memcpy(ret, coro->ctxt, sizeof(jmp_buf));

	/* Save the entry context, return to caller */
	if (setjmp(coro->ctxt) == 0) {
		JMPBUF_DISABLE_SEH(coro->ctxt);
		longjmp(ret, 1);
	}

	coro->entry(coro->param);

	if (coro->ret) tinyco_context_swap(coro->ret, NULL);

	exit(1);
}

DEF_FUNC_NO_INLINE(static int,tinyco_get_stack_ptr_in_jmp_buf, (jmp_buf *lo_buf))
{
	size_t local = (size_t)&local;
	size_t n;
	jmp_buf hi_buf;
	volatile size_t *lo_ptr, *hi_ptr;
	int i;

	setjmp(hi_buf);

	/* attempt to determine which offset in the jmp_buf stores the stack pointer */
	n = sizeof(jmp_buf) / sizeof(size_t);
	lo_ptr = (size_t *)*lo_buf;
	hi_ptr = (size_t *)hi_buf;
	for(i = 0; i < n; ++i) {
		if(hi_ptr[i] <= local && local <= lo_ptr[i] && (lo_ptr[i] - hi_ptr[i]) < 1024) {
			return i;
		}
	}

	return -1;
}

DEF_FUNC_NO_INLINE(static int,tinyco_get_stack_dir2, (volatile int *x) )
{
	volatile int y;
	return &y < x ? -1 : 1;
}

static int tinyco_get_stack_dir()
{
	volatile int x;
	return tinyco_get_stack_dir2(&x);
}

/*
 * This is kinda a ridiculous hack.. but on x64 on first 4 params passed by registers.
 * The rest passed on stack. We can use the jmp_buf hack on msvc x64 to call the entry function.
 */
#if defined(_MSC_VER) && defined(_WIN64)
static DEF_FUNC_NO_INLINE(void, tinyco_msvc_x64_entry, (__int64 dummy1,__int64 dummy2,__int64 dummy3,__int64 dummy4,void (*entry)(struct tinyco_context_t *),struct tinyco_context_t *coro) )
{
	entry(coro);
}

#endif

/*
 * This is the non-portable bit which requires a bit of ASM.
 * The stack pointer should be replaced with the passed stack pointer and the entry function should be
 * called with the new stack in use.
 */
static void tinyco_swap_stack_and_call(void *stack,void (*entry)(struct tinyco_context_t *),struct tinyco_context_t *coro)
{
#if defined(_MSC_VER) && defined(_WIN32) && !defined(_WIN64)
	__asm {
		mov ecx, stack
		lea esp, [ecx - 4]
		and esp, -16
		mov eax, coro
		mov [esp], eax
		call entry
	}
#elif defined(_MSC_VER) && defined(_WIN64)
	jmp_buf buf;
	_JUMP_BUFFER *jb;

	/* on x86 windows, we can't use inline asm, so rely on
	 * hacking the jmp_buf to exchange the stack and call our entry func */
	if(setjmp(buf) == 0) {
		jb = (_JUMP_BUFFER *)buf;
		JMPBUF_DISABLE_SEH(buf);
		jb->Rsp = (((unsigned __int64)stack - 6 * sizeof(__int64)) & ~0xFLL) - sizeof(__int64); // align 16-byte, sub 8 for return addr
		*((unsigned __int64 *)jb->Rsp) = 0xCCCCCCCCCCCCCCCCLL; // return address, but we'll never reach this
		*((unsigned __int64 *)jb->Rsp + 5) = (unsigned __int64)entry; // param 5
		*((unsigned __int64 *)jb->Rsp + 6) = (unsigned __int64)coro; // param 6
		jb->Rip = (unsigned __int64)tinyco_msvc_x64_entry;
		longjmp(buf,1);
	}
#elif defined(__GNUC__) && defined(__x86_64__)
	/* x64 GNU C */
	__asm__ __volatile__ (
		"movq %0, %%rsp\n"   /* replace stack pointer */
		"andq $-16, %%rsp\n" /* align 16-bytes */
		"callq *%2\n"        /* call function */
		:
		: "g" (stack),
		  "D" (coro),        /* load coro into edi for the first parameter to pass */
		  "r" (entry),
		  "c" (coro)         /* make windows happy */
		: );
#elif defined(__GNUC__) && defined(__i386__)
	__asm__ __volatile__ (
		"leal -4(%0), %%esp\n" /* replace stack pointer */
		"andl $-16, %%esp\n"   /* align 16-bytes */
		"movl %1, (%%esp)\n"   /* Pass 1 parameter */
		"calll *%2\n"          /* call function */
		:
		: "r" (stack), "r" (coro), "r" (entry)
		: );
#elif defined(__GNUC__) && defined(__arm__)
	__asm__ __volatile__ (
		"mov sp, %0\n"
		"mov r0, %1\n"
		"bx %2\n"
		:
		: "r" (stack), "r" (coro), "r" (entry)
		: "r0");
#else

	/* This is a very flaky ANSI hack to attempt to swap the stack and call
	 * the entry function. Try not to use this if possible. Uncomment below 
	 * if you want to use regardless. */

	#error Not supported for your compiler / platform.

	static struct tinycoroutine_t *c;
	jmp_buf buf;

	c = coro;

	/*
	 * This fallback routine attempts to set the stack by using the auto-detected
	 * jmp_buf offset. It uses setjmp to get the existing jmp_buf, then hacks
	 * in the stack address into the saved jmp_buf. When we longjmp with the hacked
	 * jmp_buf, the stack will be overloaded.
	 */

	// swap the stack pointer
	if(setjmp(buf) == 0) {
		((size_t *)buf)[tinyco_get_stack_ptr_in_jmp_buf(&buf)] = stack;
		longjmp(buf,1);
	}

	/* registers are messed at this point, don't insert code here */

	/* static function call to coroutine_entry
	 this will enter the coroutine in a state with the new stack */
	tinyco_entry(c);
#endif

	assert(0 && "Should not return here.");
}


void tinyco_context_create(struct tinyco_context_t *coro,tinyco_func_t entry,void *param,void *stack,size_t stack_size,struct tinyco_context_t *ret)
{
	coro->entry = entry;
	coro->param = param;
	coro->ret = ret;

	if(setjmp(coro->ctxt) == 0) {
		JMPBUF_DISABLE_SEH(coro->ctxt);
		char *stackBase = tinyco_get_stack_dir() < 0 ? (char *)stack + stack_size : (char *)stack;
		tinyco_swap_stack_and_call(stackBase,tinyco_entry,coro);
		/* not expected to return here */
	}
}

void tinyco_context_swap(struct tinyco_context_t * volatile coro, struct tinyco_context_t * volatile prev)
{
	if(!prev || setjmp(prev->ctxt) == 0) {
		if (prev) JMPBUF_DISABLE_SEH(prev->ctxt);
		longjmp(coro->ctxt,1);
	}
}

#ifdef setjmp
#undef setjmp
#endif

#ifdef longjmp
#undef longjmp
#endif

#undef DEF_FUNC_NO_INLINE

