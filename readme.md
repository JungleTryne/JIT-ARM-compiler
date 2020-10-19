# Simple Just-In-Time compiler

For a given *correct* expression this code generates ARM 
instructions to calculate it. The interface of the
JIT compiler looks like this:

```C
typedef struct {
    const char * name;
    void       * pointer;
} symbol_t;

extern void
jit_compile_expression_to_arm(const char * expression,
                              const symbol_t * externs,
                              void * out_buffer);
``` 

Where:
 - ```const char* expression``` - standard C string with the expression
to be calculated
 - ```const symbol_t* externs``` - array of the structures which
 describe the external symbols (variables and functions).
 Must end with ```{.name=0, .pointer=0}```
 - ```void* out_buffer``` - allocated memory pointer which
 will be filled with ARM instructions after the job
 
 Expressions can contain
 - Integer constants
 - Names of global integer variables
 - Names of functions which return int and receive up to
 4 arguments
 - Subexpressions with parenthesis.
 
 The given expression must be valid from mathematical perspective.
 