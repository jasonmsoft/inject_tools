#include "stdafx.h"
#include "log.h"


typedef struct log_handle{
	FILE *loger;
	char log_path[512];
}LOG_HANDLE_T;



void * log_create(char *logpath)
{
	LOG_HANDLE_T *handle = new LOG_HANDLE_T;
	handle->loger = fopen(logpath, "wb+");
	strcpy(handle->log_path, logpath);
	return handle;
}


void log_delete(void* loger)
{
	LOG_HANDLE_T *handle = (LOG_HANDLE_T *)loger;
	fclose(handle->loger);
	delete handle;
	return;
}


void log_write(void *loger, char * fmt, ...)
{
	LOG_HANDLE_T *handle = (LOG_HANDLE_T *)loger;
	char buffer[1024] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	fwrite(buffer, strlen(buffer), 1, handle->loger);
	fflush(handle->loger);
}