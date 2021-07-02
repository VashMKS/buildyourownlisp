#include <stdio.h>
#include <stdlib.h>

// #include with "" instead of <> searches local folder first
#include "mpc.h"  // micro parser combinator lib

// Preprocessing directive below checks if the _WIN32 macro is defined, meaning we are on Windows
// There exist other similar predefined macros for other OS like __linux, __APPLE__ or __ANDROID__
#ifdef _WIN32

// Windows requires no libedit since the default behaviour on the terminal gives those features
// Instead we provide fake substitutes
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
	// Output prompt
	fputs(prompt, stdout);
	
	// Read the user input into the buffer
	fgets(buffer, 2048, stdin);
	
	// Copy the buffer and null-terminate it to make it into a string
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	
	return cpy;
}

void add_history(char* unused) {}

#else

// On Linux & Mac we use libedit for line edition and history on input
#include <editline/readline.h>
#include <editline/history.h>

#endif


long eval_op(long x, char* op, long y) {
	if (strcmp(op, "+") == 0) {return x + y;}
	if (strcmp(op, "-") == 0) {return x - y;}
	if (strcmp(op, "*") == 0) {return x * y;}
	if (strcmp(op, "/") == 0) {return x / y;}
	return 0;
}

long eval(mpc_ast_t* t) {

	// If tagged a number, we evaluate to a int directly
	if (strstr(t->tag, "number")) {
		return atoi(t->contents);
	}
	
	// Operator is always the second child
	char* op = t->children[1]->contents;
	
	// Store third child in x
	long x = eval(t->children[2]);
	
	// Iterate remaining children and combine result
	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	
	return x;
}


int main(int argc, char** argv) {
	
	// Declare parsers
	mpc_parser_t* Number      = mpc_new("number");
	mpc_parser_t* Operator    = mpc_new("operator");
	mpc_parser_t* Expr        = mpc_new("expr");
	mpc_parser_t* Lsp         = mpc_new("lsp");

	// Define them
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                  \
		number   : /-?[0-9]+/ ;                            \
		operator : '+' | '-' | '*' | '/' | '%' ;           \
		expr     : <number> | '(' <operator> <expr>+ ')' ; \
		lsp      : /^/ <operator> <expr>+ /$/ ;            \
		",
		Number, Operator, Expr, Lsp);

	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.2");
	puts("Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		
		char* input = readline("lsp> ");
		add_history(input);
		
		// Attempt to parse user input
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lsp, &r)) {
			// On success evaluate AST and print result
			long result = eval(r.output);
			printf("%li\n", result);
			mpc_ast_delete(r.output);
		} else {
			// Otherwise print error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	// Undefine and delete our parsers
	mpc_cleanup(4, Number, Operator, Expr, Lsp);
	
	return 0;
	
}
