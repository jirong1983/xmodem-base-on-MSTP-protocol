
#include "xmodem.h"  // Public JCI header for the xmodem-x data transmission protocol
extern XmodemCB   *xModem_CB;

/******************************************************************************
* XMODEM_Check_Tran_Done
* Check Xmodem transmission is done or not, and double check that again after
* an interval, which is depended on interface specification. MSTP interface is 5ms.
*
* Arguments    : None
* Return Value : TRUE: transmission is done.
*                FALSE: transmission is still going on. 
******************************************************************************/
UNSIGNED8 XMODEM_Check_Tran_Done(void)
{

  if(xModem_CB->XM_Interface[xmodem_Bus].InterfaceTXEnd())
  {
    xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
    //still 5ms
    if(xModem_CB->XM_Interface[xmodem_Bus].InterfaceTXEnd())
	  return TRUE;
  }
  return FALSE;
}

/******************************************************************************
* XMODEM_PUT_PACKETS
* Send packets interface for xmodem protocol.
*
* Arguments    : ptr - The start address of the data to be sent
*                length - The number of bytes being operated on
* Return Value : The subsector the end of the sequence stops on
******************************************************************************/
UNSIGNED32 XMODEM_PUT_PACKETS(UNSIGNED8* ptr, UNSIGNED32 length)
{
  UNSIGNED32 len = length;
  
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceConfig(XMODEM_SEND);
  
  if(length != 0)
  {
    xModem_CB->XM_Interface[xmodem_Bus].InterfaceSendData( ptr, length);
	while(XMODEM_Check_Tran_Done()==FALSE) ;
  }
  
  return len;  
}

/******************************************************************************
* XMODEM_PUT_CHAR
* Send one byte data for xmodem protocol.
*
* Arguments    : Data - The address of the data to be sent
*               
* Return Value : None
******************************************************************************/
void XMODEM_PUT_CHAR(UNSIGNED8* Data)
{
  XMODEM_PUT_PACKETS(Data,1);

  return;
}

/******************************************************************************
* XMODEM_PUT_Message
* Send xmodem protocol notification information to interface .
*
* Arguments    : String - The start address of the message to be sent
*               
* Return Value : None
******************************************************************************/
void XMODEM_PUT_Message(UNSIGNED8* String)
{
  UNSIGNED8 index = 0;
  
  while ( *(String+index) != 0 )         /* While not end of string */
  	index ++;

 xModem_CB->XM_Interface[xmodem_Bus].InterfaceConfig(XMODEM_SEND);
 xModem_CB->XM_Interface[xmodem_Bus].InterfaceSendData( String, index);

 while(XMODEM_Check_Tran_Done()==FALSE) ;
 
}


/******************************************************************************
* XMODEM_GET_CHARS
* Receive data interface of Xmodem protocol in one xmodem time fram
* it will be called by XMODEM_GET_Packet()
*
* Arguments    : Buffer - The start address of memory to receive data from xmodem interface
*                bufSize - the target buffer size that want to be get from xmodem interface
*                len -  the real data length that have been received              
* Return Value : the real data length that have been received
******************************************************************************/
UNSIGNED16 XMODEM_GET_CHARS(UNSIGNED8 *Buffer, UNSIGNED16 bufSize,UNSIGNED16 *len)
{
  UNSIGNED16 length = 0;
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
	
  length = xModem_CB->XM_Interface[xmodem_Bus].InterfaceGetData(Buffer, bufSize);
  *len = length;
  return length;
}

/******************************************************************************
* XMODEM_GET_Packet
* firstly check whether data received from xmodem interface within one second.
* if found data, it will continuelly get data until received target length(bufSize)
*
* Arguments    : Buffer - he start address of memory to receive data from xmodem interface
*                bufSize - the target buffer size that want to be get from xmodem interface
* Return Value : the real data length that have been received
******************************************************************************/
UNSIGNED16 XMODEM_GET_Packet(UNSIGNED8 *Buffer, UNSIGNED16 bufSize)
{
  UNSIGNED16 len = 0;
  UNSIGNED16 rx_index = 0;
  UNSIGNED8  retryCount = 0;
  //first enable RX
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceConfig(XMODEM_RECEIVE);

  while((rx_index == 0)&&(retryCount++ < MSTP_RX_DETECT))
  {
    rx_index = XMODEM_GET_CHARS(Buffer,MSTP_TRANS_SIZE,&len); 
  }

  //found data and get all here
  if(rx_index > 0)
  {
    while( XMODEM_GET_CHARS(Buffer + rx_index,MSTP_TRANS_SIZE,&len)) 
	{
	  rx_index += len;
	  if(rx_index >= bufSize)
		break;
	}
  }
 return rx_index;
}
/******************************************************************************
* XMODEM_RX_Flush
* Flush data of xmodem RX direction to avoid noise before formal communication
*
* Arguments    : None
* Return Value : None
******************************************************************************/
void XMODEM_RX_Flush(void)
{
  UNSIGNED16 len = 0;
  //first enable RX
  //SERIAL_ENABLE_RECEIVER_EVENTS(serPort[xmodem_Bus])
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceConfig(XMODEM_RECEIVE);
	
  //Max buffer size is 128, so just need 2 times read
  XMODEM_GET_CHARS(xModem_CB->PktBuf,BUFFER_SIZE,&len);

  return;
}

/******************************************************************************
* XMODEM_SendNAK
* Send NAK to initial xmodem transfer or request re-send data.
* It requests a time interval before send out.
*
* Arguments    : None
* Return Value : 
******************************************************************************/
UNSIGNED8 XMODEM_SendNAK(void)
{
  UNSIGNED8 ch;
	
  if (xModem_CB->NAKMode==XnakNAK)
  {
    ch = NAK;
  }
  else 
  {
    ch = 'C';
  }
	
  XMODEM_PUT_CHAR(&ch);
    
  return (XMODEM_NOTHING);
}
/******************************************************************************
* XMODEM_SetOption
* configure Xmodem data parameters and initial buffer to store data
*
* Arguments    : option - Current only support 128 bytes CRC mode for receive and send directions
* Return Value : 
******************************************************************************/
void XMODEM_SetOption(UNSIGNED8 option)
{
  if(xModem_CB->XOption == option)
    return;
	
  xModem_CB->XOption = option;
    
  switch (xModem_CB->XOption)
  {
    case XoptCheck:                 /* Checksum , Not support this moment*/
        xModem_CB->PacketLen = 128;
		xModem_CB->BufferLen = 132;
        xModem_CB->CheckLen = 1;
		xModem_CB->NAKMode=XnakNAK;
        break;
    case XoptCRC:                   /* CRC */
        xModem_CB->PacketLen = 128;
		xModem_CB->BufferLen = 133;
        xModem_CB->CheckLen = 2;
		xModem_CB->NAKMode=XnakC;
        break;
    case Xopt1K:                    /* 1K ,Not support this moment*/
        xModem_CB->PacketLen = 1024;
		xModem_CB->BufferLen = 1029;
        xModem_CB->CheckLen = 2;
		xModem_CB->NAKMode=XnakC;
		break;
  }
	
  //allocate buffer for packet
  if(xModem_CB->PktBuf == NULL)
	xModem_CB->PktBuf = (UNSIGNED8 *)OSallocate(xModem_CB->BufferLen + 2);
}
/******************************************************************************
* XMODEM_Cancel
* Cancel xmodem transmission
*
* Arguments    : None
* Return Value : None
******************************************************************************/
void XMODEM_Cancel(void)
{
  UNSIGNED8 ch;
  
  ch = CAN;
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
  XMODEM_PUT_CHAR(&ch);
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
  XMODEM_PUT_CHAR(&ch);
  
  ch=BSP;
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
  XMODEM_PUT_CHAR(&ch);
  xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
  XMODEM_PUT_CHAR(&ch);
  
  xModem_CB->XMode = XMODEM_NONE; 
}
/******************************************************************************
* XMODEM_CalcCheck
* Calculates the CRC for 128 bytes of a given address
*
* Arguments    : PktBuf - The start address of the sequence
* Return Value : 2 bytes CRC value
******************************************************************************/
UNSIGNED16 XMODEM_CalcCheck(UNSIGNED8* PktBuf)
{
  UNSIGNED16     i;
  UNSIGNED16    Check;

  if (xModem_CB->CheckLen==1)                /* CheckSum */
  {
    Check = 0;
    for (i = 0 ; i < xModem_CB->PacketLen ; i++)
        Check = Check + (UNSIGNED8)(PktBuf[3+i]);
    return (Check & 0xff);
  }
  else                                /* CRC */
  {   
    Check = 0;
    for (i = 0 ; i < xModem_CB->PacketLen ; i++)
        Check = Calculate_CRC(Check,PktBuf[3+i]);
    return (Check);
    }
}
/******************************************************************************
* serialFlashCalculateStartBlock
* Calculates the page number of a given address
*
* Arguments    : startOffset - The start address of the sequence
* Return Value : The page number that the address is part of
******************************************************************************/
UNSIGNED16 Calculate_CRC(UNSIGNED16 crc,UNSIGNED8 ch)
{                                                 
  UNSIGNED8 i;					 
  UNSIGNED16 ch1, accum;				

  accum = 0;                                    
  ch1 = ((crc >> 8) ^ ch) << 8;                 
  for (i = 8; i > 0; i--)                       
  {                                             
    if ((ch1 ^ accum) & 0x8000)               
      accum = (accum << 1) ^ 0x1021;        
    else                                      
      accum <<= 1;  
	  
    ch1 <<= 1;                                
  }                                             
  return( (crc << 8) ^ accum);                  
} 


