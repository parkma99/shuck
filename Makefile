# COMP1521 21T2 ... a general makefile for multiple exercises

CC		 = gcc
EXERCISES	?=
CLEAN_FILES	?=

.DEFAULT_GOAL	= all
.PHONY:		all

-include *.mk

all:	${EXERCISES}

clean:
	-rm -f ${CLEAN_FILES}
