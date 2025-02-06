#include <stdint.h>
#include <stdio.h>
#include <math.h>

struct{
	float temp;
	uint16_t src;
	} tests[]={
	{125,0x07D0},
	{85,0x0550},
	{25.0625,0x0191},
	{10.125,0x00A2},
	{0.5,0x0008},
	{0,0x0000},
	{-0.5,0xFFF8},
	{-10.125,0xFF5E},
	{-25.0625,0xFE6F},
	{-55,0xFC90}
};

int main(){
	int q;
	int16_t src;
	float temp,diff;
	for(q=0;q<10;q++){
		src=tests[q].src;
		temp=(double)src/16.0;
		diff=fabs(tests[q].temp-temp);
		
		printf("test %04x->%.2f, our: %x->%.2f, diff: %f\n",tests[q].src,tests[q].temp,src,temp,diff);
		
	}
	
	return 0;
}
