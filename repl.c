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

enum { // Valid lval types
	   LVAL_NUM, LVAL_SYM,   LVAL_ERR,
	   LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

char* ltype_name(int t) {
	switch(t) {
		case LVAL_NUM: return "Number";
		case LVAL_SYM: return "Symbol";
		case LVAL_ERR: return "Error";
		case LVAL_FUN: return "Function";
		case LVAL_SEXPR: return "S-Expression";
		case LVAL_QEXPR: return "Q-Expression";
		default: return "Unknown";
	}
}

// Data Structures

struct  lval {
	int type;
	
	double num; // Number
	char* sym;  // Symbol
	char* err;  // Error
	
	// Builtin function
	lbuiltin builtin;
	// User-defined function
	lenv* env;
	lval* formals;
	lval* body;
	
	// Expression
	int count;
	lval** cell;
};

struct lenv {
	lenv* parent;
	int count;
	char** syms;
	lval** vals;
};

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

lval* lval_builtin(lbuiltin func) {
	// Constructor for builtin function lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = func;
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

lval* lval_err(char* fmt, ...) {
	// Constructor for error lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	
	va_list va;
	va_start(va, fmt);
	
	// Fixed 512B buffer
	v->err = malloc(512);
	
	// printf the error string using the format string and arguments provided
	vsnprintf(v->err, 511, fmt, va);
	
	// Reallocate to the actual size used
	v->err = realloc(v->err, strlen(v->err)+1);
	
	va_end(va);
	return v;
}

lenv* lenv_new(void);

lval* lval_lambda(lval* formals, lval* body) {
	// Constructor for user-defined function lval
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = NULL;
	v->env = lenv_new();
	v->formals = formals;
	v->body = body;
	return v;
}

void lenv_del(lenv* env);

void lval_del(lval* v) {
	// Destructor for any kind of lval
	switch (v->type) {
		case LVAL_NUM: break;
		case LVAL_FUN: 
			if (!v->builtin) {
				lenv_del(v->env);
				lval_del(v->formals);
				lval_del(v->body);
			}
			break;
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

lenv* lenv_copy(lenv* env);

lval* lval_copy(lval* v) {
	// Copies an lval and returns a pointer to the copy
	
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
	
	switch (v->type) {
		case LVAL_NUM: x->num = v->num; break;
		case LVAL_FUN:
			if (v->builtin) {
				x->builtin = v->builtin;
			} else {
				x->builtin = NULL;
				x->env = lenv_copy(v->env);
				x->formals = lval_copy(v->formals);
				x->body = lval_copy(v->body);
			}
			break;
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

lval* lval_join(lval* v, lval* w) {
	// Combines two S-expressions into one
	for (int i = 0; i < w->count; i++) { v = lval_add(v, w->cell[i]); }
	w->count = 0; lval_del(w);
	// The above line deletes w but not its children since they now belong to v
	// If it causes problems, we can use
	// free(w->cell); free(w);
	// which was used in the book and does not depend on lval_del, but I'm a bit conflicted about 
	// it... reasoning being that if the deleter changes in the future in a way that does not
	// account for this, we might run into hard to spot leaks and/or double frees
	// This comment itself, sponsored by the double free I just found here ;)
	return v;
}

lval* lval_pop(lval* v, int i) {
	// Takes an element from an S-expression and shifts the list back
	
	// Find the element to extract
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


void lval_cons(lval* v, lval* w) {
	// Appends a value to the beginning of an S-expression
	
	// Allocate space for one more element on the list, shift to the right and increase count
	w->cell = realloc(w->cell, sizeof(lval*) * w->count+1);
	memmove(&w->cell[1], &w->cell[0], sizeof(lval*) * w->count);
	w->count++;
	
	// Add element to the front
	w->cell[0] = v;
}

lenv* lenv_new(void) {
	// Constructor for lenv
	lenv* env = malloc(sizeof(lenv));
	env->parent = NULL;
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

lenv* lenv_copy(lenv* env) {
	lenv* new_env = malloc(sizeof(lenv));
	new_env->parent = env->parent;
	new_env->count  = env->count;
	new_env->syms = malloc(sizeof(char*) * new_env->count);
	new_env->vals = malloc(sizeof(lval*) * new_env->count);
	for (int i = 0; i < env->count; i++) {
		new_env->syms[i] = malloc(strlen(env->syms[i]) + 1);
		strcpy(new_env->syms[i], env->syms[i]);
		new_env->vals[i] = lval_copy(env->vals[i]);
	}
	return new_env;
}

lval* lenv_get(lenv* env, lval* key) {
	// Searches for a given symbol in an environment, returns it if found
	
	for (int i = 0; i < env->count; i++) {
		if (strcmp(env->syms[i], key->sym) == 0) {
			return lval_copy(env->vals[i]);
		}
	}
	
	if (env->parent) { return lenv_get(env->parent, key); }
	else {             return lval_err("Unbound symbol '%s'", key->sym); }
}

void lenv_put(lenv* env, lval* key, lval* value) {
	// Inserts a variable (identifier-value pair) of lvals into an environment
	
	// Check to see if the variable already exists
	// If it does, replace it with the new value
	for (int i = 0; i < env->count; i++) {
		if (strcmp(env->syms[i], key->sym) == 0) {
			lval_del(env->vals[i]);
			env->vals[i] = lval_copy(value);
			return;
		}
	}
	
	// If variable does not exist, allocate and copy into the environment
	env->count++;
	env->vals = realloc(env->vals, sizeof(lval*) * env->count);
	env->syms = realloc(env->syms, sizeof(char*) * env->count);
	
	env->vals[env->count-1] = lval_copy(value);
	env->syms[env->count-1] = malloc(strlen(key->sym)+1);
	strcpy(env->syms[env->count-1], key->sym);
}

void lenv_def(lenv* env, lval* key, lval* value) {
	// Put a variable in the outermost environment
	while (env->parent) { env = env->parent; }
	lenv_put(env, key, value);
}

void lval_print(lval* v);

void lval_print_fun(lval* v) {
	if (v->builtin) {
		printf("builtin function");
	} else {
		printf("function (");
		lval_print(v->formals);
		printf(" -> ");
		// TODO: if function is curried, print bound arguments as their values and not their names
		// Simpler alternative: print the function's env after the function's expression
		lval_print(v->body);
		putchar(')');
	}
}

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
		case LVAL_FUN:   lval_print_fun(v);            break;
		case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
		case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
	}
}

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

void lenv_print(lenv* env) {
	// Prints all named values in the environment
	printf("Bound values:\n");
	for (int i = 0; i < env->count; i++) {
		printf("%s %s\n", ltype_name(env->vals[i]->type), env->syms[i]);
	}
}

// Builtins

// The following preprocessor macros create error checking code blocks
#define LASSERT(args, cond, fmt, ...) \
	if (!(cond)) { \
		lval* error = lval_err(fmt, ##__VA_ARGS__); \
		lval_del(args); \
		return error; \
}

#define LASSERT_TYPE(func, args, index, expected) \
	LASSERT(args, args->cell[index]->type == expected, \
		"Function '%s' passed incorrect type for argument %i. Expected %s, was given %s", \
		func, index, ltype_name(expected), ltype_name(args->cell[index]->type));

#define LASSERT_NUM_ARGS(func, args, n) \
	LASSERT(args, args->count == n, \
		"Function '%s' passed incorrect number of arguments. Expected %i, was given %i", \
		func, n, args->count);

#define LASSERT_NON_EMPTY(func, args, index) \
	LASSERT(args, args->cell[index]->count != 0, \
		"Function '%s' passed empty %s, must contain at least one element", \
		func, ltype_name(args->cell[index]->type));

lval* lval_eval(lenv* env, lval* v);

lval* builtin_lambda(lenv* env, lval* args) {
	// Builtin function "lambda": Takes a list of arguments and a body in a Q-expression and
	// generates the proper function lval
	
	LASSERT_NUM_ARGS("lambda", args, 2);
	LASSERT_TYPE("lambda", args, 0, LVAL_QEXPR);
	LASSERT_TYPE("lambda", args, 1, LVAL_QEXPR);
	
	// Formal arguments can only be symbols
	for (int i = 0; i < args->cell[0]->count; i++) {
		LASSERT(args, (args->cell[0]->cell[i]->type == LVAL_SYM),
			"Function cannot take non-%s as argument, was given %s",
			ltype_name(LVAL_SYM), ltype_name(args->cell[0]->cell[i]->type));
	}
	
	lval* formals = lval_pop(args, 0);
	lval* body = lval_pop(args, 0);
	lval_del(args);
	
	return lval_lambda(formals, body);
}

lval* builtin_list(lenv* env, lval* args) {
	// Builtin function "list": Takes an S-expression and converts it to a Q-expression
	args->type = LVAL_QEXPR;
	return args;
}

lval* builtin_head(lenv* env, lval* args) {
	// Builtin function "head": Takes a Q-expression and returns the first element
	
	LASSERT_NUM_ARGS("head", args, 1);
	LASSERT_TYPE("head", args, 0, LVAL_QEXPR);
	LASSERT_NON_EMPTY("head", args, 0);
	
	lval* v = lval_take(args, 0);
	while (v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval* builtin_tail(lenv* env, lval* args) {
	// Builtin function "tail": Takes a Q-expression, removes the first element and returns it
	
	LASSERT_NUM_ARGS("tail", args, 1);
	LASSERT_TYPE("tail", args, 0, LVAL_QEXPR);
	LASSERT_NON_EMPTY("tail", args, 0);
	
	lval* v = lval_take(args, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_eval(lenv* env, lval* args) {
	//  Builtin function "eval": Takes a Q-expression and evaluates it as if it were an S-expression
	
	LASSERT_NUM_ARGS("eval", args, 1)
	LASSERT_TYPE("eval", args, 0, LVAL_QEXPR);
	
	lval* v = lval_take(args, 0);
	v->type = LVAL_SEXPR;
	return lval_eval(env, v);
}

lval* builtin_join(lenv* env, lval* args) {
	// Builtin function "join": Takes several Q-expression and concatenates them into a single one
	
	for (int i = 0; i < args->count; i++) {
		LASSERT_TYPE("join", args, i, LVAL_QEXPR);
	}
	
	lval* v = lval_pop(args, 0);
	while (args->count) { v = lval_join(v, lval_pop(args, 0)); }
	
	lval_del(args);
	return v;
}

lval* builtin_var(lenv* env, lval* args, char* func) {
	
	LASSERT_TYPE(func, args, 0, LVAL_QEXPR);
	
	// First argument is the list of symbols
	lval* syms = args->cell[0];
	
	for (int i = 0; i < syms->count; i++) {
		LASSERT(args, (syms->cell[i]->type == LVAL_SYM),
			"Function '%s' cannot define non-symbols. Expected %s, was given %s",
			func, ltype_name(LVAL_SYM), ltype_name(syms->cell[i]->type));
	}
	
	LASSERT(args, (syms->count == args->count-1),
		"Function '%s' cannot define mismatched number of values to symbols. "
		"Was given %i symbol(s) but %i value(s).",
		func, syms->count, args->count-1);
	
	for (int i = 0; i < syms->count; i++) {
		if (strcmp(func, "def") == 0) { lenv_def(env, syms->cell[i], args->cell[i+1]); }
		if (strcmp(func, "=")   == 0) { lenv_put(env, syms->cell[i], args->cell[i+1]); }
	}
	
	lval_del(args);
	return lval_sexpr();  // On success, return empty list
}

lval* builtin_def(lenv* env, lval* args) {
	// Builtin function "def": Takes symbol and value lists and registers each pair
	// to the outermost environment
	return builtin_var(env, args, "def");
}

lval* builtin_put(lenv* env, lval* args) {
	// Builtin function "=": Takes symbol and value lists and registers each pair
	// to the environment
	return builtin_var(env, args, "=");
}

lval* builtin_fun(lenv* env, lval* args) {
	// Builtin function "fun": Takes a name, argument Q-expression and body Q-expression and
	// defines that function. Same as combining def and lambda
	
	LASSERT_NUM_ARGS("fun", args, 2)
	LASSERT_TYPE("fun", args, 0, LVAL_QEXPR);
	LASSERT_TYPE("fun", args, 1, LVAL_QEXPR);
	
	// First element of first list must be the name of the function i.e. a symbol
	LASSERT(args, (args->cell[0]->count >= 1), "Invalid function definition. Must give a name");
	LASSERT(args, (args->cell[0]->cell[0]->type == LVAL_SYM),
		"Invalid function definition. "
		"First element of argument 0 (function name) must be %s, was given %s",
		ltype_name(LVAL_SYM), ltype_name(args->cell[0]->type));
	
	// Formal arguments can only be symbols
	for (int i = 1; i < args->cell[0]->count; i++) {
		LASSERT(args, (args->cell[0]->cell[i]->type == LVAL_SYM),
			"Function cannot take non-%s as argument, was given %s",
			ltype_name(LVAL_SYM), ltype_name(args->cell[0]->cell[i]->type));
	}
	
	// Get the name and wrap it into a Q-expression
	lval* syms = lval_qexpr();
	lval_add(syms, lval_pop(args->cell[0], 0));
	
	// Wrap the name and function into an S-expression for builtin_def
	lval* def_args = lval_sexpr();
	lval_add(def_args, syms);
	lval_add(def_args, builtin_lambda(env, args));
	
	return builtin_def(env, def_args);
}

lval* builtin_init(lenv* env, lval* args) {
	// Builtin function "init": Takes a Q-expression, removes the last element and returns it
	
	LASSERT_NUM_ARGS("init", args, 1);
	LASSERT_TYPE("init", args, 0, LVAL_QEXPR);
	LASSERT_NON_EMPTY("init", args, 0);
	
	lval* v = lval_take(args, 0);
	lval_del(lval_pop(v, v->count - 1));
	return v;
}

lval* builtin_cons(lenv* env, lval* args) {
	// Builtin function "cons": Takes a value and a Q-expression and appends it to the front
	
	LASSERT_NUM_ARGS("cons", args, 2)
	LASSERT_TYPE("cons", args, 1, LVAL_QEXPR);
	
	lval* v = lval_pop(args, 1);
	lval_cons(lval_take(args, 0), v);
	return v;
}

lval* builtin_len(lenv* env, lval* args) {
	// Builtin function "len": Takes a Q-expression and returns a number lval with its length
	
	LASSERT_NUM_ARGS("len", args, 1)
	LASSERT_TYPE("len", args, 0, LVAL_QEXPR);
	
	lval* result = lval_num((double)args->cell[0]->count);
	lval_del(args);
	return result;
}

lval* builtin_op(lenv* env, lval* args, char* op) {
	// Apply a builtin arithmetic function to a list of arguments
	
	// Ensure all arguments are number lvals
	for (int i = 0; i < args->count; i++) {
		LASSERT_TYPE(op, args, i, LVAL_NUM);
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

lval* builtin_add(lenv* env, lval* args) { return builtin_op(env, args, "+"); }
lval* builtin_sub(lenv* env, lval* args) { return builtin_op(env, args, "-"); }
lval* builtin_mul(lenv* env, lval* args) { return builtin_op(env, args, "*"); }
lval* builtin_div(lenv* env, lval* args) { return builtin_op(env, args, "/"); }
lval* builtin_mod(lenv* env, lval* args) { return builtin_op(env, args, "%"); }

lval* builtin_print_env(lenv* env, lval* args) {
	// Builtin function "env": prints all named values in the environment
	lenv_print(env);
	return lval_sexpr();
}

lval* builtin_exit() {
	// Builtin function "exit": returns an error code that tells the REPL to end the session
	return lval_err("LSP_REPL_EXIT_SEQUENCE");
}

void lenv_add_builtin(lenv* env, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_builtin(func);
	lenv_put(env, k, v);
	// lenv_put registers copies of the passed values so we must delete them now
	lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* env) {
	// REPL functions
	lenv_add_builtin(env, "exit", builtin_exit);

	// Variable functions
	lenv_add_builtin(env, "def", builtin_def);
	lenv_add_builtin(env, "=",   builtin_put);
	lenv_add_builtin(env, "lambda", builtin_lambda);
	lenv_add_builtin(env, "fun", builtin_fun);
	lenv_add_builtin(env, "env", builtin_print_env);
	
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

lval* lval_call(lenv* env, lval* f, lval* args) {
	// Evaluate a function lval with the given input arguments
	
	// If builtin, simply use the stored function pointer
	if (f->builtin) { return f->builtin(env, args); }
	
	int given = args->count;
	int total = f->formals->count;
	
	while (args->count) {
		if (f->formals->count == 0) {
			lval_del(args);
			return lval_err("Function passed too many arguments. Expected %i, was given %i",
							total, given);
		}
		// Bind the next formal argument to the next provided value
		lval* sym = lval_pop(f->formals, 0);
		
		// Special case: variable length arguments
		if (strcmp(sym->sym, "&") == 0) {
			if (f->formals->count != 1) {
				lval_del(args);
				return lval_err("Invalid function call. "
				                "'&' must be followed by a single symbol, got %i",
				                f->formals->count);
			}
			// Next (i.e. the last) formal must be bound to the remaining arguments
			lval* nsym = lval_pop(f->formals, 0);
			lenv_put(f->env, nsym, builtin_list(env, args));
			lval_del(sym); lval_del(nsym);
			break;
		}
		
		lval* val = lval_pop(args, 0);
		lenv_put(f->env, sym, val);
		lval_del(sym); lval_del(val);
	}
	
	lval_del(args);
	
	// Handle variable arguments ('&') with 0 optional arguments passed by binding an empty list
	if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
		if (f->formals->count != 2) {
			return lval_err("Function format invalid. "
			                "'&' must be followed by a single symbol, got %i",
			                f->formals->count);
		}
		lval_del(lval_pop(f->formals, 0));
		lval* sym = lval_pop(f->formals, 0);
		lval* val = lval_qexpr();
		lenv_put(f->env, sym, val);
		lval_del(sym); lval_del(val);
	}
	
	// If all formals have been bound then evaluate
	if (f->formals->count == 0) {
		f->env->parent = env;  // parent env in the function is the one from which it is called
		return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
	} else { return lval_copy(f); /* Otherwise return partially evaluated function (currying) */ }
}

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
	if (v->count == 1) { return lval_eval(env, lval_take(v, 0)); }
	
	// Ensure first element is a function after evaluation
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval* error = lval_err("S-expression starts with incorrect type. "
							   "Expected %s, was given %s",
							   ltype_name(LVAL_FUN), ltype_name(f->type));
		lval_del(f); lval_del(v);
		return error;
	}
	
	// If so, call the function
	lval* result = lval_call(env, f, v);
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
	if (errno != ERANGE) { return lval_num(x); }
	else { return lval_err("Invalid number. could not parse %s to Number", t->contents); }
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
	puts("Lsp version 0.0.0.0.8");
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
			if (result->type == LVAL_ERR && strcmp(result->err, "LSP_REPL_EXIT_SEQUENCE") == 0) {
				// TODO: this is a VERY janky way to exit the terminal by reserving a certain error
				// code. Unsure of how to do a better method that does not involve polluting the
				// namespace or too much unnecessary computation
				break;
			}
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
