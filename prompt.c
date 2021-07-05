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
		case LVAL_QEXPR:
		case LVAL_SEXPR:
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
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
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

lval* lval_join(lval* v, lval* w) {
	// Combines two S-expressions into one
	while (w->count) { v = lval_add(v, lval_pop(w, 0)); }
	lval_del(w);
	return v;
}

void lval_cons(lval* v, lval* w) {
	// Appends a value to the beginning of an S-expression
	
	// Allocate space for one more element on the list, shift to the right and increase count
	w->cell = realloc(w->cell, sizeof(lval*) * w->count+1);
	memmove(&w->cell[1], &w->cell[0], sizeof(lval*) * w->count);
	w->count++;
	
	// Add element to the front and then delete it
	w->cell[0] = v;
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
		case LVAL_ERR:   printf("Error: %s", v->err);  break;
		case LVAL_SYM:   printf("%s",  v->sym);        break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

// The following preprocessor macro creates error checking code blocks
#define LASSERT(args, cond, err) \
	if (!(cond)) { lval_del(args); return lval_err(err); }

lval* lval_eval(lval* v);

lval* builtin_list(lval* args) {
	// Builtin function "list": Takes an S-expression and converts it to a Q-expression
	args->type = LVAL_QEXPR;
	return args;
}

lval* builtin_head(lval* args) {
	// Builtin function "head": Takes a Q-expression and returns the first element
	
	LASSERT(args, args->count == 1,
		"Function 'head' passed to many arguments, can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'head' passed incorrect argument, can only take Q-expression");
	LASSERT(args, args->cell[0]->count != 0,
		"Function 'head' passed empty Q-expression, it must contain at least one element");
	
	lval* v = lval_take(args, 0);
	while (v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval* builtin_tail(lval* args) {
	// Builtin function "tail": Takes a Q-expression, removes the first element and returns it
	
	LASSERT(args, args->count == 1,
		"Function 'tail' passed to many arguments, can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'tail' passed incorrect argument, can only take Q-expression");
	LASSERT(args, args->cell[0]->count != 0,
		"Function 'tail' passed empty Q-expression, it must contain at least one element");
	
	lval* v = lval_take(args, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_init(lval* args) {
	// Builtin function "init": Takes a Q-expression, removes the last element and returns it
	
	LASSERT(args, args->count == 1,
		"Function 'init' can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'init' passed incorrect argument, must be a Q-expression");
	LASSERT(args, args->cell[0]->count != 0,
		"Function 'init' passed empty Q-expression, it must contain at least one element");
	
	lval* v = lval_take(args, 0);
	lval_del(lval_pop(v, v->count - 1));
	return v;
}

lval* builtin_eval(lval* args) {
	//  Builtin function "eval": Takes a Q-expression and evaluates it as if it were an S-expression
	
	LASSERT(args, args->count == 1,
		"Function 'eval' passed to many arguments, can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'eval' passed incorrect argument, can only take Q-expression");
	
	lval* v = lval_take(args, 0);
	v->type = LVAL_SEXPR;
	return lval_eval(v);
}

lval* builtin_join(lval* args) {
	// Builtin function "join": Takes several Q-expression and concatenates them into a single one
	
	for (int i = 0; i < args->count; i++) {
		LASSERT(args, args->cell[i]->type == LVAL_QEXPR,
			"Function 'join' passed incorrect arguments, can only take Q-expressions");
	}
	
	lval* v = lval_pop(args, 0);
	while (args->count) { v = lval_join(v, lval_pop(args, 0)); }
	
	lval_del(args);
	return v;
}

lval* builtin_cons(lval* args) {
	// Builtin function "cons": Takes a value and a Q-expression and appends it to the front
	
	LASSERT(args, args->count <= 2,
		"Function 'cons' passed too many arguments, must pass exactly two arguments");
		LASSERT(args, args->count >= 2,
		"Function 'cons' passed too few arguments, must pass exactly two arguments");
	LASSERT(args, args->cell[1]->type == LVAL_QEXPR,
		"Function 'cons' passed incorrect argument, second argument must be a Q-expression");
	
	lval* v = lval_pop(args, 1);
	lval_cons(lval_pop(args, 0), v);
	lval_del(args);
	return v;
}

lval* builtin_len(lval* args) {
	// Builtin function "len": Takes a Q-expression and returns a number lval with its length
	
	LASSERT(args, args->count == 1,
		"Function 'len' can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'len' passed incorrect argument, must be a Q-expression");
	
	lval* result = lval_num((double)args->cell[0]->count);
	lval_del(args);
	return result;
}

lval* builtin_op(lval* args, char* op) {
	// Apply a builtin operator to a list of arguments
	
	// Ensure all arguments are number lvals
	for (int i = 0; i < args->count; i++) {
		if (args->cell[i]->type != LVAL_NUM) {
			lval_del(args);
			return lval_err("Invalid argument(s), must be number(s)");
		}
	}
	
	// Pop the first element
	lval* x = lval_pop(args, 0);
	
	// Perform unary negation if applicable
	if ((strcmp(op, "-") == 0) && args->count == 0) { x->num = -x->num; }
	
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
			x->num = remainder(x->num, y->num);
		}
		lval_del(y);
	}
	
	lval_del(args);
	return x;
}

lval* builtin(lval* args, char* func) {
	if (strcmp("list", func) == 0) { return builtin_list(args); }
	if (strcmp("head", func) == 0) { return builtin_head(args); }
	if (strcmp("tail", func) == 0) { return builtin_tail(args); }
	if (strcmp("init", func) == 0) { return builtin_init(args); }
	if (strcmp("join", func) == 0) { return builtin_join(args); }
	if (strcmp("eval", func) == 0) { return builtin_eval(args); }
	if (strcmp("cons", func) == 0) { return builtin_cons(args); }
	if (strcmp("len",  func) == 0) { return builtin_len(args); }
	if (strstr("+-/*%", func))     { return builtin_op(args, func); }
	lval_del(args);
	return lval_err("Unknown operation"); // TODO: specify func in the error message
}

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
	lval* result = builtin(v, f->sym);
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
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
		v = lval_add(v, lval_read(t->children[i]));
	}
	
	return v;
}


int main(int argc, char** argv) {
	
	// Declare parsers
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr  = mpc_new("sexpr");
	mpc_parser_t* Qexpr  = mpc_new("qexpr");
	mpc_parser_t* Expr   = mpc_new("expr");
	mpc_parser_t* Lsp    = mpc_new("lsp");

	// Define them
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                              \
		number  : /-?[0-9]+(\\.[0-9]*)?/ ;                             \
		symbol  : \"list\" | \"head\" | \"tail\" | \"init\"            \
		        | \"join\" | \"cons\" | \"len\"  | \"eval\"            \
		        | '+' | '-' | '*' | '/' | '%' ;                        \
		sexpr   : '(' <expr>* ')' ;                                    \
		qexpr   : '{' <expr>* '}' ;                                    \
		expr    : <number> | <symbol> | <sexpr> | <qexpr> ;            \
		lsp     : /^/ <expr>* /$/ ;                                    \
		",
		Number, Symbol, Sexpr, Qexpr, Expr, Lsp);
	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.6");
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
