// BCHEncoder.h: interface for the CBCHEncoder class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BCHENCODER_H__506CC2C0_381D_4500_BA06_0D86174DFDC9__INCLUDED_)
#define AFX_BCHENCODER_H__506CC2C0_381D_4500_BA06_0D86174DFDC9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define POCSAG_PREAMBLE_CODEWORD	0xAAAAAAAA
#define POCSAG_IDLE_CODEWORD		0x7A89C197
#define POCSAG_SYNCH_CODEWORD		0x7CD215D8




class CBCHEncoder  
{
public:
	int GetEncodedData();
	int* GetEncodedDataPtr();
	void SetData(int* pData);
	void SetData(int iData);
	void Encode();
	void preamble();
	void synchronwort();

	CBCHEncoder();
	virtual ~CBCHEncoder();

private:
	void InitializeEncoder();
	void ComputeGeneratorPolynomial();
	void GenerateGf();

   /*
	* m = order of the field GF(2**5) = 5
	* n = 2**5 - 1 = 31 = length 
	* t = 2 = error correcting capability 
	* d = 2*t + 1 = 5 = designed minimum distance 
	* k = n - deg(g(x)) = 21 = dimension 
	* p[] = coefficients of primitive polynomial used to generate GF(2**5)
	* g[] = coefficients of generator polynomial, g(x)
	* alpha_to [] = log table of GF(2**5) 
	* index_of[] = antilog table of GF(2**5)
	* data[] = coefficients of data polynomial, i(x)
	* bb[] = coefficients of redundancy polynomial ( x**(10) i(x) ) modulo g(x)
	* numerr = number of errors 
	* errpos[] = error positions 
	* recd[] = coefficients of received polynomial 
	* decerror = number of decoding errors (in MESSAGE positions) 
	*/

	int m, n, k, t, d;
	int	length;
	int p[6];		// irreducible polynomial 
	int alpha_to[32], index_of[32], g[11];
	int recd[32], data[21], bb[11];
	int Mr[31];

};

#endif // !defined(AFX_BCHENCODER_H__506CC2C0_381D_4500_BA06_0D86174DFDC9__INCLUDED_)
