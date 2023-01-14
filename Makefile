CC 		= gcc

OUT	 	= prn_cnv

SOURCES 	=  emul.c gifenc.c
OBJS 		= ${SOURCES:.c=.o} 

CFLAGS 		= -Wall
LDFLAGS     =

all		: ${OUT}

clean   :
	rm *.o
	rm ${OUT}

${OUT}	: ${OBJS}
		${CC} -o ${OUT} ${OBJS} ${CFLAGS} ${LDFLAGS}
