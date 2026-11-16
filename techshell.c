#define _GNU_SOURCE
// Name: Wani Poudel
// Description: A simple command shell with cd, exit, and input/output redirection.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define SHELL_INPUT_SIZE 4096
#define MAX_ARGS 256

/*
 * Print errors in the format required by the project.
 */
static void print_error(int error)
{
    fprintf(stderr, "Error %d (%s)\n", error, strerror(error));
}

/*
 * Split a command line into tokens.
 *
 * Whitespace separates tokens, double and single quotes keep text together,
 * and backslash escapes the next character (including a space).
 * The special characters < and > are always separate tokens.
 *
 * Each token is separately allocated and must later be freed.
 */
static int tokenize(const char *input, char *tokens[], int max_tokens)
{
    int count = 0;
    size_t i = 0;
    size_t length = strlen(input);

    while (i < length)
    {
        char token[SHELL_INPUT_SIZE];
        size_t j = 0;
        char quote = '\0';
        int token_started = 0;

        /* Skip whitespace between tokens. */
        while (i < length && (input[i] == ' ' || input[i] == '\t' ||
                              input[i] == '\r' || input[i] == '\n'))
        {
            i++;
        }

        if (i >= length)
            break;

        /* Redirection operators can be adjacent to other text. */
        if (input[i] == '<' || input[i] == '>')
        {
            if (count >= max_tokens)
                return -1;

            token[0] = input[i++];
            token[1] = '\0';
            tokens[count] = strdup(token);
            if (tokens[count] == NULL)
                return -1;
            count++;
            continue;
        }

        while (i < length)
        {
            char c = input[i];

            if (quote == '\0' && (c == ' ' || c == '\t' ||
                                  c == '\r' || c == '\n' ||
                                  c == '<' || c == '>'))
            {
                break;
            }

            token_started = 1;

            if (c == '\\' && quote != '\'')
            {
                /* A backslash makes the next character literal. */
                i++;
                if (i < length)
                    token[j++] = input[i++];
                else
                    token[j++] = '\\';
            }
            else if ((c == '"' || c == '\'') &&
                     (quote == '\0' || quote == c))
            {
                if (quote == '\0')
                    quote = c;
                else
                    quote = '\0';
                i++; /* Do not include quote marks in the token. */
            }
            else
            {
                token[j++] = c;
                i++;
            }

            if (j >= sizeof(token) - 1)
                break;
        }

        token[j] = '\0';

        if (token_started)
        {
            if (count >= max_tokens)
                return -1;

            tokens[count] = strdup(token);
            if (tokens[count] == NULL)
                return -1;
            count++;
        }

        /* The next loop handles a redirection operator at input[i]. */
    }

    return count;
}

static void free_tokens(char *tokens[], int count)
{
    for (int i = 0; i < count; i++)
        free(tokens[i]);
}

/*
 * Parse redirection tokens out of the argument list.
 * The remaining arguments are passed to execvp().
 */
static int parse_redirections(char *tokens[], int token_count,
                              char *args[], char **input_file,
                              char **output_file)
{
    int arg_count = 0;

    *input_file = NULL;
    *output_file = NULL;

    for (int i = 0; i < token_count; i++)
    {
        if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0)
        {
            int is_input = (tokens[i][0] == '<');

            if (i + 1 >= token_count ||
                strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0)
            {
                print_error(EINVAL);
                return -1;
            }

            if (is_input)
                *input_file = tokens[++i];
            else
                *output_file = tokens[++i];
        }
        else
        {
            args[arg_count++] = tokens[i];
        }
    }

    args[arg_count] = NULL;
    return arg_count;
}

/*
 * Change directory in the shell process itself. A child process cannot
 * change the working directory of its parent shell.
 */
static void run_cd(char *args[])
{
    const char *destination;

    if (args[1] == NULL)
        destination = getenv("HOME");
    else
        destination = args[1];

    if (destination == NULL)
    {
        print_error(ENOENT);
        return;
    }

    if (chdir(destination) == -1)
        print_error(errno);
}

static void execute_external(char *args[], char *input_file,
                             char *output_file)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        print_error(errno);
        return;
    }

    if (pid == 0)
    {
        /* Child: set up input redirection, if requested. */
        if (input_file != NULL)
        {
            int fd = open(input_file, O_RDONLY);
            if (fd == -1)
            {
                print_error(errno);
                _exit(1);
            }

            if (dup2(fd, STDIN_FILENO) == -1)
            {
                int saved_error = errno;
                close(fd);
                print_error(saved_error);
                _exit(1);
            }
            close(fd);
        }

        /* Child: set up output redirection, if requested. */
        if (output_file != NULL)
        {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1)
            {
                print_error(errno);
                _exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) == -1)
            {
                int saved_error = errno;
                close(fd);
                print_error(saved_error);
                _exit(1);
            }
            close(fd);
        }

        execvp(args[0], args);

        /* execvp returns only when it cannot run the requested command. */
        print_error(errno);
        _exit(127);
    }

    /* Parent: wait for this command to finish before showing the next prompt. */
    int status;
    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno != EINTR)
        {
            print_error(errno);
            break;
        }
    }
}

int main(void)
{
    char input[SHELL_INPUT_SIZE];
    char cwd[PATH_MAX];

    while (1)
    {
        /* Display the current working directory as the shell prompt. */
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s$ ", cwd);
        else
            printf("$ ");

        fflush(stdout);

        /* Read one command line. */
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        /* If the line was too long, discard the rest of it. */
        if (strchr(input, '\n') == NULL && !feof(stdin))
        {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
        }

        input[strcspn(input, "\n")] = '\0';

        char *tokens[MAX_ARGS];
        int token_count = tokenize(input, tokens, MAX_ARGS - 1);

        if (token_count < 0)
        {
            fprintf(stderr, "Error: unable to parse command\n");
            continue;
        }

        /* Empty or whitespace-only input: just show the prompt again. */
        if (token_count == 0)
            continue;

        char *args[MAX_ARGS];
        char *input_file = NULL;
        char *output_file = NULL;

        int arg_count = parse_redirections(tokens, token_count, args,
                                           &input_file, &output_file);

        if (arg_count < 0)
        {
            free_tokens(tokens, token_count);
            continue;
        }

        /* A line containing only redirection syntax has no command to run. */
        if (arg_count == 0)
        {
            free_tokens(tokens, token_count);
            continue;
        }

        if (strcmp(args[0], "exit") == 0)
        {
            free_tokens(tokens, token_count);
            break;
        }

        if (strcmp(args[0], "cd") == 0)
        {
            run_cd(args);
            free_tokens(tokens, token_count);
            continue;
        }

        execute_external(args, input_file, output_file);
        free_tokens(tokens, token_count);
    }

    return 0;
}
