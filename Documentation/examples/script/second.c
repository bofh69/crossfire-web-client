#include <stdio.h>

int main (){
	char buf[200];
	int len;
	fprintf (stdout,"monitor\n");
	fflush (stdout);
	for(;;){
		len=read (0,buf,150);
		if (len)
			write (2,buf,len);
		else
			exit(-1);
	}
}
