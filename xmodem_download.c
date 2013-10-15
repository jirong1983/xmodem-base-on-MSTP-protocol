
#include "xmodem.h"  // Public JCI header for the xmodem-x data transmission protocol

extern XmodemCB   *xModem_CB;
/************************************************************************
 * Procedure  : XMODEM_DownloadFrame( void)
 * Input      : none.
 * Output     : none.
 * Description: It receive a frame (128 bytes data + header) from xmodem RX interface
 *              and send a acknowledge (cancel, timeout).
 ************************************************************************/
static UNSIGNED8 XMODEM_DownloadFrame(void)
{
  UNSIGNED8  reply,d,bufferIndex;
  UNSIGNED16 Check=0,CheckSum;
  UNSIGNED8  XM_BufferLength;

  OSmemset(xModem_CB->PktBuf, 0 , xModem_CB->BufferLen + 2);

  XM_BufferLength = XMODEM_GET_Packet(xModem_CB->PktBuf,xModem_CB->BufferLen);

		
  if (XM_BufferLength == 0)
  {
    return XMODEM_ERROR;
  }
  else if(XM_BufferLength == 1)
  {
	if(xModem_CB->PktBuf[0]== EOT)
	{
	  //clear the count to 0, and begin a new data transfer
	  XM_BufferLength = 0;
	  reply = ACK;
	  xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
	  XMODEM_PUT_CHAR(&reply);
	  return XMODEM_COMPLETED;
	}
    else
	{
	  XM_BufferLength = 0;
	  return XMODEM_ERROR;
	}
  }
  else if(XM_BufferLength == 2)
  {
	if((xModem_CB->PktBuf[0]== CAN)&&(xModem_CB->PktBuf[1]== CAN))
	{
	  XM_BufferLength = 0;
	  return XMODEM_CANCEL;
	}
	else
	{
	  XM_BufferLength = 0;
	  return XMODEM_ERROR;
	}
  }
  else if((XM_BufferLength > 2)&&(XM_BufferLength < xModem_CB->BufferLen))
  {
	if((xModem_CB->PktBuf[0]== CAN)&&(xModem_CB->PktBuf[1]== CAN))
	{
	  XM_BufferLength = 0;
	  return XMODEM_CANCEL;
	}
	XM_BufferLength = 0;		  

	return XMODEM_ERROR;
  }
  else if(XM_BufferLength >= xModem_CB->BufferLen)
 {
	//clear the count to 0, and begin a new data transfer
	XM_BufferLength = 0;


	if(xModem_CB->PktBuf[0] != SOH)
	{
	  // Only Support 128 bytes CRC mode, so it must be SOH here
	  return XMODEM_ERROR;
	}
	
	if(xModem_CB->PktBuf[1] + xModem_CB->PktBuf[2] != 255)
	{
	  // it is a wrong data for its checksum
	  return XMODEM_ERROR;
	}
	
	CheckSum = 0;
	for (bufferIndex = 3; bufferIndex < (xModem_CB->PacketLen+3) ; bufferIndex++)
	{
	  if (xModem_CB->CheckLen==1)				 /* CheckSum */
	    Check += (UNSIGNED8)(xModem_CB->PktBuf[bufferIndex]);
	  else								 /* CRC */
		Check = Calculate_CRC(Check,(UNSIGNED8)xModem_CB->PktBuf[bufferIndex]);
	}
	  
	bufferIndex = xModem_CB->PacketLen+3;

	CheckSum=(xModem_CB->PktBuf[bufferIndex++]&0xff)<<8;
		
	CheckSum+=(xModem_CB->PktBuf[bufferIndex++]&0xff);
	
	if (Check!=CheckSum)
	{
	  // it is a wrong data for its checksum
	  return XMODEM_ERROR;
	}
	
	// later will add sequence checking, d should be 1
	d = xModem_CB->PktBuf[1] - xModem_CB->PktNum;
	if (d>1)
	{
	  //pause(2);
	  XMODEM_Cancel();
      return XMODEM_CANCEL;
	}
	
	xModem_CB->NAKMode = XnakC;//XnakC
	xModem_CB->NAKCount = 3;//when data communication begins, the next data will be timeout in 3 times retry
	
	if (d==0) 
	  return XMODEM_NOTHING;
	xModem_CB->PktNum = xModem_CB->PktBuf[1];
	
	if (xModem_CB->PktNum==0)
	  xModem_CB->PktNumOffset = xModem_CB->PktNumOffset + 256;
		
	return XMODEM_OK;
  }
  else
  {
    //suppose it will never run to here
	return XMODEM_CONTINUE;//return XMODEM_NOTHING;
  }
	
}
/******************************************************************************
* XMODEM_Download
* Download data to filed device by Xmodem protocol from utility PC or laptop.
*
* Arguments    : data_ptr -- The starting address to send in field devices
*                data_length -- The buffer to be sent
*                download_len -- The number of bytes to send
* Return Value : numBytes if the send was successful
                 0 if there was an error while sending
******************************************************************************/
 UNSIGNED8 XMODEM_Download
(   
  UNSIGNED8 *data_ptr,
  UNSIGNED32 data_length, 
  UNSIGNED32 *download_len
)
{
  UNSIGNED16 cpy_length;
  UNSIGNED8  ret;
  UNSIGNED8 reply;
  UNSIGNED32 addr = (UNSIGNED32)data_ptr;
  
//we will only support 128 byte CRC mode  
  XMODEM_Config(XMODEM_RECEIVE,XoptCRC);
	
  XMODEM_PUT_Message("\n\rStarting XMODEM Download file to Serial flash ");


  switch (xModem_CB->XOption)
  {                                       /* default CRC mode */
    case XoptCheck:
        ret = 'K';
        XMODEM_PUT_Message("(checksum mode)....\n\r");
        break;
    case XoptCRC:
    default:
        XMODEM_PUT_Message("(CRC mode)....\n\r");
        ret = 'C';
        break;
  }
   
  //adjust baudrate to better performance
  #ifdef SERIAL_BETTER_PERFORMANCE
  XM_MSTP_SET_BAUD_RATE(serPort[xmodem_Bus],XM_Baudrate_76800);
  #endif
  
  *download_len = 0;
	
  //clean the data in RX direction
  XMODEM_RX_Flush();
	
  XMODEM_SendNAK();   
	
  while (1)
  {
    OSSetWatchDogMask();
    ret=XMODEM_DownloadFrame(); 
	
    switch (ret)
    {
        case XMODEM_OK:
		    //cpy_length is update here
            if (data_length < xModem_CB->PacketLen)
               cpy_length = data_length;
            else
               cpy_length = xModem_CB->PacketLen;
            //update download data length
            *download_len += cpy_length;

            //program data(128 or less bytes) to serial flash
            xModem_CB->ProgramData((UNSIGNED32)addr,&xModem_CB->PktBuf[3],cpy_length);
 
		    addr +=cpy_length;
            data_length -= cpy_length;

            reply = ACK;                           /* send ACK */
            XMODEM_PUT_CHAR(&reply);
			break;
				
		case XMODEM_ERROR:
            if (xModem_CB->NAKCount-- == 0)
            {
              XMODEM_Cancel();
			  XMODEM_PUT_Message("Fail: Download has been canceled by timeout\n\r");
              return (XMODEM_TIMEOUT);
            }
			xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
			XMODEM_SendNAK();
            break;
				
		
        case XMODEM_TIMEOUT:
            pause(1);
			XMODEM_PUT_Message("Fail: Download time out\n\r");
            return XMODEM_TIMEOUT;

        case XMODEM_CANCEL:
            pause(1);
			XMODEM_PUT_Message(" Download has been canceled\n\r");
			return XMODEM_CANCEL;

        case XMODEM_COMPLETED:
            pause(1);
			XMODEM_PUT_Message("Download has been completed!\n\r");
            return(XMODEM_COMPLETED);
    }
  }

  return(XMODEM_COMPLETED);
}
