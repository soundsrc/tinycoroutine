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
#ifndef INC_COROUTINE_H
#define INC_COROUTINE_H

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tinyco_func_t)(void *);
typedef void *(*tinyco_alloc_func_t)(size_t);
typedef void (*tinyco_free_func_t)(void *);

struct tinyco_context_t
{
	jmp_buf ctxt;
	tinyco_func_t entry;
	void *param;
	struct tinyco_context_t *ret;
};

struct tinyco_context_list_t
{
	struct tinyco_context_t context;
	struct tinyco_context_list_t *prev, *next;
};

struct tinyco_t
{
	tinyco_alloc_func_t alloc;
	tinyco_free_func_t release;
	struct tinyco_context_list_t context_root;
	struct tinyco_context_list_t *current_context;
	int context_count;
};


/********************************************************************************************************************************************************
 * HI-LEVEL API
 * The high level APIs provides a system which maintains a running
 * list of coroutines that can be easily switched between.
 ********************************************************************************************************************************************************/

/**
 * Creates a execution context which manages a list of running coroutines.
 * @param[out] context Pointer to an uninitialized @a tinyco_t struct
 * @param alloc Optional allocation function.
 * @param release Optional release function.
 */
void tinyco_init(struct tinyco_t *context,tinyco_alloc_func_t alloc,tinyco_free_func_t release);

/**
 * Schedules a new coroutine on the current context calling entry(param) as the entry function with stack of @a stack_size.
 * @param context tinyco_t context
 * @param entry Entry function
 * @param param Parameter
 * @param stack Pointer to stack pointer
 * @param stack_size Stack size
 */
void tinyco_spawn(struct tinyco_t *context,tinyco_func_t entry,void *param,void *stack,size_t stack_size);

/**
 * Starts the coroutine execution loop, executing each coroutine in round robin style.
 * This function does not return until all coroutines have completed.
 * @param context tinyco_t context
 */
void tinyco_exec(struct tinyco_t *context);

/**
 * Saves the current execution state to the current coroutine and switches to the next available coroutine.
 * @param context tinyco_t context
 * @return Returns the number of running coroutines.
 */
int tinyco_yield(struct tinyco_t *context);

/**
 * Terminates the current coroutine and removes it from execution.
 * This routine terminates the current execution and does not perform any cleanup,
 * it is up to the user to release unused resources before calling @a tinyco_exit.
 * @param context tinyco_t context
 * @param exitCode Exit code (currently does nothing).
 */
void tinyco_exit(struct tinyco_t *context,int exitCode);


/********************************************************************************************************************************************************
 * LOW-LEVEL API
 * The level level APIs provides a low level ucontext-like interface to coroutines.
 * The low level apis allows one to obtain a state of executation and switch between
 * them.
 ********************************************************************************************************************************************************/


/**
 * Initializes a coroutine context, with an entry function and a stack.
 * When @a coroutine_swap is called with the context, execution begins at the
 * entry function and once the entry function has completed, the coroutine passed
 * in @a ret is switched to or the routine exists with a @a exit() call.
 *
 * @param coro Address of an uninitialized coroutine_t structure.
 * @param entry Entry function of the coroutine.
 * @param param User passed pointer which is passed as the first paramter in the entry function.
 * @param stack Pointer to the buffer to be used for the stack
 * @param stack_size Size of the stack buffer.
 * @param ret Pointer to an initialized coroutine_t to switch to for when the coroutine returns.
 *            NULL may be passed here, which will cause the coroutine to call @a exit() when it completes.
 */
void tinyco_context_create(struct tinyco_context_t *coro,tinyco_func_t entry,void *param,void *stack,size_t stack_size,struct tinyco_context_t *ret);

/**
 * Initializes a coroutine context with the current state of execution.
 *
 * @param coro Address of an uninitialized coroutine_t structure.
 */
void tinyco_context_get(struct tinyco_context_t *coro);

/**
 * Saves the current coroutine context and switches to the passed coroutine.
 * @param coro Coroutine context to switch to
 * @param prev Current coroutine context is saved. This parameter can be NULL.
 */
void tinyco_context_swap(struct tinyco_context_t * volatile coro, struct tinyco_context_t * volatile prev);

#ifdef __cplusplus
}
#endif

#endif
