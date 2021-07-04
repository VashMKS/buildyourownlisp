// #include with "" instead of <> searches local folder first
#include "mpc.h"  // micro parser combinator lib

// Preprocessing directive below checks if the _WIN32 macro is defined, meaning we are on Windows
// There exist other similar predefined macros for other OS like __linux, __APPLE__ or __ANDROID__

#ifdef _WIN32

// Windows requires no libedit since the default behaviour on cmd gives those features
// Instead we provide fake substitutes

static char buffer[2048];

char* readline(char* prompt) {
	// Output prompt
	fputs(prompt, stdout);
	
	// Read user input into the buffer
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

// Valid lval types
enum {
	LVAL_NUM,
	LVAL_SYM,
	LVAL_SEXPR,
	LVAL_QEXPR,
	LVAL_ERR
};

typedef struct  lval {
	int type;
	
	// Possible data that the lval holds (dependent on type)
	double num;
	char* sym;
	int count;
	struct lval** cell;
	
	// Error information
	char* err;
} lval;

lval* lval_num(double x) {
	// Constructor for number lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_sym(char* s) {
	// Constructor symbol lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s)+1);
	strcpy(v->sym, s);
	return v;
}

lval* lval_sexpr(void) {
	// Constructor for S-expression lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void) {
	// Constructor for Q-expression lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_err(char* m) {
	// Constructor for error lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m)+1);
	strcpy(v->err, m);
	return v;
}

void lval_del(lval* v) {
	// Destructor for any kind of lval
	switch (v->type) {
		case LVAL_NUM: break;
		case LVAL_SYM: free(v->sym); break;
		case LVAL_ERR: free(v->err); break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			for (int i = 0; i < v->count; i++) {
				lval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
}

lval* lval_add(lval* v, lval* x) {
	// Appends an lval to an S-expression lval
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

lval* lval_pop(lval* v, int i) {
	// Takes an element from an S-expression and shifts the list back
	
	// Find the element we want to extract
	lval* w = v->cell[i];
	
	// Shift memory after the i-th element, decrease count and reallocate used memory
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	
	return w;
}

lval* lval_take(lval* v, int i) {
	// Takes an element from an S-expression and deletes the list
	lval* w = lval_pop(v, i);
	lval_del(v);
	return w;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {
		lval_print(v->cell[i]);
		if (i != (v->count-1)) { putchar(' '); }
	}
	putchar(close);
}

void lval_print(lval* v) {
	switch (v->type) {
		case LVAL_NUM:   printf("%g",  v->num);        break;
		case LVAL_SYM:   printf("%s",  v->sym);        break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
		case LVAL_ERR:   printf("Error: %s", v->err);  break;
	}
}

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

lval* builtin_op(lval* args, char* op) {
	// Apply a builtin operator to a list of arguments
	
	// Ensure all arguments are lval of type number
	for (int i = 0; i < args->count; i++) {
		if (args->cell[i]->type != LVAL_NUM) {
			lval_del(args);
			return lval_err("Invalid argument(s)");
		}
	}
	
	// Pop the first element
	lval* x = lval_pop(args, 0);
	
	// Perform unary negation if applicable
	if ((strcmp(op, "-") == 0) && args->count == 0) {
		x->num = -x->num;
	}
	
	// fold operation over all arguments
	while (args->count > 0) {
		lval* y = lval_pop(args, 0);
		if (strcmp(op, "+") == 0) { x->num += y->num; }
		if (strcmp(op, "-") == 0) { x->num -= y->num; }
		if (strcmp(op, "*") == 0) { x->num *= y->num; }
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division by zero");
				break;
			}
			x->num /= y->num;
		}
		if (strcmp(op, "%") == 0) { 
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Remainder on division by zero");
				break;
			}
			x = lval_num(remainder(x->num, y->num));
		}
		lval_del(y);
	}
	
	// delete arguments and return result
	lval_del(args);
	return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
	// Evaluate children
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(v->cell[i]);
	}
	
	// Error checking
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}
	
	// Check for empty or single expression
	if (v->count == 0) { return v; }
	if (v->count == 1) { return lval_take(v, 0); }
	
	// Ensure first element is symbol
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_SYM) {
		lval_del(f); lval_del(v);
		return lval_err("S-expression must start with a symbol");
	}
	
	// Call builtin operator
	lval* result = builtin_op(v, f->sym);
	lval_del(f);
	return result;
}

lval* lval_eval(lval* v) {
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
	return v;
}

lval* lval_read_num(mpc_ast_t* t) {
	// If the representation contains a dot, it is a decimal number, otherwise an integer
	errno = 0;
	double x = strtod(t->contents, NULL);
	return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval* lval_read(mpc_ast_t* t) {

	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	
	// If root (>) or sexpr then create empty list
	lval* v = NULL;
	if (strcmp(t->tag, ">") == 0) { v = lval_sexpr(); }
	if (strstr(t->tag, "sexpr"))  { v = lval_sexpr(); }
	if (strstr(t->tag, "qexpr"))  { v = lval_qexpr(); }
	
	// Fill the list with any valid expression contained within
	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		v = lval_add(v, lval_read(t->children[i]));
	}
	
	return v;
}


int main(int argc, char** argv) {
	
	// Declare parsers
	mpc_parser_t* Number      = mpc_new("number");
	mpc_parser_t* Symbol      = mpc_new("symbol");
	mpc_parser_t* Sexpr       = mpc_new("sexpr");
	mpc_parser_t* Qexpr       = mpc_new("qexpr");
	mpc_parser_t* Expr        = mpc_new("expr");
	mpc_parser_t* Lsp         = mpc_new("lsp");

	// Define them
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                   \
		number  : /-?[0-9]+(\\.[0-9]*)?/ ;                  \
		symbol  : '+' | '-' | '*' | '/' | '%' ;             \
		sexpr   : '(' <expr>* ')' ;                         \
		qexpr   : '{' <expr>* '}' ;                         \
		expr    : <number> | <symbol> | <sexpr> | <qexpr> ; \
		lsp     : /^/ <expr>* /$/ ;                         \
		",
		Number, Symbol, Sexpr, Qexpr, Expr, Lsp);
	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.5");
	puts("Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		
		char* input = readline("lsp> ");
		add_history(input);
		
		// Attempt to parse user input
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lsp, &r)) {
			// On success evaluate AST and print result
			lval* result = lval_eval(lval_read(r.output));
			lval_println(result);
			lval_del(result);
			mpc_ast_delete(r.output);
		} else {
			// Otherwise print parse error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	// Undefine and delete our parsers
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lsp);
	
	return 0;
	
}
