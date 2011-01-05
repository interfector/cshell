XML2LIB = -lxml2
XML2INC = /usr/include/libxml2

INCS = -I/usr/include -I${XML2INC} -Iinclude
LIBS = ${XML2LIB} -ldl -lpcre -lreadline

CFLAGS = ${INCS}
LDFLAGS = -s ${LIBS}

all:
	gcc -o cshell src/main.c src/xml.c ${INCS} ${LIBS}
clean:
	rm cshell
