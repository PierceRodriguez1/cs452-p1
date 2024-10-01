#include "../src/lab.h"
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

//#define MIN(a,b) ((a) < (b) ? (a) : (b))

void initialize_shell(struct shell *sh) {
    sh->shell_terminal = STDIN_FILENO;
    sh->shell_is_interactive = isatty(sh->shell_terminal);

    if (sh->shell_is_interactive) {
        while (tcgetpgrp(sh->shell_terminal) != (sh->shell_pgid = getpgrp()))
            kill(-sh->shell_pgid, SIGTTIN);

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
        tcgetattr(sh->shell_terminal, &sh->shell_tmodes);
    }
}

void launch_process(struct shell *sh, char **cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        pid_t child = getpid();
        setpgid(child, child);
        tcsetpgrp(sh->shell_terminal, child);
        
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        execvp(cmd[0], cmd);
        fprintf(stderr, "exec failed: %s\n", strerror(errno));
        exit(1);
    } else if (pid < 0) {
        // Fork failed
        perror("fork");
    } else {
        // Parent process
        setpgid(pid, pid);
        tcsetpgrp(sh->shell_terminal, pid);

        int status;
        waitpid(pid, &status, WUNTRACED);

        tcsetpgrp(sh->shell_terminal, sh->shell_pgid);
        tcsetattr(sh->shell_terminal, TCSADRAIN, &sh->shell_tmodes);
    }
}

int main(int argc, char **argv) {
    int opt;
    long arg_max = sysconf(_SC_ARG_MAX);
    if (arg_max == -1) {
        perror("sysconf");
        return 1;
    }

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                printf("lab version %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
                return 1;
        }
    }

    struct shell sh;
    parse_args(argc, argv);
    initialize_shell(&sh);

    char *prompt = getenv("MY_PROMPT");
    if (prompt == NULL) {
        prompt = "$ ";
    }
    sh.prompt = prompt;

    using_history();

    char *line;
    while ((line = readline(sh.prompt))) {
        add_history(line);

        char *trimmed_line = trim_white(line);
        if (strlen(trimmed_line) == 0) {
            free(line);
            continue;
        }

        char **argv_parsed = cmd_parse(trimmed_line);
        if (!do_builtin(&sh, argv_parsed)) {
            launch_process(&sh, argv_parsed);
        }
        cmd_free(argv_parsed);
        free(line);
    }

    sh_destroy(&sh);
    return 0;
}