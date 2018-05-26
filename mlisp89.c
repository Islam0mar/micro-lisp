/* micro lisp (C) A. Carl Douglas */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define debug(m,e) printf("%s:%d: %s:",__FILE__,__LINE__,m); print_obj(e,1); puts("");
typedef struct Object {
  enum { _Symbol, _Pair, _Primitive, _Closure, _Macro, _Syntax } tag;
  union Value {
    const char * string;
    struct Object * (*pfn) (struct Object *);
    struct Object * (*psyn) (struct Object *, struct Object *);
    struct List {
      struct Object * next;
      struct Object * data;
    } pair;
    struct Closure {
      struct Object * params;
      struct Object * body;
      struct Object * env;
    } closure;
  } value;
} Object;
Object * symbols = 0;
static int look; /* look ahead character */
#define SYMBOL_MAX  32
static char token[SYMBOL_MAX]; /* token */
int is_space(char x)  { return x == ' ' || x == '\n'; }
int is_parens(char x) { return x == '(' || x == ')'; }
Object *car(Object *x) { return x ? x->value.pair.data : 0; }
Object *cdr(Object *x) { return x ? x->value.pair.next : 0; }
static void gettoken() {
  int index = 0;
  while(is_space(look)) { look = getchar(); }
  if (is_parens(look)) {
    token[index++] = look;  look = getchar();
  } else {
    while(index < SYMBOL_MAX - 1 && look != EOF && !is_space(look) && !is_parens(look)) {
      token[index++] = look;  look = getchar();
    }
  }
  token[index] = '\0';
}

#define e_true     cons(intern("quote"), cons(intern("t"), 0))
#define e_false    0

Object * tagalloc(int type) {
  Object *obj = calloc( 1, sizeof (Object) );
  obj->tag = type;
  return obj;
}

Object * cons(Object * _car, Object * _cdr) {
  Object *_pair = tagalloc(_Pair);
  _pair->value.pair.data = _car, _pair->value.pair.next = _cdr;
  return _pair;
}

Object * newsymbol(const char *str) {
  Object *obj = tagalloc(_Symbol);
  obj->value.string = strdup(str);
  return obj;
}

Object * newprimop( Object * (*fn) (Object *) ) {
  Object *obj = tagalloc(_Primitive);
  obj->value.pfn = fn;
  return obj;
}

Object * newclosure( Object *params, Object *body, Object *env ) {
  Object *obj = tagalloc(_Closure);
  obj->value.closure.params = params, obj->value.closure.body = body, obj->value.closure.env = env;
  return obj;
}

Object * newsyntax( Object * (*fn) (Object *, Object *) ) {
  Object *obj = tagalloc(_Syntax);
  obj->value.psyn = fn;
  return obj;
}

Object * newmacro( Object *params, Object *body, Object *env ) {
  Object *obj = tagalloc(_Macro);
  obj->value.closure.params = params, obj->value.closure.body = body, obj->value.closure.env = env;
  return obj;
}

Object * intern(const char *sym) {
  Object *_pair = symbols;
  for ( ; _pair ; _pair = cdr(_pair))
    if (strncmp(sym, car(_pair)->value.string, SYMBOL_MAX)==0)
      return car(_pair);
  symbols = cons(newsymbol(sym), symbols);
  return car(symbols);
}

Object * getlist();

Object * getobj() {
  if (token[0] == '(') return getlist();
  return intern(token);
}

Object * getlist() {
  Object *tmp;
  gettoken();
  if (token[0] == ')') return (Object *) NULL;
  tmp = getobj();
  return cons(tmp, getlist());
}

void print_obj(Object *ob, int head_of_list) {
  if (ob == 0) {
    printf("null");
  } else if (ob->tag == _Symbol) {
    printf("%s", ob->value.string);
  } else if (ob->tag == _Pair) {
    if (head_of_list) printf("(");
    print_obj(car(ob), 1);
    if (cdr(ob) == 0) {
      printf(")");
    } else if(cdr(ob)->tag == _Pair) {
      printf(" ");
      print_obj(cdr(ob), 0);
    } else {
      printf(" . ");
      print_obj(cdr(ob), 0);
      printf(")");
    }
  } else if (ob->tag == _Closure) {
    printf("<CLOSURE>");
  }
}

Object * map(Object *list, Object * (*fn) (Object *, Object *), Object *context) {
  Object *head = 0, **args = &head;
  for ( ; list ; list = cdr(list) ) {
    *args = cons( fn(car(list), context), 0);
    args = &(*args)->value.pair.next;
  }
  return head;
}

Object * eval(Object *exp, Object *env);
Object * apply(Object *fun, Object *exp, Object *env);
Object * _apply(Object *fun, Object *args);

Object *fcons(Object *a)    {  return cons(car(a), car(cdr(a)));  }
Object *fcar(Object *a)     {  return car(car(a));  }
Object *fcdr(Object *a)     {  return cdr(car(a));  }
Object *feq(Object *a)      {  return car(a) == car(cdr(a)) ? (Object *) e_true : e_false;  }
Object *fpair(Object *a)    {  return car(a)->tag == _Pair  ? e_true : e_false;  }
Object *fatom(Object *a)    {  return car(a)->tag == _Symbol  ? e_true : e_false;  }
Object *fnull(Object *a)    {  return car(a) == 0           ? e_true : e_false; }
Object *freadobj(Object *a) {  look = getchar(); gettoken(); return getobj();  }
Object *fwriteobj(Object *a){  print_obj(car(a), 1); puts(""); return e_true;  }

Object *fapply(Object *exp, Object *env) {
  Object *head = 0, **args = &head, *tmp = cdr(cdr(exp));
  for (  ; cdr(tmp); tmp = cdr(tmp) ) {
    *args = cons( eval(car(tmp), env), 0);
    args = &(*args)->value.pair.next;
  }
  *args = eval(car(tmp), env); /* last argument to apply must be a list */
  return _apply(eval(car(cdr(exp)), env), head);
}

Object *fquote(Object *exp, Object *env){  return car(cdr(exp)); }
Object *flambda(Object *exp, Object *env){ return newclosure(car(cdr(exp)), car(cdr(cdr(exp))), env); }
Object *fcond(Object *exp, Object *env) {
  for (exp = cdr(exp) ; exp ; exp = cdr(exp) ) {
    if (eval(car(car(exp)), env)) /* anything not #f */
      return eval(car(cdr(car(exp))), env);
  }
  return 0;
}

Object *fmacro(Object *exp, Object *env) {
  Object *macro = eval(car(cdr(exp)), env);
  return newmacro(macro->value.closure.params, macro->value.closure.body, macro->value.closure.env);
}

Object *flet(Object *exp, Object *env) {
  Object *names_head = 0, **names = &names_head, *values_head = 0, **values = &values_head, *body = car(cdr(cdr(exp)));
  for ( exp = car(cdr(exp)); exp ; exp = cdr(exp) ) {
    *names = cons(car(car(exp)), 0);
    names = &( *names )->value.pair.next;
    *values = cons(car(cdr(car(exp))), 0);
    values = &( *values )->value.pair.next;
  }
  return eval(cons(cons(intern("lambda"), cons(names_head, cons(body, 0))), values_head), env);
}

Object * bind_append(Object *names, Object *values, Object *tail) {
  Object *head = tail, **args = &head;
  for ( ; values ; values = cdr(values), names = cdr(names) ) {
    if (car(names) == intern(".")) { /* variadic lambda syntax */
      names = cdr(names);
      *args = cons( cons(car(names), cons(values, 0)), tail);
      break;
    }
    *args = cons( cons(car(names), cons(car(values), 0)) , tail);
    args = &( *args )->value.pair.next;
  }
  return head;
}

Object * _apply(Object *fun, Object *args) {
  if (fun->tag == _Primitive) {
    return fun->value.pfn(args);
  } else if (fun->tag == _Closure) {
    Object *env = bind_append(fun->value.closure.params, args, fun->value.closure.env);
    return eval( fun->value.closure.body, env );
  }
  puts("not applicable: "); print_obj(fun, 1); printf("\n");
  return 0;
}

Object * apply(Object *fun, Object *exp, Object *env) {
  Object *args = cdr(exp);
  if (fun->tag == _Macro) {
    Object *env = bind_append(fun->value.closure.params, args, fun->value.closure.env);
    return eval( fun->value.closure.body, env );
  } else if (fun->tag == _Syntax) {
    return fun->value.psyn(exp, env);
  } else {
    Object *arg_values = map(args, eval, env);
    return _apply(fun, arg_values);
  }
}

Object * eval(Object *exp, Object *env) {
  if (exp->tag == _Symbol ) {
    for ( ; env != 0; env = cdr(env) ) {
      if (exp == car(car(env)))
        return car(cdr(car(env)));
    }
    printf("unbound variable: "); print_obj(exp, 1); printf("\n");
    return 0;
  } else if (exp->tag == _Closure) {
    return eval( exp->value.closure.body, env );
  } else if (exp->tag == _Pair) {
    return apply(eval(car(exp), env), exp, env);
  }
  puts("cannot evaluate expression: "); print_obj(exp, 1); printf("\n");
  return 0;
}

int main(int argc, char *argv[]) {
  Object *env = cons (cons(intern("car"), cons(newprimop(fcar), 0)),
              cons (cons(intern("cdr"), cons(newprimop(fcdr), 0)),
              cons (cons(intern("cons"), cons(newprimop(fcons), 0)),
              cons (cons(intern("eq?"), cons(newprimop(feq), 0)),
              cons (cons(intern("pair?"), cons(newprimop(fpair), 0)),
              cons (cons(intern("symbol?"), cons(newprimop(fatom), 0)),
              cons (cons(intern("null?"), cons(newprimop(fnull), 0)),
              cons (cons(intern("read"), cons(newprimop(freadobj), 0)),
              cons (cons(intern("write"), cons(newprimop(fwriteobj), 0)),
              cons (cons(intern("null"), cons(0,0)),
              cons (cons(intern("apply"), cons(newsyntax(fapply), 0)),
              cons (cons(intern("quote"), cons(newsyntax(fquote), 0)),
              cons (cons(intern("lambda"), cons(newsyntax(flambda), 0)),
              cons (cons(intern("cond"), cons(newsyntax(fcond), 0)),
              cons (cons(intern("let"), cons(newsyntax(flet), 0)),
              cons (cons(intern("macro"), cons(newsyntax(fmacro), 0)),
               0))))))))))))))));
  look = getchar();
  gettoken();
  print_obj( eval(getobj(), env), 1 );
  printf("\n");
  return 0;
}
