#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
int main(int argc, const char ** argv)
{
	openlog(NULL,0,LOG_USER);
	if(argc != 3)
	{

		syslog(LOG_ERR,"Invalid Argument");
		return 1;
	}

	const char * filename = argv[1];
	const char * text = argv[2];
	FILE * fp;
	fp = fopen(filename,"a");
	if(fp == NULL)
	{
		syslog(LOG_ERR,"Error opening file %s %s",filename,strerror(errno));
		return 1;
	}
	fprintf(fp,"%s\n",text);
	syslog(LOG_DEBUG,"Writing %s to %s", text,filename);
	fclose(fp);

	return 0;
}
