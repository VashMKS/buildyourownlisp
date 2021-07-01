#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"  // #include with "" instead of <> searches local folder first

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


int main(int argc, char** argv) {
	
	// Declare parsers
	mpc_parser_t* Integer     = mpc_new("integer");
	mpc_parser_t* Decimal     = mpc_new("decimal");
	mpc_parser_t* Number      = mpc_new("number");
	mpc_parser_t* OperatorSym = mpc_new("operator_sym");
	mpc_parser_t* OperatorStr = mpc_new("operator_str");
	mpc_parser_t* Operator    = mpc_new("operator");
	mpc_parser_t* Expr        = mpc_new("expr");
	mpc_parser_t* Lsp         = mpc_new("lsp");

	// Define them
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                                \
		integer      : /-?[0-9]+/ ;                                      \
		decimal      : /-?[0-9]+/ '.' /[0-9]+/ ;                         \
		number       : <decimal> | <integer> ;                           \
		operator_sym : '+' | '-' | '*' | '/' | '%' ;                     \
		operator_str : \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ; \
		operator     : <operator_sym> | <operator_str> ;                 \
		expr         : <number> | '(' <operator> <expr>+ ')' ;           \
		lsp          : /^/ <operator> <expr>+ /$/ ;                      \
		",
		Integer, Decimal, Number, OperatorSym, OperatorStr, Operator, Expr, Lsp);

	
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
			// On success print AST
			mpc_ast_print(r.output);
			mpc_ast_delete(r.output);
		} else {
			// Otherwise print error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	// Undefine and delete our parsers
	mpc_cleanup(8, Integer, Decimal, Number, OperatorSym, OperatorStr, Operator, Expr, Lsp);
	
	return 0;
	
}
