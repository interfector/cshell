#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <pcre.h>

#include <cshell.h>

DllGlobal lib = { 0, 0, 0, 0 };

char* specialPointer;
int show_proto = 0;

char** commands = 0;
int    command_index = 0;

void
banner(void)
{
	printf(
		"       _        _ _ \n"
		" __ __| |_  ___| | |\n"
		"/ _(_-< ' \\/ -_) | |\n"
		"\\__/__/_||_\\___|_|_|\n"
		"                    \n");
}

char*
generator(const char* text, int state)
{
	static int list_index = 0, len;
	char *name;

	if (!state)
	{
	    	list_index = 0;
	    	len = strlen (text);
	}

	while ((name = commands[list_index]))
	{
	    	list_index++;
	    	if (strncmp (name, text, len) == 0)
	         	return strdup(name);
	}

	return NULL;
}

char**
pcre_match( char* string, const  char* pattern, int* size )
{
	pcre *re;
	const char *error;
	int erroroff;
	int rc, i;

	char** data;

	int vector[OVECCOUNT] = { 0 };

	re = pcre_compile( pattern, 0, &error, &erroroff, NULL );

	if(!re)
		return NULL;

	rc = pcre_exec( re, NULL, string, strlen(string), 0, 0, vector, OVECCOUNT );

	if( rc < 0 )
		return NULL;

	*size = rc;

	data = malloc( sizeof(char*) * rc);

	for(i = 0;i < rc;i++)
	{
		data[i] = strdup( string ) + vector[i + 2];
		data[i][vector[i + 3]-vector[i + 2]] = 0;
	}

	return data;
}

int 
load(ParseCtx* ctx)
{
	if( !lib.dll )
		lib.dll = malloc( CH_BLOCK * sizeof(FILE*) );

	if( lib.dll_count % CH_BLOCK )
		lib.dll = realloc( lib.dll, ( CH_BLOCK + lib.dll_count ) * sizeof(FILE*) );

	if(!(lib.dll[lib.dll_count] = dlopen(ctx->args[0], RTLD_GLOBAL | RTLD_NOW)))
	{
		printf("Library \'%s\' not loaded!\n", ctx->args[0]);
		return 1;
	}

	lib.dll_count++;

	return 0;
}

int 
dlink(ParseCtx *ctx)
{
	void* null = NULL;
	int i;

	void* ptr;

	for(i = 0;i < lib.dll_count;i++)
	{
		ptr = dlsym(lib.dll[i], ctx->args[0] );

		if(!dlerror())
		{
			if( !lib.syms )
				lib.syms = malloc( CH_BLOCK * sizeof(Symbol) );

			if( lib.sym_count % CH_BLOCK )
				lib.syms = realloc( lib.syms, ( CH_BLOCK + lib.sym_count ) * sizeof(Symbol) );

			lib.syms[lib.sym_count].name = strdup(ctx->args[0]);
			lib.syms[lib.sym_count].proto.types = 0;
			lib.syms[lib.sym_count].proto.type_count = 0;
			lib.syms[lib.sym_count++].pointer = ptr;

			return 0;
		}
	}

	ptr = dlsym(NULL, ctx->args[0] );

	if(!dlerror())
	{
		if( !lib.syms )
			lib.syms = malloc( CH_BLOCK * sizeof(Symbol) );

		if( lib.sym_count % CH_BLOCK )
			lib.syms = realloc( lib.syms, ( CH_BLOCK + lib.sym_count ) * sizeof(Symbol) );

		lib.syms[lib.sym_count].name = strdup(ctx->args[0]);
		lib.syms[lib.sym_count].proto.ret = 0;
		lib.syms[lib.sym_count].proto.types = 0;
		lib.syms[lib.sym_count].proto.type_count = 0;
		lib.syms[lib.sym_count++].pointer = ptr;

		return 0;
	}

	printf("Symbol \"%s\" not loaded.\n",ctx->args[0]);

	return 1;
}


ParseCtx*
pcre_parse_string( char* string )
{
	const char * pattern = "([^\\(]+)\\((.+)\\)";
	int size = 0;

	ParseCtx *ctx = malloc( sizeof(ParseCtx) );

	char** matched = NULL;

	char* token;

	if(!string)
		return NULL;
		
	matched = pcre_match(string, pattern, &size );

	if( !matched )
	{
		matched = pcre_match(string, "([^\\(]+)\\((.?)\\)", &size );

		if( !matched )
		{
			printf("* Syntax error.\n", string);

			free( ctx );
			return NULL;
		}
	}

	ctx->keyword = strdup( matched[0] );

	ctx->argc = 0;

	ctx->args = malloc( CH_BLOCK * sizeof(char*) );

	token = strtok( matched[2], "," );

	if( !token )
	{
		free( ctx->args );

		ctx->args = NULL;

		return ctx;
	}

	ctx->args[ctx->argc++] = strdup( token );

	while(( token = strtok( NULL, "," )))
	{
		if( ctx->argc % CH_BLOCK )
			ctx->args = realloc( ctx->args, (CH_BLOCK + ctx->argc) * sizeof(char*));

		ctx->args[ctx->argc++] = strdup( token );
	}

	return ctx;
}

void
special_chars( char* arg )
{
	char* dup = strdup( arg );
	int i, j;

	for(i = 0,j = 0;i < strlen(dup);i++)
	{
		if(dup[i] == '\\' && i + 1 != strlen(dup) )
		{
			switch(dup[i + 1])
			{
				case 't':
					arg[j++] = '\t';
					i++;
					break;
				case 'n':
					arg[j++] = '\n';
					i++;
					break;
				case 'r':
					arg[j++] = '\r';
					i++;
					break;
				case 'x':
					arg[j++] = strtol(strndup(dup + i + 2,2),NULL,16);
					i += 3;
					break;
				default:
					arg[j++] = '\\';
					arg[j++] = dup[i + 1];
					break;
			}
		} else
			arg[j++] = dup[i];
	}

	arg[j] = 0;
}

int*
intDup( int number )
{
	int* array = malloc( sizeof(int) );

	*array = number;

	return array;
}

int
dlcall( void* ptr, char** argv, int argc, Prototype proto )
{
	int i, j, tmp;
	int ret;

	if( argc < proto.type_count )
	{
		printf("* Too few arguments.\n");
		return 1;
	}

	for(i = 0;i < /*proto.type_count*/argc;i++)
	{
		if(!strcmp(argv[i],"##"))
		{
			argv[i] = specialPointer;
			continue;
		} else if(!strcmp(argv[i],"stdin")) {
			argv[i] = (char*)stdin;
			continue;
		} else if(!strcmp(argv[i],"stdout")) {
			argv[i] = (char*)stdout;
			continue;
		} else if(!strcmp(argv[i],"NULL")) {
			argv[i] = (char*)NULL;
			continue;
		}
			
		if(!strncmp(argv[i],"0x",2))
		{
			tmp = strtol( argv[i] + 2, NULL, 16 );
			argv[i] = (char*)tmp;
			continue;
		} else if(!strncmp(argv[i],"*[int*]0x",9)) {
			tmp = strtol( argv[i] + 9, NULL, 16 );
			argv[i] = *(int*)tmp;
			continue;
		} else if(!strncmp(argv[i],"*[char*]0x",10)) {
			tmp = strtol( argv[i] + 10, NULL, 16 );
			argv[i] = *((char*)tmp);
			continue;
		} else if(!strncmp(argv[i],"*[int*]##",9)) {
			argv[i] = *(int*)specialPointer;
			continue;
		} else if(!strncmp(argv[i],"*[char*]##",10)) {
			argv[i] = *(char*)specialPointer;
			continue;
		}

		if(i < proto.type_count)
		{
			if(proto.types[i] == Integer)
			{
				tmp = atoi(argv[i]);

				argv[i] = (char*)tmp;
			} else if(proto.types[i] == Double)
			{
				tmp = atof(argv[i]);

				argv[i] = (char*)tmp;
			} else if(proto.types[i] == Pointer)
			{
				special_chars( argv[i] );
			}
		}
	}

	for(i = 0,j = 0;i < argc;i++, j += 4)
		asm("movl %0, (%%esp,%1)" : :"r"(argv[i]), "r"(j) );

	asm("call *%0" : :"r"(ptr));
	asm("movl %%eax, %0" :"=r"(ret));

	return ret;
}

int
main(int argc,char **argv)
{
	ParseCtx* ctx;
	int i, j, ret = 0xbad;
	char* buf;

	banner();

	rl_completion_entry_function = generator;

	if(argv[1] && !strcmp(argv[1],"--prototype"))
		show_proto = 1;

	commands = malloc( sizeof(char*) * 4 );
	commands[command_index++] = strdup("quit");
	commands[command_index++] = strdup("help");
	commands[command_index++] = strdup("clear_ptr");
	commands[command_index++] = strdup("prototype");

	load_prototypes("./dynamic.xml");

	specialPointer = malloc( BUFSIZ );
	memset( specialPointer, 0, BUFSIZ );

	while((buf = readline(">> ")))
	{
		rl_bind_key('\t',rl_complete);

		if( !buf[0] || buf[0] == '\n' || buf[0] == '#' )
			continue;

		if(strstr(buf,"//"))
			*(char*)strstr(buf,"//") = 0;

		ctx = pcre_parse_string( buf );

		if( !ctx )
			continue;

		if(!strcmp( ctx->keyword, "clear_ptr" ))
			memset( specialPointer, 0, BUFSIZ );
		else if(!strcmp( ctx->keyword, "help" ))
			printf(
				"clear_ptr()\t-\tClear the specialPointer.\n"
				"prototype([char*])\t-\tShow all the prototypes.\n"
				"help()     \t-\tShow this help.\n"
				"quit()     \t-\tQuit program.\n"
				);
		else if(!strcmp( ctx->keyword, "prototype" )) {
			if(ctx->argc == 0 )
			{
				printf("===== PROTOTYPE STARTS HERE =====\n");
				for(i = 0;i < lib.sym_count;i++)
				{
					switch( lib.syms[i].proto.ret )
					{
						case Int:
							printf("int ");
							break;
						case Char:
							printf("char ");
							break;
						case Ptr:
							printf("void* ");
							break;
						case Str:
							printf("char* ");
							break;
						case Db:
							printf("double ");
							break;
						default:
							printf("void ");
							break;
					}

					if( !lib.syms[i].proto.type_count )
						printf("%s( );\n", lib.syms[i].name );
					else {
						printf("%s(", lib.syms[i].name );
	
						for(j = 0;j < lib.syms[i].proto.type_count;j++)
						{
							if(lib.syms[i].proto.types[j] == Integer)
								printf(" int" );
							else if(lib.syms[i].proto.types[j] == Pointer)
								printf(" void*" );
							else if(lib.syms[i].proto.types[j] == Double)
								printf(" double");

							if( (j+1) != lib.syms[i].proto.type_count )
								putchar(',');
							else
								printf(" );\n");
						}
					}
				}

				printf("===== PROTOTYPE ENDS HERE =====\n");
			} else {
				for(i = 0;i < lib.sym_count;i++)
				{
					if(!strcmp(lib.syms[i].name,ctx->args[0]))
					{
						switch( lib.syms[i].proto.ret )
						{
							case Int:
								printf("int ");
								break;
							case Char:
								printf("char ");
								break;
							case Ptr:
								printf("void* ");
								break;
							case Str:
								printf("char* ");
								break;
							case Db:
								printf("double ");
								break;
							default:
								printf("void ");
								break;
						}

						if( !lib.syms[i].proto.type_count )
							printf("%s( );\n", lib.syms[i].name );
						else {
							printf("%s(", lib.syms[i].name );
	
							for(j = 0;j < lib.syms[i].proto.type_count;j++)
							{
								if(lib.syms[i].proto.types[j] == Integer)
									printf(" int" );
								else if(lib.syms[i].proto.types[j] == Pointer)
									printf(" void*" );
								else if(lib.syms[i].proto.types[j] == Double)
									printf(" double");

								if( (j+1) != lib.syms[i].proto.type_count )
									putchar(',');
								else
									printf(" );\n");
							}
						}
					}
				}
			}
		} else if(!strcmp( ctx->keyword, "quit" )) {
			free( ctx->args );
			free( ctx );

			exit( 0 );
		}
		else {
			for(i = 0;i < lib.sym_count;i++)
			{
				if(!strcmp( lib.syms[i].name, ctx->keyword))
				{
					ret = dlcall( lib.syms[i].pointer, ctx->args, ctx->argc, lib.syms[i].proto );

					switch( lib.syms[i].proto.ret )
					{
						case Int:
							printf("[return: %d]\n", ret );
							break;
						case Ptr:
							printf("[return: %p]\n", (void*)ret );
							break;
						case Str:
							printf("[return: %s]\n", (char*)ret );
							break;
						case Char:
							printf("[return: \'%c\']\n", (char)ret );
							break;
						case Db:
							printf("[return: %lf]\n", (double)ret );
							break;
						default:
							printf("[return: %p]\n", (void*)ret );
							break;
					}

					break;
				}
			}

			if( ret == 0xbad )
				printf("Unknown command.\n");
		}

		add_history( buf );

		free( ctx->args );
		free( ctx );
		free( buf );
	}

	return 0;
}

