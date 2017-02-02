// BCHEncoder.cpp: implementation of the CBCHEncoder class.
//
//////////////////////////////////////////////////////////////////////

//#include "stdafx.h"
//#include "POCSAG_DLG.h"

#include "BCHEncoder.h"
#include "cstring"
#include "cstdlib"
#include "wiringPi.h"
#include "adf7012.h"
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBCHEncoder::CBCHEncoder()
{
	//Initialize Variables
	m = 5; n = 31; k = 21; t = 2; d = 5;
	length = 31;

	// Primitive polynomial of degree 5
    // x^5 + x^2 + 1
	p[0] = p[2] = p[5] = 1; 
	p[1] = p[3] = p[4] = 0;


	//Initialize BCH Encoder
	InitializeEncoder();
}

CBCHEncoder::~CBCHEncoder()
{

}

void CBCHEncoder::GenerateGf()
{
	/*
	* generate GF(2**m) from the irreducible polynomial p(X) in p[0]..p[m]
	* lookup tables:  index->polynomial form   alpha_to[] contains j=alpha**i;
	* polynomial form -> index form  index_of[j=alpha**i] = i alpha=2 is the
	* primitive element of GF(2**m) 
	*/

	int i, mask;
	mask = 1;
	alpha_to[m] = 0;

	for (i = 0; i < m; i++) 
	{
		alpha_to[i] = mask;

		index_of[alpha_to[i]] = i;

		if (p[i] != 0)
			alpha_to[m] ^= mask;

		mask <<= 1;
	}

	index_of[alpha_to[m]] = m;

	mask >>= 1;

	for (i = m + 1; i < n; i++) 
	{
		if (alpha_to[i - 1] >= mask)
		  alpha_to[i] = alpha_to[m] ^ ((alpha_to[i - 1] ^ mask) << 1);
		else
		  alpha_to[i] = alpha_to[i - 1] << 1;

		index_of[alpha_to[i]] = i;
	}

	index_of[0] = -1;
}


void CBCHEncoder::ComputeGeneratorPolynomial()
{
	/* 
	* Compute generator polynomial of BCH code of length = 31, redundancy = 10
	* (OK, this is not very efficient, but we only do it once, right? :)
	*/

	int ii, jj, ll, kaux;
	int test, aux, nocycles, root, noterms, rdncy;
	int cycle[15][6], size[15], min[11], zeros[11];

	// Generate cycle sets modulo 31 
	cycle[0][0] = 0; size[0] = 1;
	cycle[1][0] = 1; size[1] = 1;
	jj = 1;			// cycle set index 

	do 
	{
		// Generate the jj-th cycle set 
		ii = 0;
		do 
		{
			ii++;
			cycle[jj][ii] = (cycle[jj][ii - 1] * 2) % n;
			size[jj]++;
			aux = (cycle[jj][ii] * 2) % n;

		} while (aux != cycle[jj][0]);

		// Next cycle set representative 
		ll = 0;
		do 
		{
			ll++;
			test = 0;
			for (ii = 1; ((ii <= jj) && (!test)); ii++)
			// Examine previous cycle sets 
			  for (kaux = 0; ((kaux < size[ii]) && (!test)); kaux++)
					if (ll == cycle[ii][kaux])
						test = 1;
		} 
		while ((test) && (ll < (n - 1)));

		if (!(test)) 
		{
			jj++;	// next cycle set index 
			cycle[jj][0] = ll;
			size[jj] = 1;
		}

	} while (ll < (n - 1));

	nocycles = jj;		// number of cycle sets modulo n 
	// Search for roots 1, 2, ..., d-1 in cycle sets 

	kaux = 0;
	rdncy = 0;

	for (ii = 1; ii <= nocycles; ii++) 
	{
		min[kaux] = 0;

		for (jj = 0; jj < size[ii]; jj++)
			for (root = 1; root < d; root++)
				if (root == cycle[ii][jj])
					min[kaux] = ii;

		if (min[kaux]) 
		{
			rdncy += size[min[kaux]];
			kaux++;
		}
	}

	noterms = kaux;
	kaux = 1;

	for (ii = 0; ii < noterms; ii++)
		for (jj = 0; jj < size[min[ii]]; jj++) 
		{
			zeros[kaux] = cycle[min[ii]][jj];
			kaux++;
		}

	//printf("This is a (%d, %d, %d) binary BCH code\n", length, k, d);
	// Compute generator polynomial 
	g[0] = alpha_to[zeros[1]];
	g[1] = 1;		// g(x) = (X + zeros[1]) initially 

	for (ii = 2; ii <= rdncy; ii++) 
	{
	  g[ii] = 1;
	  for (jj = ii - 1; jj > 0; jj--)
	    if (g[jj] != 0)
	      g[jj] = g[jj - 1] ^ alpha_to[(index_of[g[jj]] + zeros[ii]) % n];
	    else
	      g[jj] = g[jj - 1];
	  
		g[0] = alpha_to[(index_of[g[0]] + zeros[ii]) % n];
	}
}

void CBCHEncoder::Encode()
{
	int    h, i, j=0, start=0, end=(n-k);

	// reset the M(x)+r array elements
	for (i = 0; i < n; i++)
		Mr[i] = 0;

	// copy the contents of data into Mr and add the zeros
	// use the copy method of arrays, its better :-)
	for (h=0; h < k; ++h)
		Mr[h] = data[h];


	while (end < n)
	{
		for (i=end; i>start-2; --i)
		{
			if (Mr[start] != 0)
			{
				Mr[i]^=g[j];
				++j;
			}
			else
			{
				++start;
				j = 0;
				end = start+(n-k);
				break;
			}
		}
	}


	//printf("start: %d, end: %d\n\n",start,end);
	j = 0;
	for (i = start; i<end; ++i)
	{
		bb[j]=Mr[i];
		++j;
	}
}



void CBCHEncoder::InitializeEncoder()
{
	GenerateGf();					// Generate the Galois Field GF(2**m) 
	ComputeGeneratorPolynomial();	// Compute the generator polynomial of BCH code 
}


void CBCHEncoder::SetData(int *pData)
{
	for (int i=0; i<21; i++)
		data[i]=pData[i];
}


int* CBCHEncoder::GetEncodedDataPtr()
{
	int i;
	int iEvenParity=0;
/*

	for (i = 0; i < length - k; i++)
	{
		recd[i+1] = bb[i];	// first (length-k) bits are redundancy 

		if (bb[i]==1)
			iEvenParity++;
	}

	for (i = 0; i < k; i++)
	{
		recd[i + length - k] = data[i];	// last k bits are data 

		if (bb[i]==1)
			iEvenParity++;
	}
*/
	for (i=0; i<21; i++)
	{
		//recd[i+11]=data[i];
		recd[31-i]=data[i];

		if (data[i]==1)
			iEvenParity++;
	}

	for (i=0; i<11; i++)
	{
		recd[10-i]=bb[i];

		if (bb[i]==1)
			iEvenParity++;
	}

	if (iEvenParity%2==0)
		recd[0]=0;
	else
		recd[0]=1;

	return (int*) recd;
}

void CBCHEncoder::SetData(int iData)
{
	//we only use the 21 most significant bits
	int iTemp=iData;

//	iTemp=iTemp >> 11;
	int j=0;

	for (int i=31; i>10; i--)
	{
		int iOne=1<<i;
		if (iTemp & iOne)
			data[j++]=1; //data[i-11]=1;
		else
			data[j++]=0; //data[i-11]=0;
	}
}

int CBCHEncoder::GetEncodedData()
{
	int Codeword[32];
	int iResult=0;

	std::memcpy(Codeword, GetEncodedDataPtr(), sizeof(int)*32);

	for (int i=0; i<32; i++)
	{
		if (Codeword[i])
			iResult|=(1<<i);
	}

	return iResult;
}
