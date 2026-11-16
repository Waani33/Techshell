# Techshell

Name:Wani Poudel

## Description

`techshell` is a small command-line shell written in C. It displays the current
working directory as its prompt, reads commands, and runs external programs in
child processes using `fork()` and `execvp()`. The built-in `cd` command changes
the working directory of the shell itself, and `exit` closes the shell.

The shell supports input redirection with `<`, output redirection with `>`,
double-quoted and single-quoted arguments, and backslash-escaped spaces in
filenames.

## Clone, compile, and run

Clone the repository:

```bash
git clone YOUR_GITHUB_REPOSITORY_URL
cd YOUR_REPOSITORY_DIRECTORY
```

Compile:

```bash
gcc -Wall -Wextra -std=c11 -D_GNU_SOURCE techshell.c -o techshell
```

Run:

```bash
./techshell
```

Type `exit` to leave the shell.

## Testing examples

```bash
pwd
ls -l
echo "hello there"
cat < input.txt
wc -l < input.txt
ls > listing.txt
cat < input.txt > copy.txt
cd ..
exit
```

## Limitations

Pipes (`|`), command chaining, environment-variable expansion, wildcard
expansion, and background execution are not implemented. The required basic
commands, `cd`, `exit`, input/output redirection, and quoted or escaped
arguments are implemented.
