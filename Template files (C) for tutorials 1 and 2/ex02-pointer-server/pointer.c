/*
 * pointer.c
 *
 *  Created on: Feb 15, 2016
 *      Author: jiaziyi
 *  Updated: JACF, 2020
  *	Updated: kjiokeng, Jun 2023
 */

#include<stdlib.h>
#include<stdio.h>
#include<string.h>

#include "idserver.h"

int main(int argc, char* argv[]){

	// DO NOT MODIFY THESE LINES
	idserver cmok;
	idserver *s1;

	cmok.latency = 600;
	cmok.region = "eur";
	cmok.id = "cmok";
	strncpy(cmok.status, "up", strlen("up") + 1);
	int p = 70;
	cmok.nthreads = &p;
	s1 = &cmok;
	// END OF DO NOT MODIFY


	//STEP 1: pointer observation
	puts("=========STEP 1===========");
	printf("s1: %p\n", s1);
	printf("cmok's name: %s\n", cmok.id);
	printf("cmok's latency: %d\n", cmok.latency);
	printf("cmok's nthreads - first try: %p\n", cmok.nthreads);
	printf("cmok's nthreads - second try: %d\n", *cmok.nthreads);
	printf("cmok's nthreads through pointer - first try: %p\n", s1->nthreads);
	printf("cmok's nthreads through pointer - second try: %d\n", *s1->nthreads);
	puts("=========================");
	puts("");
//
//	//step 2: print idserver
//	puts("=========STEP 2===========");
//	puts("--results of print_idserver--");
//	print_idserver(cmok);
//	puts("=========================");
//	puts("");
//
//
//	//step 3: modification
//	puts("=========STEP 3===========");
//	modify(cmok, "cmok", 13000, "unknown");
//
//	puts("--results of modify--");
//	print_idserver(cmok);
//
//	modify_by_pointer(&cmok,"cmok", 13000, "unknown");
//	puts("--results of modify_by_pointer--");
//	print_idserver(cmok);
//	puts("=========================");
//	puts("");
//
//	//step 4: pointers
//	puts("=========STEP 4===========");
//	idserver albi;
//	idserver *s2;
//
//	albi = cmok;
//	s2 = &albi;
//
//	modify_by_pointer(s2, "albi", 9000, "down");
//	puts("--*s2--");
//	print_idserver(*s2);
//	puts("--albi--");
//	print_idserver(albi);
//	puts("--cmok--");
//	print_idserver(cmok);
//	puts("=========================");
//	puts("");
//
//
//	//step 5: create idservers
//	puts("=========STEP 5===========");
//	int nthreads = 20;
//
//	idserver *s3 = create_idserver("thorn", "afr", 5200, "up", &nthreads);
//	puts("--results of creating ted, printed outside--");
//	print_idserver(*s3);
//	puts("=========================");
//	puts("");


	return EXIT_SUCCESS;
}


