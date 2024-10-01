#include "lab.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <ctype.h>
#include <stdlib.h>

char *get_prompt(const char *env) {
    char *prompt = getenv(env);
    if (prompt == NULL) {
        prompt = "$ ";
    }
    return strdup(prompt);
}

int change_dir(char **dir) {
    if (*dir == NULL || **dir == '\0') {
        *dir = getenv("HOME");
    }
    return chdir(*dir);
}

char **cmd_parse(char const *line) {
    long arg_max = sysconf(_SC_ARG_MAX);
    if (arg_max == -1) {
        perror("sysconf");
        return NULL;
    }

    char *line_copy = strdup(line);
    if (line_copy == NULL) {
        perror("strdup");
        return NULL;
    }

    // Calculate the maximum number of arguments
    size_t max_args = arg_max / sizeof(char *);
    char **argv = malloc(sizeof(char *) * (max_args + 1));
    if (argv == NULL) {
        perror("malloc");
        free(line_copy);
        return NULL;
    }

    int argc = 0;
    char *token;
    char *saveptr;
    for (token = strtok_r(line_copy, " \t\n", &saveptr);
         token != NULL && argc < max_args;
         token = strtok_r(NULL, " \t\n", &saveptr)) {
        argv[argc] = strdup(token);
        if (argv[argc] == NULL) {
            perror("strdup");
            for (int i = 0; i < argc; i++) {
                free(argv[i]);
            }
            free(argv);
            free(line_copy);
            return NULL;
        }
        argc++;
    }
    argv[argc] = NULL;

    free(line_copy);
    return argv;
}

void cmd_free(char **argv) {
    if (argv == NULL) return;
    for (int i = 0; argv[i] != NULL; i++) {
        free(argv[i]);
    }
    free(argv);
}

char *trim_white(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool do_builtin(struct shell *sh, char **argv) {
    if (argv[0] == NULL) {
        return false;
    }

    if (strcmp(argv[0], "exit") == 0) {
        printf("Goodbye!\n");
        sh_destroy(sh);
        exit(0);
    } else if (strcmp(argv[0], "cd") == 0) {
        if (argv[1] == NULL) {
            argv[1] = getenv("HOME");
        }
        if (change_dir(&argv[1]) != 0) {
            fprintf(stderr, "cd: %s\n", strerror(errno));
        }
        return true;
    } else {
        return false;
    }
}

void sh_init(struct shell *sh) {
    sh->shell_is_interactive = isatty(STDIN_FILENO);
    sh->shell_terminal = STDIN_FILENO;
    tcgetattr(sh->shell_terminal, &sh->shell_tmodes);
    sh->shell_pgid = getpid();
    if (sh->shell_is_interactive) {
        while (tcgetpgrp(sh->shell_terminal) != sh->shell_pgid) {
            kill(- sh->shell_pgid, SIGTTIN);
        }
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        sh->shell_pgid = getpid();
        if (setpgid(sh->shell_pgid, sh->shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }
        tcsetpgrp(sh->shell_terminal, sh->shell_pgid);
        tcsetattr(sh->shell_terminal, TCSADRAIN, &sh->shell_tmodes);
    }
}

void sh_destroy(struct shell *sh) {
    if (sh->prompt && strcmp(sh->prompt, "$ ") != 0 && strcmp(sh->prompt, "foo>") != 0) {
        free(sh->prompt);
    }
}

void parse_args(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    // Implement command-line argument parsing here if needed
}