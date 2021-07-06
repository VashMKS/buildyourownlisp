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

// Forward declarations

struct lval;
struct lenv;

typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

// Data Structures

struct  lval {
	// Lsp value
	
	int type;
	
	// Possible data that the lval holds (dependent on type)
	double num;
	char* sym;
	char* err;
	lbuiltin fun; 
	int count;
	lval** cell;
};

enum { // Valid lval types
	   LVAL_NUM, LVAL_SYM,   LVAL_ERR,
	   LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

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

lval* lval_fun(lbuiltin func) {
	// Constructor for function lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->fun = func;
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
		case LVAL_FUN: break;
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

lval* lval_copy(lval* v) {
	// Copies an lval and returns a pointer to the copy
	
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
	
	switch (v->type) {
		case LVAL_NUM: x->num = v->num; break;
		case LVAL_FUN: x->fun = v->fun; break;
		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->sym, v->sym);
			break;
		case LVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err);
			break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(lval*) * x->count);
			for (int i = 0; i < x->count; i++) {
				x->cell[i] = lval_copy(v->cell[i]);
			}
			break;
	}
	
	return x;
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
	
	// Add element to the front
	w->cell[0] = v;
}

void lval_print(lval* v);

void lval_print_expr(lval* v, char open, char close) {
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
		case LVAL_FUN:   printf("<function>");         break;
		case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
		case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
	}
}

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

struct lenv {
	// Lsp environment
	int count;
	char** syms;
	lval** vals;
};

lenv* lenv_new(void) {
	// Constructor for lenv
	lenv* env = malloc(sizeof(lenv));
	env->count = 0;
	env->syms = NULL;
	env->vals = NULL;
	return env;
}

void lenv_del(lenv* env) {
	// Destructor for lenv
	for ( int i = 0; i < env->count; i++) {
		free(env->syms[i]);
		lval_del(env->vals[i]);
	}
	free(env->syms);
	free(env->vals);
	free(env);
}

lval* lenv_get(lenv* env, lval* k) {
	// Searches for a given symbol in an environment, returns it if found
	
	for (int i = 0; i < env->count; i++) {
		if (strcmp(env->syms[i], k->sym) == 0) {
			return lval_copy(env->vals[i]);
		}
	}
	
	return lval_err("Unbound Symbol");
}

void lenv_put(lenv* env, lval* k, lval* v) {
	// Inserts a variable (identifier-value pair) of lvals into the environment
	
	// Check to see if the variable already exists
	// If it is, replace it with the new value
	for (int i = 0; i < env->count; i++) {
		if (strcmp(env->syms[i], k->sym) == 0) {
			lval_del(env->vals[i]);
			env->vals[i] = lval_copy(v);
			return;
		}
	}
	
	// If variable does not exist, allocate and copy into the environment
	env->count++;
	env->vals = realloc(env->vals, sizeof(lval*) * env->count);
	env->syms = realloc(env->syms, sizeof(char*) * env->count);
	
	env->vals[env->count-1] = lval_copy(v);
	env->syms[env->count-1] = malloc(strlen(k->sym)+1);
	strcpy(env->syms[env->count-1], k->sym);
}

// Builtins

// The following preprocessor macro creates error checking code blocks
#define LASSERT(args, cond, err) \
	if (!(cond)) { lval_del(args); return lval_err(err); }

lval* lval_eval(lenv* env, lval* v);

lval* builtin_list(lenv* env, lval* args) {
	// Builtin function "list": Takes an S-expression and converts it to a Q-expression
	args->type = LVAL_QEXPR;
	return args;
}

lval* builtin_head(lenv* env, lval* args) {
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

lval* builtin_tail(lenv* env, lval* args) {
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

lval* builtin_init(lenv* env, lval* args) {
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

lval* builtin_eval(lenv* env, lval* args) {
	//  Builtin function "eval": Takes a Q-expression and evaluates it as if it were an S-expression
	
	LASSERT(args, args->count == 1,
		"Function 'eval' passed to many arguments, can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'eval' passed incorrect argument, can only take Q-expression");
	
	lval* v = lval_take(args, 0);
	v->type = LVAL_SEXPR;
	return lval_eval(env, v);
}

lval* builtin_join(lenv* env, lval* args) {
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

lval* builtin_cons(lenv* env, lval* args) {
	// Builtin function "cons": Takes a value and a Q-expression and appends it to the front
	
	LASSERT(args, args->count <= 2,
		"Function 'cons' passed too many arguments, must pass exactly two arguments");
		LASSERT(args, args->count >= 2,
		"Function 'cons' passed too few arguments, must pass exactly two arguments");
	LASSERT(args, args->cell[1]->type == LVAL_QEXPR,
		"Function 'cons' passed incorrect argument, second argument must be a Q-expression");
	
	lval* v = lval_pop(args, 1);
	lval_cons(lval_take(args, 0), v);
	return v;
}

lval* builtin_len(lenv* env, lval* args) {
	// Builtin function "len": Takes a Q-expression and returns a number lval with its length
	
	LASSERT(args, args->count == 1,
		"Function 'len' can only take one argument");
	LASSERT(args, args->cell[0]->type == LVAL_QEXPR,
		"Function 'len' passed incorrect argument, must be a Q-expression");
	
	lval* result = lval_num((double)args->cell[0]->count);
	lval_del(args);
	return result;
}

lval* builtin_op(lenv* env, lval* args, char* op) {
	// Apply a builtin arithmetic function to a list of arguments
	
	// Ensure all arguments are number lvals
	for (int i = 0; i < args->count; i++) {
		LASSERT(args, args->cell[i]->type == LVAL_NUM,
			"Invalid argument(s), must be number(s)");
	}
	
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
			// TODO: convert to assertion macro
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division by zero");
				break;
			}
			x->num /= y->num;
		}
		if (strcmp(op, "%") == 0) {
			// TODO: convert to assertion macro
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

lval* builtin_add(lenv* env, lval* args) {
	return builtin_op(env, args, "+");
}

lval* builtin_sub(lenv* env, lval* args) {
	return builtin_op(env, args, "-");
}

lval* builtin_mul(lenv* env, lval* args) {
	return builtin_op(env, args, "*");
}

lval* builtin_div(lenv* env, lval* args) {
	return builtin_op(env, args, "/");
}

lval* builtin_mod(lenv* env, lval* args) {
	return builtin_op(env, args, "%");
}

void lenv_add_builtin(lenv* env, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_fun(func);
	lenv_put(env, k, v);
	// lenv_put registers copies of the passed values so we must delete them now
	lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* env) {
	// List functions
	lenv_add_builtin(env, "list", builtin_list);
	lenv_add_builtin(env, "head", builtin_head);
	lenv_add_builtin(env, "tail", builtin_tail);
	lenv_add_builtin(env, "init", builtin_init);
	lenv_add_builtin(env, "eval", builtin_eval);
	lenv_add_builtin(env, "join", builtin_join);
	lenv_add_builtin(env, "cons", builtin_cons);
	lenv_add_builtin(env, "len", builtin_len);
	
	// Arithmetic functions
	lenv_add_builtin(env, "+", builtin_add);
	lenv_add_builtin(env, "-", builtin_sub);
	lenv_add_builtin(env, "*", builtin_mul);
	lenv_add_builtin(env, "/", builtin_div);
	lenv_add_builtin(env, "%", builtin_mod);
}

// Evaluation

lval* lval_eval_sexpr(lenv* env, lval* v) {
	// Evaluate children
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(env, v->cell[i]);
	}
	
	// Error checking
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}
	
	// Check for empty or single expression
	if (v->count == 0) { return v; }
	if (v->count == 1) { return lval_take(v, 0); }
	
	// Ensure first element is a function after evaluation
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval_del(f); lval_del(v);
		return lval_err("S-expression must start with a function");
	}
	
	// If so, call the function
	lval* result = f->fun(env, v);
	lval_del(f);
	return result;
}

lval* lval_eval(lenv* env, lval* v) {
	// If it's a symbol, go fetch the corresponding value in the environment
	if (v->type == LVAL_SYM) {
		lval* x = lenv_get(env, v);
		lval_del(v);
		return x;
	}
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(env, v); }
	return v;
}

// Reading

lval* lval_read_num(mpc_ast_t* t) {
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
		"                                                   \
		number  : /-?[0-9]+(\\.[0-9]*)?/ ;                  \
		symbol  : /[a-zA-Z0-9_+\\-*\\/%\\\\=<>!&]+/ ;       \
		sexpr   : '(' <expr>* ')' ;                         \
		qexpr   : '{' <expr>* '}' ;                         \
		expr    : <number> | <symbol> | <sexpr> | <qexpr> ; \
		lsp     : /^/ <expr>* /$/ ;                         \
		",
		Number, Symbol, Sexpr, Qexpr, Expr, Lsp);
	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.7");
	puts("Ctrl+C to exit\n");
	
	// Initialise environment
	lenv* env = lenv_new();
	lenv_add_builtins(env);
	
	// Endlessly prompt for input and reply back
	while (1) {
		
		// Get user input and add it to the history
		char* input = readline("lsp> ");
		add_history(input);
		
		// Attempt to parse user input
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lsp, &r)) {
			// On success evaluate AST and print result
			lval* result = lval_eval(env, lval_read(r.output));
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
	
	lenv_del(env);
	
	// Undefine and delete our parsers
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lsp);
	
	return 0;
	
}
