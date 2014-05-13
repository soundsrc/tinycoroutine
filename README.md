TinyCoroutine
=============

TinyCoroutine is a simple coroutine library. It currently runs on x86 and ARM and should be easy to extend to other platforms. TinyCoroutine uses setjmp()/longjmp() and a small bit of ASM to handle execution context switches.

TinyCoroutine provides a high-level and low level API. The high level API provides a coroutine system which manages multiple coroutine contexts and allows easy switching between ech context. The low level API provides a ucontext-like interface which allows one to obtain a state of execution and switch between execution states.

High Level API
--------------

	void tinyco_init(struct tinyco_t *context,tinyco_alloc_func_t alloc,tinyco_free_func_t release);

Initializes tinycoroutine context.

	void tinyco_spawn(struct tinyco_t *context,tinyco_func_t entry,void *param,void *stack,size_t stack_size);

Schedules the function entry(param) with stack to be executed.

	void tinyco_exec(struct tinyco_t *context);

Switches to the next coroutine and returns only when all coroutines have completed. This is equaivalent to calling tinyco_yield() in an loop.

	void tinyco_yield(struct tinyco_t *context);

Saves the current execution and switches to the next coroutine.

	void tinyco_exit(struct tinyco_t *context,int exitCode);

Terminates the current coroutine.

Low Level API
-------------

	void tinyco_context_create(struct tinyco_context_t *coro,tinyco_func_t entry,void *param,void *stack,size_t stack_size,struct tinyco_context_t *ret);

Initializes a execution context with execution starting at function entry(param) and stack.

	void tinyco_context_get(struct tinyco_context_t *coro);

Initializes a execution context saving the current execution state.

	void tinyco_context_swap(struct tinyco_context_t *coro,struct tinyco_context_t *prev);

Switches to the execution context coro while saving the current context to prev.
