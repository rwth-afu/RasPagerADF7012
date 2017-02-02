// POCSAGDlg.cpp : implementation file
//

#include "stdafx.h"
#include "POCSAG_DLG.h"
#include "POCSAGDlg.h"
#include "7BitASCII.h"
#include "SerialSettings.h"


#include <math.h>
#include <bitset>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPOCSAGDlg dialog

CPOCSAGDlg::CPOCSAGDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPOCSAGDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPOCSAGDlg)
	m_Message = _T("Salam");
	m_EncodedMessage = _T("");
	m_Reciever = _T("1234567");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	SC.iBaudRate=_BAUDRATE_1200;
	SC.iDataBits=_DATABITS_8;
	SC.iParity=_PARITY_NONE;
	SC.iStopbits=_STOPBITS_1;
	SC.iHandshake=_HANDSHAKE_OFF;
	SC.iCOM=_COM2;
}

void CPOCSAGDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPOCSAGDlg)
	DDX_Text(pDX, IDC_MESSAGE, m_Message);
	DDX_Text(pDX, IDC_ENCODED_MSG, m_EncodedMessage);
	DDX_Text(pDX, IDC_RECIEVER, m_Reciever);
	DDV_MaxChars(pDX, m_Reciever, 7);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPOCSAGDlg, CDialog)
	//{{AFX_MSG_MAP(CPOCSAGDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_CALCULATE, OnCalculate)
	ON_BN_CLICKED(IDC_COM_SETTINGS, OnCOMPortSettings)
	ON_BN_CLICKED(IDOK, OnSend)
	ON_BN_CLICKED(IDCANCEL, OnClose)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPOCSAGDlg message handlers

BOOL CPOCSAGDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	LoadRegistry();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CPOCSAGDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CPOCSAGDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CPOCSAGDlg::OnCalculate() 
{
	UpdateData(TRUE);

	int i;
	int iReciever=atoi(m_Reciever.GetBuffer(0));

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
	iAddress|=0x3;
	iAddress=iAddress<<11;

	m_bch.SetData(iAddress);
	m_bch.Encode();
	int iAddressEnc=m_bch.GetEncodedData();

	iPOCSAGMsg[iStartFrame*2+1]=iAddressEnc;

	//computer message codeword
	CString TempBits;
	int iLen=m_Message.GetLength();
	char cMessage[2048];
	ZeroMemory(cMessage, 2048);
	strcpy(cMessage, m_Message.GetBuffer(0));

	for (i=0; i<iLen; i++)
	{
		char c=cMessage[i];

		for (int j=0; j<128; j++)
			if (ASCII7BitTable[j].Letter==c)
			{
				CString TempStr=ASCII7BitTable[j].Bits;
				TempStr.MakeReverse();
				TempBits+=TempStr;
				break;
			}
	}
/*
	CString TempStr=ASCII7BitTable[16].Bits;	//DLE = Data Link Escape
	TempStr.MakeReverse();
	TempBits+=TempStr;
*/
	CString TempStr=ASCII7BitTable[4].Bits;				//EOT = End of Transmission
	TempStr.MakeReverse();
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
		TempStr.MakeReverse();
		TempBits+=TempStr;
	}

	iRemainBit=TempBits.GetLength()%20;

	//must remove iRemainBits from right of string
	TempBits=TempBits.Left(TempBits.GetLength()-iRemainBit);

	std::bitset<2100> MsgBits(TempBits.GetBuffer(0));

	int k=TempBits.GetLength()-1;

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

	//now update display
	CString szRes;
	CString szTemp;
	for (i=0; i<68; i++)
	{
		szTemp.Format("%X   ", iPOCSAGMsg[i]);
		szRes+=szTemp;

		if (i%17==0)
			szRes+=CString("\r\n");

		if (i%17==8 || i%17==16)
			szRes+=CString("\r\n");
	}

	m_EncodedMessage=szRes;

	UpdateData(FALSE);
}

void CPOCSAGDlg::OnCOMPortSettings() 
{
	CSerialSettings set;
	set.SetSerialConfig(SC);
	if (set.DoModal()==IDOK)
	{
		SC=set.GetSerialConfig();
	}
}

void CPOCSAGDlg::OnSend() 
{
	int i, j;

	//open and config the serial port
	InitializeSerialPort();

	Packet MyPacket;

	//first send preamble
	//72 bytes=576 bits of 101010.....
	for (i=0; i<72; i++)
	{
		//Send POCSAG_PREAMBLE_CODEWORD
		MyPacket.iPacket=POCSAG_PREAMBLE_CODEWORD;

		for (j=3; j>=0; j--)
			m_Serial.Write(&MyPacket.cPacket[j], sizeof(char));
	}

	for (i=0; i<68; i++)
	{
		//Send iPOCSAGMsg[i];
		MyPacket.iPacket=iPOCSAGMsg[i];

		for (j=3; j>=0; j--)
			m_Serial.Write(&MyPacket.cPacket[j], sizeof(char));
	}

	m_Serial.Close();
}

//DEL void CPOCSAGDlg::InitializePOCSAGProtocol()
//DEL {
//DEL 	poc.iPreamble=POCSAG_PREAMBLE_CODEWORD;
//DEL 	poc.iIdle=POCSAG_IDLE_CODEWORD;
//DEL 	poc.iSynch=POCSAG_SYNCH_CODEWORD;
//DEL 
//DEL 	poc.iAddress=0;
//DEL 	ZeroMemory(&poc.iMessage, sizeof(poc.iMessage));
//DEL }

void CPOCSAGDlg::OnClose() 
{
	SaveRegistry();

	CDialog::OnOK();
}

void CPOCSAGDlg::SaveRegistry()
{
	CWinApp* pApp=(CWinApp*) AfxGetApp();
	pApp->WriteProfileBinary("Config", "SC", (LPBYTE) &SC, sizeof(SC));
}

void CPOCSAGDlg::LoadRegistry()
{
	CWinApp* pApp=(CWinApp*) AfxGetApp();
	UINT uiBytes=sizeof(SC);

	SerialConfig* pSC;
	if (pApp->GetProfileBinary("Config", "SC", (LPBYTE*) &pSC, &uiBytes))
	{
		memcpy(&SC, pSC, sizeof(SerialConfig));

		delete [] pSC;
	}
}

void CPOCSAGDlg::InitializeSerialPort()
{
	CSerial::EBaudrate iBaudRate;
	CSerial::EParity iParity;
	CSerial::EDataBits iDataBits;
	CSerial::EHandshake iHandshake;
	CSerial::EStopBits iStopbits;
	CString COM;

	switch (SC.iBaudRate)
	{
		case _BAUDRATE_1200:
			iBaudRate=CSerial::EBaud1200;
			break;

		case _BAUDRATE_2400:
			iBaudRate=CSerial::EBaud2400;
			break;

		case _BAUDRATE_9600:
			iBaudRate=CSerial::EBaud9600;
			break;

		case _BAUDRATE_19200:
			iBaudRate=CSerial::EBaud19200;
			break;

		case _BAUDRATE_14400:
			iBaudRate=CSerial::EBaud14400;
			break;

		case _BAUDRATE_38400:
			iBaudRate=CSerial::EBaud38400;
			break;

		case _BAUDRATE_56000:
			iBaudRate=CSerial::EBaud56000;
			break;

		case _BAUDRATE_57600:
			iBaudRate=CSerial::EBaud57600;
			break;

		case _BAUDRATE_115200:
			iBaudRate=CSerial::EBaud115200;
			break;
	}

	switch (SC.iDataBits)
	{
		case _DATABITS_5:
			iDataBits=CSerial::EData5;
			break;

		case _DATABITS_6:
			iDataBits=CSerial::EData6;
			break;

		case _DATABITS_7:
			iDataBits=CSerial::EData7;
			break;

		case _DATABITS_8:
			iDataBits=CSerial::EData8;
			break;
	}

	switch (SC.iHandshake)
	{
		case _HANDSHAKE_OFF:
			iHandshake=CSerial::EHandshakeOff;
			break;

		case _HANDSHAKE_SOFT:
			iHandshake=CSerial::EHandshakeSoftware;
			break;

		case _HANDSHAKE_HARD:
			iHandshake=CSerial::EHandshakeHardware;
			break;
	}

	switch (SC.iParity)
	{
		case _PARITY_NONE:
			iParity=CSerial::EParNone;
			break;

		case _PARITY_EVEN:
			iParity=CSerial::EParEven;
			break;

		case _PARITY_MARK:
			iParity=CSerial::EParMark;
			break;

		case _PARITY_SPACE:
			iParity=CSerial::EParSpace;
			break;

		case _PARITY_ODD:
			iParity=CSerial::EParOdd;
			break;
	}

	switch (SC.iStopbits)
	{
		case _STOPBITS_1:
			iStopbits=CSerial::EStop1;
			break;

		case _STOPBITS_1_5:
			iStopbits=CSerial::EStop1_5;
			break;

		case _STOPBITS_2:
			iStopbits=CSerial::EStop2;
			break;
	}

	switch (SC.iCOM)
	{
		case _COM1:
			COM="COM1";
			break;

		case _COM2:
			COM="COM2";
			break;

		case _COM3:
			COM="COM3";
			break;

		case _COM4:
			COM="COM4";
			break;
	}

	m_Serial.Open(COM);
	m_Serial.Setup(iBaudRate, iDataBits, iParity, iStopbits);
    m_Serial.SetupHandshaking(iHandshake);
}



