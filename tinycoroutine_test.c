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
#include <stdio.h>
#include "tinycoroutine.h"

struct tinyco_t co;
struct tinyco_context_t c1, c2;
char stack1[4096];
char stack2[4096];
char stack3[4096];

void funcA(void *p)
{
	int i;
	for(i = 10; i < 20; ++i) {
		printf("A%d\n",i);
		tinyco_context_swap(&c2,&c1);
	}
}

void funcB(void *p)
{
	int i;
	for(i = 0; i < 10; ++i) {
		printf("B%d\n",i);
		tinyco_context_swap(&c1,&c2);
	}

}

void funcC(void *p)
{
	int i;
	for(i = 0; i < 8; ++i) {
		printf("C%d\n",i);
		tinyco_yield(&co);
	}
	tinyco_exit(&co, 0);
}

void funcD(void *p)
{
	int i;
	for(i = 0; i < 5; ++i) {
		printf("D%d\n",i);
		tinyco_yield(&co);
	}
}

void funcE(void *p)
{
	int i;
	for(i = 0; i < 10; ++i) {
		printf("E%d\n",i);
		tinyco_yield(&co);
	}
}

int main(int argc,char *argv[])
{
	struct tinyco_context_t cur;

	tinyco_context_get(&cur);
	tinyco_context_create(&c1,funcA,NULL,stack1,sizeof(stack1),&cur);
	tinyco_context_create(&c2,funcB,NULL,stack2,sizeof(stack2),&cur);

	tinyco_context_swap(&c1,&cur);


	tinyco_init(&co, NULL, NULL);
	tinyco_spawn(&co, funcC, NULL, stack1, sizeof(stack1));
	tinyco_spawn(&co, funcD, NULL, stack2, sizeof(stack2));
	tinyco_spawn(&co, funcE, NULL, stack3, sizeof(stack3));

	tinyco_exec(&co);

	return 0;
}
