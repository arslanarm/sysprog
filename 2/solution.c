#include "parser.h"

#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define __WEXITSTATUS(status) (((status) & 0xff00) >> 8)

static void
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */

	assert(line != NULL);
    FILE *output;
	if ((int)line->is_background) {

    }
	if (line->out_type == OUTPUT_TYPE_STDOUT) {
        output = stdout;
	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
        output = fopen(line->out_file, "w");
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
        output = fopen(line->out_file, "a");
	} else {
		assert(false);
	}
    int fd1[2];
    int fd2[2];
    int num_children = 0;
    int *children = NULL;
    int prev_piped = 0;
    int only_one = 1;
	const struct expr *e = line->head;
	while (e != NULL) {
        if (e->type == EXPR_TYPE_COMMAND && strcmp(e->cmd.exe, "cd") == 0) {
            only_one = 0;
//            printf("CD\n");
            if (e->cmd.arg_count != 1) {
                printf("Arg count for cd must be 1\n");
                return;
            }
            chdir(e->cmd.args[0]);
        } else if (e->type == EXPR_TYPE_COMMAND && strcmp(e->cmd.exe, "exit") == 0 && e->next == NULL && only_one) {
//            printf("EXITTING\n");
            exit(0);
        } else if (e->type == EXPR_TYPE_COMMAND) {
//            printf("Executing: %s\n", e->cmd.exe);
//            for (uint32_t i = 0; i < e->cmd.arg_count; i++) {
//                printf("Arg %i %s\n", i, e->cmd.args[i]);
//            }
//            if (line->out_type == OUTPUT_TYPE_STDOUT) {
//                printf("Output to stdout\n");
//            } else {
//                printf("Output to %s\n", line->out_file);
//            }
            only_one = 0;
            int piped = 0;
            if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
                pipe(fd1);
                piped = 1;
            }
            int pid = fork();
            if (pid == 0) {
                if (prev_piped) {
                    close(fd2[1]);
                    dup2(fd2[0], STDIN_FILENO);
                    close(fd2[0]);
                }
                if (piped) {
                    close(fd1[0]);
                    dup2(fd1[1], STDOUT_FILENO);
                    close(fd1[1]);
                } else if (line->out_type != OUTPUT_TYPE_STDOUT) {
                    dup2(fileno(output), STDOUT_FILENO);
                }
                if (line->out_type != OUTPUT_TYPE_STDOUT) {
                    fclose(output);
                }
                char **argv = malloc(sizeof (char *) * (e->cmd.arg_count + 2));
                argv[0] = e->cmd.exe;
                for (uint32_t i = 0; i < e->cmd.arg_count; i++) {
                    argv[i + 1] = e->cmd.args[i];
                }
                argv[e->cmd.arg_count + 1] = NULL;
                execvp(e->cmd.exe, argv);
            }
            if (prev_piped) {
                close(fd2[0]);
                close(fd2[1]);
            }
            if (piped) {
                fd2[0] = fd1[0];
                fd2[1] = fd1[1];
            }
            num_children++;
            if (children != NULL) {
                children = malloc(sizeof(int));
            } else {
                children = realloc(children, sizeof(int) * num_children);
            }
            children[num_children - 1] = pid;
            prev_piped = piped;
		} else if (e->type == EXPR_TYPE_PIPE) {
		} else if (e->type == EXPR_TYPE_AND) {
            int status;
            for (int i = 0; i < num_children; i++) {
                waitpid(children[i], &status, 0);
            }
            free(children);
            children = NULL;
            num_children = 0;
            if (status != 0) {
                return;
            }
		} else if (e->type == EXPR_TYPE_OR) {
            int status;
            for (int i = 0; i < num_children; i++) {
                waitpid(children[i], &status, 0);
            }
            free(children);
            children = NULL;
            num_children = 0;
            if (status == 0) {
                return;
            }
		} else {
			assert(false);
		}
		e = e->next;
	}
    for (int i = 0; i < num_children; i++) {
        int status;
        waitpid(children[i], &status, 0);
    }
    free(children);
}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
    fflush(stdout);
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
            fflush(stdout);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
