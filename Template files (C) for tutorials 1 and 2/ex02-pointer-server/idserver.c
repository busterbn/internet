/*
 * idserver.c
 *
 *  Created on: Feb 15, 2016
 *      Author: jiaziyi
 *  Updated: JACF, 2020
 */

#include<stdlib.h>
#include<stdio.h>
#include<string.h>

#include "idserver.h"

/**
 * print the server's information
 */
void print_idserver(idserver s)
{
}

/**
 * try to modify the server information
 */
void modify(idserver s, char *id, int latency, char status[])
{
}

/**
 * try to modify the server information using pointer
 */
void modify_by_pointer(idserver *s, char *id, int latency, char status[])
{
}

idserver* create_idserver(char *id, char *region, int latency,
		char *status, int *nthreads)
{
	idserver s;
	s.id = id;
	s.region = region;
	s.latency = latency;
	strncpy(s.status, status, strlen(status)+1);
	s.nthreads = nthreads;
	puts("---print inside create_idserver function---");
	print_idserver(s);
	puts("---end of print inside");
	return &s;
}
