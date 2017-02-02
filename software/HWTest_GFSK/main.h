#include <wiringPi.h>
#include <math.h>
#include "iostream"
#include <time.h>
#include "stdlib.h"
#include "BCHEncoder.h"
#include "adf7012.h"
#include "7BitASCII.h"
#include <bitset>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

// PIN Definitions:
    
#define LE 0
#define CE 7
#define CLK 3
#define SDATA 2
//#define TXDATA 11	//Revision 1
#define MUXOUT 13
#define TXCLK 14
#define CLKOUT 12
//#define ATCLK 16	//Revison 1
//#define ATDATA 15	//Revison 1
//#define HANDSHAKE 5	//Revison 1
//#define PTT 4		//Revison 1

//Revision 2:
#define ATCLK 11
#define ATDATA 10
#define HANDSHAKE 5
#define PTT 4


// Radio Parameters
//#define CRYSTALFREQ 4915200UL
//#define FREQUENCY 439987500UL     
    
    
// #define SCKpin  13   // SCK
//#define SSpin  10    // SS
// #define MOSIpin 11   // MOSI

//#define ADF7012_CRYSTAL_FREQ VCXO_FREQ
#define ADF7012_CRYSTAL_DIVIDER 1 
    
// POCSAG Protokoll
#define POCSAG_PREAMBLE_CODEWORD 0xAAAAAAAA
#define POCSAG_IDLE_CODEWORD 0x7A89C197
#define POCSAG_SYNCH_CODEWORD 0x7CD215D8
