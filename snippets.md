# Compiling, running and debugging

```bash
# compile and link against libedit
gcc -std=c99 -Wall program.c -ledit -o program

# run resulting executable
./program

# compile debug executable and start debug session with dbg
gcc -std=c99 -Wall -g program.c -ledit -o program
gdb program
```

# Setup

In order to be able to compile and run the project the following tools are needed. This whole project was developed on Ubuntu 21.04. For other distros look up the equivalent commands.

## GCC and adjacent tools (gcc, gdb)

```bash
sudo apt install build-essential
```

## Dependencies

```bash
# editline
sudo apt install libedit-dev
```

# Notes

## Type System

(TODO) We have builtin types (integers, booleans, floats). Strings are denoted as `char*` and are null terminated. More about types on [cppreference](https://en.cppreference.com/w/c/language/type).

## The Preprocessor

More about the preprocessor on [cppreference.com](https://en.cppreference.com/w/c/preprocessor).

You can check in which OS you are compiling with preprocessor directives (e.g. `#idef __linux`), which allows you to write OS-specific code. The following are some of the predefined macros on each major OS:

- Linux: `__linux`
- Windows: `_WIN32` for 32 and 64 bit, `_WIN64` for 64 bit only.
- Mac OS X: both `__APPLE__` `__MACH__`

More info on [this StackOverflow question](https://stackoverflow.com/questions/142508/how-do-i-check-os-with-a-preprocessor-directive)
