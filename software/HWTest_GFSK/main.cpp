#include <wiringPi.h>
#include <math.h>
#include "iostream"
#include <time.h>
#include "stdlib.h"
#include "main.h"
#include "BCHEncoder.h"
#include "adf7012.h"
#include "7BitASCII.h"
#include <bitset>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <vector>
#include <sstream>
#include "4BitTable.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>

int iPOCSAGMsg[68];
CBCHEncoder m_bch;
std::string m_Message;
RadioAdf7012 radio;
int *currentTime;
char *slotEnabled[16];

struct message {
  int type;
  int id;
  int function;
  char text[300];
} *queue[1000];

int *currentQueueItem ;
int *lastQueueItem ;
int *queueCount ;

int* encodeNumericMsg(int number, int type, std::string message) 
{
	int i;
	int iReciever=number;

	//Reset the codewords
	for (i=0; i<68; i++)
		iPOCSAGMsg[i]=POCSAG_IDLE_CODEWORD;

	//Initilizing Synch Codewords
	for (i=0; i<68; i+=17)
		iPOCSAGMsg[i]=POCSAG_SYNCH_CODEWORD;

	//compute address codeword
	int iStartFrame=iReciever%8;

	/*
	for (i=0; i<iStartFrame*2; i++)
		iPOCSAGMsg[i]=POCSAG_IDLE_CODEWORD;
	*/

	int iAddress=iReciever >> 3;
	iAddress=iAddress<<2;
	iAddress|=type;				//Funktionsbits
	iAddress=iAddress<<11;

	m_bch.SetData(iAddress);
	m_bch.Encode();
	int iAddressEnc=m_bch.GetEncodedData();

	iPOCSAGMsg[iStartFrame*2+1]=iAddressEnc;

	//computer message codeword
	std::string TempBits;
	int iLen = message.size();
	char cMessage[2048];
	memset(cMessage,' ', 2048);
	message.copy(cMessage, iLen);

	for (i=0; i<iLen; i++)
	{
		char c=cMessage[i];

		for (int j=0; j<128; j++)
			if (num4BitTable[j].Letter==c)
			{
				std::string TempStr=num4BitTable[j].Bits;
				TempStr = std::string(TempStr.rbegin(), TempStr.rend()); //reverse string;
				TempBits+=TempStr;
				break;
			}
	}
	
	std::string TempStr;	

	TempStr = std::string(TempStr.rbegin(), TempStr.rend()); //reverse string
	TempBits+=TempStr;

	//now we have bits of message!
	int iMessageCodewords=(iLen+1)*4/20;

	if (iMessageCodewords*20!=(iLen+1)*4)
		iMessageCodewords++;

	int iRemainBit=iMessageCodewords*20-4*(iLen+1);
	float fRemain=ceil((float) iRemainBit/4);
	iRemainBit=(int) fRemain;

	for (i=0; i<iRemainBit; i++)
	{
		TempStr=num4BitTable[' '].Bits;
		TempStr = std::string(TempStr.rbegin(), TempStr.rend()); //reverse string
		TempBits+=TempStr;
	}

	iRemainBit=TempBits.length()%20;

	//must remove iRemainBits from right of string
	TempBits=TempBits.substr(0, TempBits.length()-iRemainBit);

	std::bitset<2100> MsgBits(TempBits);

	int k=TempBits.length()-1;

	int iCodeword=1;

	for (i=k; i>=0; i-=20)
	{
		int iMsgCode=1<<31;		//0x80000000

		for (int j=0; j<20; j++)
		{
			if (i-j>=0)
				iMsgCode|=MsgBits[i-j]<<(30-j);
		}

		m_bch.SetData(iMsgCode);
		m_bch.Encode();

		iMsgCode=m_bch.GetEncodedData();

		if ((iStartFrame*2+1+iCodeword)%17==0)
			iCodeword++;

		iPOCSAGMsg[iStartFrame*2+1+iCodeword]=iMsgCode;

		iCodeword++;
	}

	return iPOCSAGMsg;
}

int* encodeAlphaNumericMsg(int number, int type, std::string message) 
{

	int i;
	int iReciever=number;

	//Reset the codewords
	for (i=0; i<68; i++)
		iPOCSAGMsg[i]=POCSAG_IDLE_CODEWORD;

	//Initilizing Synch Codewords
	for (i=0; i<68; i+=17)
		iPOCSAGMsg[i]=POCSAG_SYNCH_CODEWORD;

	//compute address codeword
	int iStartFrame=iReciever%8;

	/*
	for (i=0; i<iStartFrame*2; i++)
		iPOCSAGMsg[i]=POCSAG_IDLE_CODEWORD;
	*/

	int iAddress=iReciever >> 3;
	iAddress=iAddress<<2;
	iAddress|=type;				//Funktionsbits
	iAddress=iAddress<<11;

	m_bch.SetData(iAddress);
	m_bch.Encode();
	int iAddressEnc=m_bch.GetEncodedData();

	iPOCSAGMsg[iStartFrame*2+1]=iAddressEnc;

	//computer message codeword
	std::string TempBits;
	int iLen = message.size();
	char cMessage[2048];
	memset(cMessage,' ', 2048);
	message.copy(cMessage, iLen);

	for (i=0; i<iLen; i++)
	{
		char c=cMessage[i];

		for (int j=0; j<128; j++)
			if (ASCII7BitTable[j].Letter==c)
			{
				std::string TempStr=ASCII7BitTable[j].Bits;
				TempStr = std::string(TempStr.rbegin(), TempStr.rend()); //reverse string;
				TempBits+=TempStr;
				break;
			}
	}

	std::string TempStr = ASCII7BitTable[4].Bits;				//EOT = End of Transmission
	TempStr = std::string(TempStr.rbegin(), TempStr.rend()); //reverse string
	TempBits+=TempStr;

	//now we have bits of message!
	int iMessageCodewords=(iLen+1)*7/20;

	if (iMessageCodewords*20!=(iLen+1)*7)
		iMessageCodewords++;

	int iRemainBit=iMessageCodewords*20-7*(iLen+1);
	float fRemain=ceil((float) iRemainBit/7);
	iRemainBit=(int) fRemain;

	for (i=0; i<iRemainBit; i++)
	{
		TempStr=ASCII7BitTable[4].Bits;
		TempStr = std::string(TempStr.rbegin(), TempStr.rend()); //reverse string
		TempBits+=TempStr;
	}

	iRemainBit=TempBits.length()%20;

	//must remove iRemainBits from right of string
	TempBits=TempBits.substr(0, TempBits.length()-iRemainBit);

	std::bitset<2100> MsgBits(TempBits);

	int k=TempBits.length()-1;

	int iCodeword=1;

	for (i=k; i>=0; i-=20)
	{
		int iMsgCode=1<<31;		//0x80000000

		for (int j=0; j<20; j++)
		{
			if (i-j>=0)
				iMsgCode|=MsgBits[i-j]<<(30-j);
		}

		m_bch.SetData(iMsgCode);
		m_bch.Encode();

		iMsgCode=m_bch.GetEncodedData();

		if ((iStartFrame*2+1+iCodeword)%17==0)
			iCodeword++;

		iPOCSAGMsg[iStartFrame*2+1+iCodeword]=iMsgCode;

		iCodeword++;
	}

	return iPOCSAGMsg;
}


void sendMessage(int type, int number, int function, std::string message) { 
     
	int* encodedMessage;
	if(type == 5){
	  encodedMessage = encodeNumericMsg(number, function, message); 
	}
	else if(type == 6){
	  encodedMessage = encodeAlphaNumericMsg(number, function, message); 
	}
	

	int c = 0;
	bool x = false;
	do {
	  x = radio.ptt_on();
	  
	}
	while(!x && c++ <10);  
	  
	delay(100);
	
	digitalWrite(ATDATA, LOW);
	digitalWrite(ATCLK, LOW);
	
	
	//Präambel an ATMega 8 FIFO senden
	for(int c=0;c<38;c++){
	for (int i = 31; i >= 0; i--) {
	
	  while(digitalRead(HANDSHAKE) == 0)  					//FIFO Puffer voll?, dann warten...
	  {
	    delayMicroseconds(20000);
	  }
	  
	  if (((unsigned long) POCSAG_PREAMBLE_CODEWORD & (unsigned long)(1UL << i)) >> i)
			digitalWrite(ATDATA, HIGH);
		else
			digitalWrite(ATDATA, LOW);
		delayMicroseconds(20);
		digitalWrite(ATCLK, HIGH);
		delayMicroseconds(150);
		digitalWrite(ATCLK, LOW);
		delayMicroseconds(150);
	}
	}
	
	
	
	//Nachricht an ATMega 8 FIFO senden (4 Batches fest 4*17=68)
	for(int i = 0;i<68;i++){
	  for(int c = 31; c>=0;c--){
	    while(digitalRead(HANDSHAKE) == 0)  				//FIFO Puffer voll?, dann warten...
	  {
	    delayMicroseconds(20000);
	  }
	  
	    if (((unsigned long) encodedMessage[i] & (unsigned long)(1UL << c)) >> c)
			digitalWrite(ATDATA, HIGH);
		else
			digitalWrite(ATDATA, LOW);
		delayMicroseconds(20);
		digitalWrite(ATCLK, HIGH);
		delayMicroseconds(150);
		digitalWrite(ATCLK, LOW);
		delayMicroseconds(150);
	  }
	  	  
	}

	digitalWrite(ATDATA, LOW);
	digitalWrite(ATCLK, LOW);	
	
	while(digitalRead(PTT) == 1)  						//Sender so lange eingeschaltet lassen, bis FIFO leer ist
	  {
	    delay(10);
	  }
	  
	  radio.ptt_off(); 							//wenn FIFO leer, PTT off

	std::cout << "ENDE" << std::endl;

  }


/*void processServerMessage(int sock){
  int n;
  char ack[4] = "+\r\n";
  char buffer[4000];
  bzero(buffer,sizeof(buffer));
 
  printf("Connection established\n");
  while((n = read(sock,buffer,sizeof(buffer)))>0) {
    
    //String an Doppelpunkt trennen und in parameters schreiben
    std::vector<std::string> parameters;
	std::stringstream ss(buffer);
	std::string item;
	int c=0;
	while(c++<4 && std::getline(ss, item, ':')){
	  parameters.push_back(item);
	}
	std::getline(ss, item);
	parameters.push_back(item);
    
    switch(buffer[0]){
      case '#': {	
	
	char * pEnd;
	int type = atoi(parameters[0].substr(parameters[0].length()-1,1).c_str());
	int id = strtol(parameters[2].c_str(),&pEnd,16);
	int function = atoi(parameters[3].c_str());
	std::string message = parameters[4];
	
	std::cout << "Nachricht: type=" << type <<", receiver=" << id<< ", function="<<function<<", message=" <<message<<"\n";
	
	(*queue[*lastQueueItem]).type = type;
	(*queue[*lastQueueItem]).id = id;
	(*queue[*lastQueueItem]).function = function;
	//strncpy((*queue[*lastQueueItem]).text, message.c_str(),5);
		
	*queueCount = *queueCount+1;
	*lastQueueItem = (*lastQueueItem+1)%1000;
      
	printf("%i Nachrichten in Warteschlange\n",*queueCount);
	
	char response[15] = "";
	sprintf(response,"#%02x +\r\n", (strtol(parameters[0].substr(1,3).c_str(),&pEnd,16)+1)%256);
	write(sock,response,strlen(response));
	
      }
	break;
      case '2':
      {
	
	char response[14] = "";
	
	sprintf(response,"2:%s:%04x\r\n", parameters[1].substr(0,parameters[1].length()-1).c_str(), *currentTime);
	
	write(sock,response,strlen(response));
	write(sock, ack, strlen(ack));
	
      }
	break;		
      case '3':{
	int delay = atoi(parameters[1].c_str());
	
	if(*currentTime+delay >0){
	  *currentTime = (*currentTime + delay ) % 65536;
	}else{
	  *currentTime = *currentTime+ delay+65536;
	}
	write(sock, ack, strlen(ack));

	printf("Time corrected: %i\n", delay);
	break;}
      case '4':{
	
	bzero(*slotEnabled, 16);
	strcpy(*slotEnabled, parameters[1].c_str());
	
	write(sock, ack, strlen(ack));
	printf("Slots received: %s\n", parameters[1].c_str());
	
	break;}
      default:
	printf("\n\nUnbekannte Nachricht:\n%s\n",buffer);
	break;
    }
    
  bzero(buffer,sizeof(buffer));
  }
  printf("Connection lost\n");
}
*/

/*

int main(int argc, char* argv[]) {
  if(argc <2) {
    std::cout << "Usage: sudo ./main <port>\n";
  } else {
     wiringPiSetup();

	//ADF7012 Pins:
	pinMode(LE, OUTPUT);
	pinMode(CE, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(SDATA, OUTPUT);
	pinMode(TXDATA, OUTPUT);
	pinMode(CLKOUT, OUTPUT);
	pinMode(MUXOUT, INPUT);
	pinMode(TXCLK, INPUT);
	
	//ATMega8 Pins:
	pinMode(ATDATA, OUTPUT);
	pinMode(ATCLK, OUTPUT);
	pinMode(HANDSHAKE, INPUT);
	pinMode(PTT, INPUT);

	//Objekt der Klasse Radio erstellen:
	radio = *new RadioAdf7012();
	
	//ADF7012 konfigurieren:
	radio.setup();
	
    
    currentTime = (int*)mmap(NULL, sizeof *currentTime, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *currentTime = 0;
    
   slotEnabled[0] = (char*)mmap(NULL, sizeof *slotEnabled, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    bzero(*slotEnabled,16);
    
    currentQueueItem = (int*)mmap(NULL, sizeof *currentQueueItem, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *currentQueueItem = 0;
    
    lastQueueItem = (int*)mmap(NULL, sizeof *lastQueueItem, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *lastQueueItem = 0;
    
    queueCount = (int*)mmap(NULL, sizeof *queueCount, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *queueCount = 0;
     
    queue[0] = (message*)mmap(NULL, sizeof *queue, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);      
               
    if(fork()==0){
      char prevValue = ' ';
	while(1){
	  delay(100);
	  *currentTime = (*currentTime+1) % 65536;
	
	  char hexValue[1];
	  sprintf(hexValue, "%X",((int)(*currentTime / 37.5)) % 16);
	  	  
	  if(prevValue != hexValue[0]){
	  
	  if(strchr(*slotEnabled, hexValue[0]) != NULL) {
	  printf("Slot: %c Count: %i Current: %i\n",hexValue[0], *queueCount, *currentQueueItem);
	    	    
	  if(*queueCount>0){  
	    
	    printf("Sende: type=%i, id=%i, function=%i, text=%s\n",(*queue[*currentQueueItem]).type,(*queue[*currentQueueItem]).id,(*queue[*currentQueueItem]).function,(*queue[*currentQueueItem]).text);
	    
	    sendMessage((*queue[*currentQueueItem]).type,(*queue[*currentQueueItem]).id,(*queue[*currentQueueItem]).function,(*queue[*currentQueueItem]).text);
	    *queueCount = *queueCount-1;
	    *currentQueueItem = (*currentQueueItem+1)%1000;
	  }
	  }}
	  prevValue=hexValue[0];
	}         
    } else {
	
   
	//socket öffnen
      int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(sockfd<0) {
     printf("Socket öffnen fehlgeschlagen.\n");
     return 0;
   }
   
   struct sockaddr_in serv_addr;
   bzero((char *) &serv_addr, sizeof(serv_addr));
   
   //Portnummer aus Programmaufruf als Integer speichern
   int portno = atoi(argv[1]);
   
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(portno);
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   
   if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) <0)
   {
     printf("Port %i konnte nicht geöffnet werden.\n", portno);
     return 0;
  }
   
   listen(sockfd, 5);
   
   struct sockaddr_in cli_addr;
   uint clilen = sizeof(cli_addr);
   char buffer[256];
   int newsockfd, n;
   
   printf("Server ready on port %u\n", portno);
   while(1){
    newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
    if(newsockfd >= 0){
      
 //     char welcome[33] = "[uPSDrpc/XOS v1.0-SCP-#2345678]\n";
	char welcome[33] = "[RasPager v1.0-SCP-#2345678]\r\n";
      write(newsockfd, welcome, strlen(welcome));
//      sprintf(newsockfd, welcome, 33);
//    printf("%s%s", newsockfd, welcome);
      if(fork()==0){
	close(sockfd);
	processServerMessage(newsockfd);
	exit(0);
 }else {
   close(newsockfd);
      
    }}}
  }
    return 0;
    
  }}


*/
//MAIN um Sender zu tasten (Oberwellenversuch)
int main() {

   
 
	
    wiringPiSetup();

	pinMode(LE, OUTPUT);
	pinMode(CE, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(SDATA, OUTPUT);
	pinMode(TXDATA, OUTPUT);
	pinMode(CLKOUT, OUTPUT);
	pinMode(MUXOUT, INPUT);
	pinMode(TXCLK, INPUT);
	
	pinMode(ATDATA, OUTPUT);
	pinMode(ATCLK, OUTPUT);
	pinMode(HANDSHAKE, INPUT);
	pinMode(PTT, INPUT);

	radio = *new RadioAdf7012();

	std::cout << "RasPagerTest Software 1.0 \n\n" << std::endl;
	delay (500);
	
	std::cout << "Frequenz ist eingestellt auf" << FREQUENCY << " Hz (Änderung in adf7012.h)"<< std::endl;
	delay (500);
	
	std::string mode = "x";
	while(mode != "q" && mode != "Q"){
	  
	std::cout << "\n\nBitte auswählen: FSK, CW, ZEIT oder RUF - Q und P (eintippen und ENTER drücken)"<< std::endl;
	delay (500);
	std::cin >> mode;
	std::cout << "ausgewählter Mode: " << mode << "\n\n" << std::endl;
	delay (500);	
	
	if (mode=="p"||mode=="P")
	{
	  std::cout << radio.get_params();
	  
	  char freq_err[20];
	  char mod_dev[20];
	  char pow_amp[20];

	  std::cout << "P: FREQ ERR CORR"<< std::endl;
	  std::cin >> freq_err;

	  std::cout << "P: MOD DEV"<< std::endl;
	  std::cin >> mod_dev;

	  
	  std::cout << "P: POW AMP"<< std::endl;
	  std::cin >> pow_amp;
	  
	  radio.set_params(atoi(freq_err),atoi(mod_dev),atoi(pow_amp));
	  
	  radio.setup();
	}
	
	if (mode=="cw"||mode=="CW")
	{
	radio.setup();
	
	int counter = 0;
	bool status = false;

	do {
	  status = radio.ptt_on();
    
	} while(!status && counter++ <10);  
  	
	std::cout << "CW: Zum Beenden beliebiges Zeichen eingeben und ENTER drücken..."<< std::endl;
	char cdummy;
	std::cin >> cdummy;
//	for (int i = 0; i<=10000; i++){
//	digitalWrite(ATDATA,LOW);
//	delay(10);
//	digitalWrite(ATDATA,HIGH);
//	delay(10);
//	}
	radio.ptt_off();
	}
	if (mode=="fsk"||mode=="FSK")
	{

	std::cout << "FSK: Wieviele Zeichen sollen gesendet werden?"<< std::endl;
	int zeichen;
	std::cin >> zeichen;
//	char cdummy;
//	std::cin >> cdummy;
	
	radio.setup();
	
	int counter = 0;
	bool status = false;

	do {
	  status = radio.ptt_on();
    
	} while(!status && counter++ <10);  
	
	for (int i = 0; i<=zeichen; i++){
	   
	    while(digitalRead(HANDSHAKE) == 0) {
	      delayMicroseconds(100);
	    }
	  
	  digitalWrite(ATDATA, i%2);
    	
	  delayMicroseconds(20);
	  digitalWrite(ATCLK, HIGH);
	  delayMicroseconds(100);
	  digitalWrite(ATCLK, LOW);
	  delayMicroseconds(50);
	}
	
	while(digitalRead(PTT) == 1) {
	  delay(100);
	}
	
	radio.ptt_off();
	}
if (mode=="ruf"||mode=="RUF")
	{

	std::cout << "RUF: Skyper Adresse eingeben"<< std::endl;
	int adresse;
	std::cin >> adresse;

	std::cout << "RUF: Nachricht eingeben"<< std::endl;

	while((fgetc(stdin)!= '\n'));
	
	char nachricht[200];
        fgets(nachricht,200,stdin);

	radio.setup();
	
	sendMessage(6,adresse,3,nachricht);

	}
if (mode=="zeit"||mode=="ZEIT")
	{
	radio.setup();
	sendMessage(5,2504,0, "101010   101010");
	}
}
}
