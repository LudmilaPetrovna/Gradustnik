#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "crc32.c"
#include "fancylog2.h"

char *cp866[256]={".","☺","☻","♥","♦","♣","♠","•","◘","○","◙","♂","♀","♪","♫","☼","►","◄","↕","‼","¶","§","▬","↨","↑","↓","→","←","∟","↔","▲","▼"," ","!","\"","#","$","%","&","'","(",")","*","+",",","-",".","/",
"0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?","@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","[","\\","]","^","_","`","a","b","c","d",
"e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","{","|","}","~","⌂","А","Б","В","Г","Д","Е","Ж","З","И","Й","К","Л","М","Н","О","П","Р","С","Т","У","Ф","Х","Ц","Ч","Ш","Щ",
"Ъ","Ы","Ь","Э","Ю","Я","а","б","в","г","д","е","ж","з","и","й","к","л","м","н","о","п","░","▒","▓","│","┤","╡","╢","╖","╕","╣","║","╗","╝","╜","╛","┐","└","┴","┬","├","─","┼","╞","╟","╚","╔","╩","╦","╠","═","╬",
"╧","╨","╤","╥","╙","╘","╒","╓","╫","╪","┘","┌","█","▄","▌","▐","▀","р","с","т","у","ф","х","ц","ч","ш","щ","ъ","ы","ь","э","ю","я","Ё","ё","Є","є","Ї","ї","Ў","ў","°","∙","·","√","№","¤","■"," "};

void fancylog_print_source(const char *file, int line, const char *func){
	int color_file=crc32_le(0,file,strlen(file))&0xFF;
	int color_func=crc32_le(0,func,strlen(func))&0xFF;
	fprintf(stderr,"\x1b[38;5;%d;48;5;%dm%12s:%04d:\x1b[38;5;%d;48;5;%dm%s\x1b[0m: ",color_file^0x80,color_file,file,line,color_func^0x80,color_func,func);
}

void fancylog_print_newline(){
	fprintf(stderr,"\x1b[0m\r\n");
}

void trace_impl_pre(const char *file, int line, const char *func){
	fancylog_print_source(file,line,func);
}
void trace_impl_post(const char *file, int line, const char *func){
	fancylog_print_newline();
}
void crash_impl_pre(const char *file, int line, const char *func){
	fancylog_print_source(file,line,func);
}
void crash_impl_post(const char *file, int line, const char *func){
	fancylog_print_newline();
	abort();
}

void logbuf_impl(void *buf, int size, const char *file, int line, const char *func){
	fancylog_print_source(file,line,func);
	int crc=crc32_le(0,buf,size);
	
	fprintf(stderr,"Buffer: %p, size: %d, crc: %08x\n",buf,size,crc);
	int q;
	int linesize;
	int offset=0;
	uint8_t *bytes=buf;
	int lin=0;
	int row=0;
	int padding;
	while(offset<size){
		row=0;
		linesize=size-offset;
		if(linesize>16){
		linesize=16;}
		fprintf(stderr,"\x1b[1;44;33m%08X: ",offset);
		for(q=0;q<linesize;q++){
			if(q%4==0){
				if(((lin^row)&1)==0){
					fprintf(stderr,"\x1b[1;32m");
				} else {
					fprintf(stderr,"\x1b[1;36m");
				}
				row++;
			}
			
			fprintf(stderr,"%02x",bytes[offset+q]);
			
			if(q%4==3){
				fprintf(stderr,"\x1b[0;44;30m");
				if(q%8==7){
					fprintf(stderr,"║");
				} else {
					fprintf(stderr,"│");
				}
				} else{
				fprintf(stderr," ");
			}
			
		}
		
		//draw padding
		if(linesize!=16){
			padding=16-linesize;
			for(q=0;q<padding;q++){
				fprintf(stderr,"   ");
			}
		}
		
		for(q=0;q<linesize;q++){
			if(q%4==0){
				if(((lin^row)&1)==0){
					fprintf(stderr,"\x1b[0;44;32m");
				} else {
					fprintf(stderr,"\x1b[0;44;36m");
				}
				row++;
			}
			
			fprintf(stderr,"%s",cp866[bytes[offset+q]]);
			
		}
		
		//draw padding
		if(linesize!=16){
			padding=16-linesize;
			for(q=0;q<padding;q++){
				fprintf(stderr," ");
			}
		}
		
		fprintf(stderr,"\x1b[0m\n");
		offset+=linesize;
		lin++;
		
	}
	
}

/*
int main(int argc, char **argv){
	char buf[1024];
	FILE *f=fopen(argv[1],"rb");
	int size=fread(buf,1,777,f);
	
	logbuf(buf,size);
	
	return EXIT_SUCCESS;
}
*/
