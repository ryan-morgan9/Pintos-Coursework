#include <stdio.h>
#include <syscall.h>
#include "tests/userprog/sample.inc"
#include <string.h>
int
main (void)
{

//read bad fd
//char buf;
//read(0x20101234, &buf, 1);

//open empty file
//int handle = open("");

//open missing
//int handle = open("no-such-file");

//execute missing
//exec("no-such-file");

//closing twice
//create("sample.txt", 1);
//int handle = open("sample.txt");
//close(handle);
//close(handle);

//read bad file pointer
//int handle = open("sample.txt");
//read(handle, (char*)0xc0100000, 123); 

//create null pointer
//create(NULL,0);

//create too long name
//static char name[512];
//memset(name, 'x', sizeof name);
//name[sizeof name -1] = '\0';
//create(name, 0);

//create empty
//create("", 0);

//bad write pointer
//int handle, byte_cnt;
 // handle = open ("sample.txt");
 // byte_cnt =  write(handle, (char *) 0x10123420,  123);
 // close(handle);

}
