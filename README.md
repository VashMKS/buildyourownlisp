# Learning C by Building a Lisp-like Interpreter

As the title says, the purpose of this repository is not to actually craft a useful Lisp variant, but to learn the basics of the C language, as well as some fundamentals on compiler design and the Lisp family of languages.

The code follows the book *Build Your Own Lisp* by Daniel Holden, which is available online for free at [buildyourownlisp.com](buildyourownlisp.com). For more information, check out the first couple of chapters online.

## Setup

In order to be able to compile and run the project the following is needed. This project was developed on Ubuntu 21.04. For other distros look up the equivalent commands.

### Tools

```bash
# Compiler (gcc) and debugger (dbg)
sudo apt install build-essential
```

### Dependencies

```bash
# editline
sudo apt install libedit-dev

# mpc 
# simply copy mpc.c and mpc.h from the repo onto your source folder
git clone git@github.com:orangeduck/mpc.git
cp mpc/{mpc.c,mpc.h} buildyourownlisp/src/
```

### Compiling, running and debugging

```bash
# compile and link against mpc, libedit and math
gcc -std=c99 -Wall prompt.c mpc.c -ledit -lm -o prompt

# run resulting executable
./prompt
```

```bash
# compile debug executable and start debug session with dbg
# same as above plus -g flag
gcc -std=c99 -Wall -g prompt.c mpc.c -ledit -lm -o prompt
gdb prompt
```
