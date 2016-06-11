#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdarg.h>



void * log_create(char *logpath);
void log_delete(void* loger);
void log_write(void *loger, char * fmt, ...);
