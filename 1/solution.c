#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "libcoro.h"
#include "../utils/heap_help/heap_help.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

#define NUMBER_BUFFER_SIZE 4096 // Can be tweaked to avoid allocation syscalls and more efficient memory usage
#define number_buffer int*

struct numbers {
    number_buffer *numbers;
    int buffers;
};
struct numbers*
numbers_create() {
    struct numbers *numbers = malloc(sizeof(struct numbers));
    numbers->buffers = 0;
    numbers->numbers = NULL;
    return numbers;
}
number_buffer
numbers_create_buffer(struct numbers *numbers) {
    number_buffer buffer = malloc(sizeof(int) * NUMBER_BUFFER_SIZE);
    if (numbers->numbers == NULL) {
        numbers->numbers = malloc(sizeof(number_buffer));
    } else {
        numbers->numbers = realloc(numbers->numbers, (numbers->buffers + 1) *sizeof(number_buffer));
    }
    numbers->numbers[numbers->buffers] = buffer;
    numbers->buffers++;
    return buffer;
}

void
numbers_free(struct numbers *numbers) {
    for (int i = 0; i < numbers->buffers; i++) {
        free(numbers->numbers[i]);
    }
    free(numbers->numbers);
    free(numbers);
}

struct my_context {
	char *name;
    struct numbers *numbers;
    long work_time;
	/** ADD HERE YOUR OWN MEMBERS, SUCH AS FILE NAME, WORK TIME, ... */
};

static struct my_context *
my_context_new(const char *name, struct numbers *numbers, long work_time)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->name = strdup(name);
    ctx->numbers = numbers;
    ctx->work_time = work_time;
	return ctx;
}

static void insert_sort(number_buffer buffer, int number, int i) {
    // find where number should be placed
    int place = i - 1;
    while (place >= 0) {
        if (buffer[place] <= number) {
            break;
        }
        place--;
    }
    place++;
    // move everything after the number to the right
    for (int j = i; j > place; j--) {
        buffer[j] = buffer[j - 1];
    }

    buffer[place] = number;
}

static void
my_context_delete(struct my_context *ctx)
{
	free(ctx->name);
	free(ctx);
}


long getns() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000000 + time.tv_nsec;
}


/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static int
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */
	struct coro *this = coro_this();
	struct my_context *ctx = context;
	char *name = ctx->name;
    FILE *fd = fopen(name, "r");

    struct numbers *numbers = ctx->numbers;
    number_buffer current_buffer = numbers_create_buffer(numbers);
    int current_number;
    int numbers_in_buffer = 0;
    long total_time = 0;
    long current_time = getns();
    while (fscanf(fd, "%d", &current_number) != EOF) {
        if (numbers_in_buffer >= NUMBER_BUFFER_SIZE) {
            current_buffer = numbers_create_buffer(numbers);
            numbers_in_buffer = 0;
        }
        insert_sort(current_buffer, current_number, numbers_in_buffer);
        numbers_in_buffer++;
        long now = getns();
        if (now - current_time >= ctx->work_time) {
            total_time += now - current_time;
            coro_yield();
            current_time = getns();
        }
    }
    total_time += getns() - current_time;
    if (numbers_in_buffer < NUMBER_BUFFER_SIZE) {
        current_buffer[numbers_in_buffer] = -1;
    }

    fclose(fd);
    int seconds = total_time / 1000000000;
    int millis = total_time / 1000000 % 1000;
    int micros = total_time / 1000 % 1000;
    printf("Total time coroutine worked %ds %dms %dus. Switches %lld\n", seconds, millis, micros, coro_switch_count(this));
	my_context_delete(ctx);
	return 0;
}


int
main(int argc, char **argv)
{
	/* Delete these suppressions when start using the args. */
	(void)argc;
	(void)argv;
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
    struct numbers *numbers = numbers_create();
    long overall_target_latency = atoi(argv[1]);
    long target_latency = overall_target_latency / (argc - 1);
    for (int i = 0; i < argc - 2; i++) {
        coro_new(coroutine_func_f, my_context_new(argv[i + 2], numbers, target_latency));
    }
    long start_time = getns();
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	/* All coroutines have finished. */
    long end_time = getns();

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */
    FILE *output = fopen("output", "w");
    int *indices = malloc(numbers->buffers * sizeof(int));
    for (int i = 0; i < numbers->buffers; i++) {
        indices[i] = 0;
    }
    int min_number;
    int min_i = 0;
    while (min_i != -1) {
        min_i = -1;
        min_number = INT_MAX;
        for (int i = 0; i < numbers->buffers; i++) {
            int local_index = indices[i];
            if (local_index >= NUMBER_BUFFER_SIZE) continue;
            int value = numbers->numbers[i][local_index];
            if (value != -1 && value < min_number) {
                min_i = i;
                min_number = value;
            }
        }
        if (min_i != -1) {
            fprintf(output, "%d ", min_number);
            indices[min_i]++;
        }
    }
    fclose(output);

    numbers_free(numbers);
    free(indices);
    long total_time = end_time - start_time;
    int seconds = total_time / 1000000000;
    int millis = total_time / 1000000 % 1000;
    int micros = total_time / 1000 % 1000;
    printf("Alloc counts: %llu\n", heaph_get_alloc_count());
    printf("Total time all coroutines worked %ds %dms %dus\n", seconds, millis, micros);
    return 0;
}
