#ifndef _PROTOC_
#define _PROTOC_

#define OVECCOUNT 30 /* should be a multiple of 3 */
#define CH_BLOCK 5

/* #include <gc.h>  garbage collector */

typedef enum { Integer, Pointer, Double } Types;
typedef enum { Int, Ptr, Str, Char, Db } Return;

typedef struct {
	Return ret;
	Types* types;
	int type_count;
} Prototype;

typedef struct {
	char *keyword; 
	char **args;
	int  argc;
} ParseCtx;

typedef struct {
	char* name;
	void* pointer;
	Prototype proto; /* => related to an xml file for found prototypes */
} Symbol;

typedef struct {
	FILE   **dll;
	Symbol *syms;
	int dll_count;
	int sym_count;
} DllGlobal;

void load_prototypes(const char*);
int dlink(ParseCtx*);
int load(ParseCtx*);

char* strndup(char*,int);

#endif
