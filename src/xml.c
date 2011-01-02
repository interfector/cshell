#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/parser.h>
#include <cshell.h>

extern DllGlobal lib;
extern int show_proto;

void
load_prototypes(const char * file)
{
	LIBXML_TEST_VERSION

	int i = 0, j;
	char* token, *string, *tmp;
	xmlDocPtr doc = NULL;
	xmlNodePtr node = NULL;

	if ((doc = xmlReadFile (file, NULL, 0)) == NULL)
	{
		fprintf (stderr, "Error: couldn't parse file %s\n", file);
		exit(1);
	}

	node = xmlDocGetRootElement (doc);

	if (xmlStrcmp (node->name, (xmlChar *) "functions"))
	{
		fprintf (stderr, "Error when loading configuration\n");
		exit(1);
	}

	node = ((xmlNodePtr)node->children);

	while (node)
	{
		if (node->type == XML_ELEMENT_NODE && node->children->type == XML_TEXT_NODE)
		{
			char** array = malloc( sizeof(char*) );
			ParseCtx ctx = { NULL, array, 1 };

			if(!strcmp((char*)xmlStrdup(node->name),"library"))
			{
				token = strtok( (char*)xmlStrdup(node->children->content), ":" );

				if( ctx.keyword )
					free( ctx.keyword );

				ctx.keyword = strdup("load");;

				if(!token)
				{
					ctx.args[0] = (char*)xmlStrdup(node->children->content);

					load( &ctx );
				} else {
				
					ctx.args[0] = token;

					load( &ctx );

					while(( token = strtok( NULL, ":" )))
					{
						ctx.args[0] = token;

						load( &ctx );
					}
				}

				if( show_proto )
					printf("===== PROTOTYPE STARTS HERE =====\n");
			} else {
				ctx.args[0] = xmlStrdup( node->name );

				if( ctx.keyword )
					free( ctx.keyword );

				ctx.keyword = strdup("link");;

				dlink( &ctx );

				for(i = 0;i < lib.sym_count;i++)
				{
					if(!xmlStrcmp(node->name, (xmlChar *)lib.syms[i].name ))
					{
						token = strtok( (char*)xmlStrdup(node->children->content), ":" );

						if(!strcmp(token,"Int"))
							lib.syms[i].proto.ret = Int;
						else if(!strcmp(token,"Ptr"))
							lib.syms[i].proto.ret = Ptr;
						else if(!strcmp(token,"Str"))
							lib.syms[i].proto.ret = Str;
						else if(!strcmp(token,"Char"))
							lib.syms[i].proto.ret = Char;
						else if(!strcmp(token,"Dbl"))
							lib.syms[i].proto.ret = Db;

						tmp = strtok( NULL, ":" );

						token = strtok( tmp, "," );

						if(!token)
						{
							if( show_proto )
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
							
								printf("%s( );\n", xmlStrdup(node->name) );
							}

							break;
						}

						if( !lib.syms[i].proto.types )
							lib.syms[i].proto.types = malloc( CH_BLOCK * sizeof(Types) );

						if(!strcmp(token,"Integer"))
							lib.syms[i].proto.types[lib.syms[i].proto.type_count++] = Integer;
						else if(!strcmp(token,"Pointer"))
							lib.syms[i].proto.types[lib.syms[i].proto.type_count++] = Pointer;
						else if(!strcmp(token,"Double"))
							lib.syms[i].proto.types[lib.syms[i].proto.type_count++] = Double;

						while((token = strtok(NULL, "," )))
						{
							if(lib.syms[i].proto.type_count % CH_BLOCK)
								lib.syms[i].proto.types = realloc( lib.syms[i].proto.types, ( CH_BLOCK + lib.syms[i].proto.type_count ) * sizeof(Types) );

							if(!strcmp(token,"Integer"))
								lib.syms[i].proto.types[lib.syms[i].proto.type_count++] = Integer;
							else if(!strcmp(token,"Pointer"))
								lib.syms[i].proto.types[lib.syms[i].proto.type_count++] = Pointer;
							else if(!strcmp(token,"Double"))
								lib.syms[i].proto.types[lib.syms[i].proto.type_count++] = Double;
						}

						if( show_proto )
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

							printf("%s(", xmlStrdup(node->name) );
	
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

						break;
					}
				}
			}
		}

		node = node->next;
	}

	if( show_proto )
		printf("===== PROTOTYPE ENDS HERE =====\n\n");
}
