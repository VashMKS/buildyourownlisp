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

// Lsp value, either a number or an error
typedef struct {
	int type;
	union Number {
		long integer;
		double real;
	} num;
	int err;
} lval;

// Valid lval types
enum {
	LVAL_INT,
	LVAL_REAL,
	LVAL_ERR
};

// Valid lval errors
enum {
	LERR_DIV_ZERO, // Produced on division by zero
	LERR_BAD_OP,   // Produced on invalid operator
	LERR_BAD_NUM   // Produced on invalid number
};

// Creates an integer lval
lval lval_int(long x) {
	lval v;
	v.type = LVAL_INT;
	v.num.integer = x;
	return v;
}

// Creates a real lval
lval lval_real(double x) {
	lval v;
	v.type = LVAL_REAL;
	v.num.real = x;
	return v;
}

// Creates an error lval
lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

// Print an lval, handling all possible cases
void lval_print(lval v) {
	switch (v.type) {
		case LVAL_INT:
			printf("%li", v.num.integer); break;
		case LVAL_REAL:
			printf("%g", v.num.real); break;
		case LVAL_ERR:
			if (v.err == LERR_DIV_ZERO) {
				printf("Error: Division by zero");
			}
			if (v.err == LERR_BAD_OP) {
				printf("Error: Invalid operator");
			}
			if (v.err == LERR_BAD_NUM) {
				printf("Error: Invalid number");
			}
			break;
	}
}

// Print an lval, followed by a newline
void lval_println(lval v) {
	lval_print(v);
	putchar('\n');
}

lval eval_op(lval x, char* op, lval y) {
	// If either lval is an error, return it
	if (x.type == LVAL_ERR) { return x; }
	if (y.type == LVAL_ERR) { return y; }
	
	// Otherwise, evaluate the operators, accounting for all possible combinations
	if (x.type == LVAL_REAL && y.type == LVAL_REAL) {
		if (strcmp(op, "+") == 0) { return lval_real(x.num.real + y.num.real); }
		if (strcmp(op, "-") == 0) { return lval_real(x.num.real - y.num.real); }
		if (strcmp(op, "*") == 0) { return lval_real(x.num.real * y.num.real); }
		if (strcmp(op, "/") == 0) {
			if (y.num.real == 0) return lval_err(LERR_DIV_ZERO); 
			else return lval_real(x.num.real / y.num.real);
		}
	}
	if (x.type == LVAL_REAL && y.type == LVAL_INT) {
		if (strcmp(op, "+") == 0) { return lval_real(x.num.real + (double)y.num.integer); }
		if (strcmp(op, "-") == 0) { return lval_real(x.num.real - (double)y.num.integer); }
		if (strcmp(op, "*") == 0) { return lval_real(x.num.real * (double)y.num.integer); }
		if (strcmp(op, "/") == 0) {
			if (y.num.integer == 0) return lval_err(LERR_DIV_ZERO); 
			else return lval_real(x.num.real / (double)y.num.integer);
		}
	}
	if (x.type == LVAL_INT && y.type == LVAL_REAL) {
		if (strcmp(op, "+") == 0) { return lval_real((double)x.num.integer + y.num.real); }
		if (strcmp(op, "-") == 0) { return lval_real((double)x.num.integer - y.num.real); }
		if (strcmp(op, "*") == 0) { return lval_real((double)x.num.integer * y.num.real); }
		if (strcmp(op, "/") == 0) {
			if (y.num.real == 0) return lval_err(LERR_DIV_ZERO); 
			else return lval_real((double)x.num.integer / y.num.real);
		}
	}
	if (x.type == LVAL_INT && y.type == LVAL_INT) {
		if (strcmp(op, "+") == 0) { return lval_int(x.num.integer + y.num.integer); }
		if (strcmp(op, "-") == 0) { return lval_int(x.num.integer - y.num.integer); }
		if (strcmp(op, "*") == 0) { return lval_int(x.num.integer * y.num.integer); }
		if (strcmp(op, "/") == 0) {
			if (y.num.integer == 0) return lval_err(LERR_DIV_ZERO); 
			else return lval_int(x.num.integer / y.num.integer);
		}
		if (strcmp(op, "%") == 0) { return lval_int(x.num.integer % y.num.integer); }
	}
	
	// If operator is not recognized return error
	return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {

	// If tagged a number, evaluate to long/double appropriately
	if (strstr(t->tag, "integer")) {
		// Error if conversion fails
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE ? lval_int(x) : lval_err(LERR_BAD_NUM);
	}
	if (strstr(t->tag, "decimal")) {
		// Error if conversion fails
		errno = 0;
		double x = strtod(t->contents, NULL);
		return errno != ERANGE ? lval_real(x) : lval_err(LERR_BAD_NUM);
	}
	
	// Operator is always the second child
	char* op = t->children[1]->contents;
	
	// Store third child in x
	lval x = eval(t->children[2]);
	
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
	mpc_parser_t* Integer     = mpc_new("integer");
	mpc_parser_t* Decimal     = mpc_new("decimal");
	mpc_parser_t* Number      = mpc_new("number");
	mpc_parser_t* Operator    = mpc_new("operator");
	mpc_parser_t* Expr        = mpc_new("expr");
	mpc_parser_t* Lsp         = mpc_new("lsp");

	// Define them
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                  \
		integer  : /-?[0-9]+/ ;                            \
		decimal  : /-?[0-9]+\\.[0-9]*/ ;                   \
		number   : <decimal> | <integer> ;                 \
		operator : '+' | '-' | '*' | '/' | '%' ;           \
		expr     : <number> | '(' <operator> <expr>+ ')' ; \
		lsp      : /^/ <operator> <expr>+ /$/ ;            \
		",
		Integer, Decimal, Number, Operator, Expr, Lsp);

	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.3");
	puts("Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		
		char* input = readline("lsp> ");
		add_history(input);
		
		// Attempt to parse user input
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lsp, &r)) {
			// On success evaluate AST and print result
			lval result = eval(r.output);
			lval_println(result);
			mpc_ast_delete(r.output);
		} else {
			// Otherwise print error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	// Undefine and delete our parsers
	mpc_cleanup(6, Integer, Decimal, Number, Operator, Expr, Lsp);
	
	return 0;
	
}
