#ifndef _7BIT_ASCII_H_
#define _7BIT_ASCII_H_

typedef struct tagASCII7Bit
{
	char Letter;
	char Bits[8];
} ASCII7Bit;

extern ASCII7Bit ASCII7BitTable[128];

#endif //_7BIT_ASCII_H_
