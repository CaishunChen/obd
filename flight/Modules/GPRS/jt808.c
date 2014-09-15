#include "jt808.h"



#ifdef TEST
#include <stdio.h>

void hexdump(unsigned char *p, unsigned int len)
{
	unsigned char *line = p;
	int i, thisline, offset = 0;
	while (offset < len)
	{
		printf("%04x ", offset);
		thisline = len - offset;
		if (thisline > 16)
			thisline = 16;
		for (i = 0; i < thisline; i++)
			printf("%02x ", line[i]);
		for (; i < 16; i++)
			printf("   ");
		for (i = 0; i < thisline; i++)
			printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
		printf("n");
		offset += thisline;
		line += thisline;
	}

	printf("\n");
}

int main()
{
	msgHdr hdr;

	memset(&hdr,0,sizeof(hdr));

	hdr.id = 0x0001;
	hdr.prop.len = 1023;
	hdr.seq = 1;

	hexdump(&hdr,sizeof(hdr));
}
#endif

