
#include <serialFlashLib.h>  // Private JCI header for the serial flash library
#include "xmodem.h"  // Public JCI header for the xmodem-x data transmission protocol
extern XmodemCB   *xModem_CB;
/************************************************************************
 * Procedure  : XMODEM_UploadFrame( void)
 * Input      : none.
 * Output     : none.
 * Description: It send a frame (128 bytes data + header) to xmodem out interface
 *              and wait for acknowledge (cancel, timeout).
 *              if ACK come => XMDMStatus = XMODEM_OK
 *              if timeout  => XMDMStatus = XMODEM_ABORT
 *              if CAN come => XMDMStatus = XMODEM_CANCEL
 ************************************************************************/
static UNSIGNED8 XMODEM_UploadFrame(void)  
{                                                 
  UNSIGNED16    i;
  UNSIGNED16    Check;
  UNSIGNED8     SendFlag;
  UNSIGNED16    PktBufCount;
  UNSIGNED8     opcode_len;
  UNSIGNED16    dataLength;
  UNSIGNED8     temp[BUFFER_SIZE];
  UNSIGNED8     timeCount = 0;

    SendFlag = FALSE; 
    
	
  //firstly detect data from PC utility to get transfer status
  while(SendFlag == FALSE)
  {
    //check wether it has been timeout (20s) for upload 
	if( ++timeCount >= xModem_CB->TimeOut)
	  return  XMODEM_TIMEOUT;

    OSmemset(temp, 0 , BUFFER_SIZE);
    opcode_len = XMODEM_GET_Packet(temp,MSTP_TRANS_SIZE);

    if (opcode_len != 0)  //frank change to 0xFF
    {
      switch (temp[0]&0xff) 
      {   
        case ACK:
          if(xModem_CB->XMode==XMODEM_NONE)      
          {
            xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
            return (XMODEM_COMPLETED);
          }
          else if (xModem_CB->PktNumSent==(UNSIGNED8)(xModem_CB->PktNum+1))  
          {
            xModem_CB->PktNum = xModem_CB->PktNumSent;
            if (xModem_CB->PktNum==0)
              xModem_CB->PktNumOffset = xModem_CB->PktNumOffset + 256;
              SendFlag = TRUE;
          }
          break;
		  
        case NAK:
          SendFlag = TRUE;
          break;

        case CAN:
          if(temp[1] == CAN)  
		    return  XMODEM_ABORT;
          break;

        case 'C':
          if ((xModem_CB->PktNum==0) && (xModem_CB->PktNumOffset==0))
          {
           // force to be CRC mode, only support CRC
            SendFlag = TRUE;
           }
           break;
      }
    }
		  	
  }

		
  //send data to uitilty
  if (xModem_CB->PktNumSent==xModem_CB->PktNum) /* make a new packet */
  {
	OSmemset(xModem_CB->PktBuf, 0 , xModem_CB->BufferLen+2);

	xModem_CB->PktNumSent++;
			
    if (xModem_CB->PacketLen==128)
      xModem_CB->PktBuf[0] = SOH;
    else
      xModem_CB->PktBuf[0] = STX;
	  
    xModem_CB->PktBuf[1] = xModem_CB->PktNumSent;
    xModem_CB->PktBuf[2] = ~ ((UNSIGNED8)xModem_CB->PktNumSent);

    i = 1;

    //read 128 bytes or less data from serial flash
    if(xModem_CB->DataLen > 0)
	{
       if(xModem_CB->PacketLen >= xModem_CB->DataLen )
       {
	     dataLength = xModem_CB->DataLen;
       }
	   else		
	   {
	     dataLength = xModem_CB->PacketLen;			
	   }

	   if(serialFlash_Read((UNSIGNED32)xModem_CB->PktBufPtr,&xModem_CB->PktBuf[3],dataLength))
	   {
		  xModem_CB->PktBufPtr += dataLength;
		  i += dataLength;
		  xModem_CB->DataLen -= dataLength;
	   }
	}	
     if (i>1)
     {
        while (i<=xModem_CB->PacketLen)
        {
          xModem_CB->PktBuf[2+i] = SERIAL_FLASH_BLANK_DATA;  //frank change from 0x1A to 0xFF
          i++;
        }

        Check = XMODEM_CalcCheck(xModem_CB->PktBuf);
         
		if(xModem_CB->CheckLen==1) /* Checksum */
           xModem_CB->PktBuf[xModem_CB->PacketLen+3] = (UNSIGNED8)Check;
        else 
        {
           xModem_CB->PktBuf[xModem_CB->PacketLen+3] = (UNSIGNED8)((Check&0xff00)>>8);
           xModem_CB->PktBuf[xModem_CB->PacketLen+4] = (UNSIGNED8)(Check&0xff);
         }
		 
        PktBufCount = XMODEM_BUFFER_SIZE;
     }
     else
     { 
	    /* have finished receiving all data
	    send EOT */
        xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay();
        xModem_CB->XMode=XMODEM_NONE;
        xModem_CB->PktBuf[0] = EOT;
        PktBufCount = 1;
		while(PktBufCount != 0)
           PktBufCount -= XMODEM_PUT_PACKETS(xModem_CB->PktBuf,PktBufCount);
		
        return (XMODEM_COMPLETED);
     }

   }
   else 
   { /* resend packet */
      PktBufCount = xModem_CB->BufferLen;
   }
	
  PktBufCount -= XMODEM_PUT_PACKETS(xModem_CB->PktBuf,PktBufCount);
	
  return (UNSIGNED8)XMODEM_OK;

}                                                 

/******************************************************************************
* XMODEM_Upload
* Read data from filed device by Xmodem protocol to PC or laptop.
*
* Arguments    : data_ptr -- The starting address to read from
*                data_length -- The buffer to read data into
*                upld_len -- The number of bytes to read
* Return Value : numBytes if the read was successful
                 0 if there was an error while reading
******************************************************************************/
 UNSIGNED8 XMODEM_Upload
(   
  UNSIGNED8 *data_ptr,
  UNSIGNED32 data_length, 
  UNSIGNED32 *upld_len
)
{                                                 
  UNSIGNED8     ret;
 
  *upld_len = 0;                                 

  XMODEM_Config(XMODEM_SEND,XoptCRC);
    
  xModem_CB->DataLen = data_length;   
  xModem_CB->PktBufPtr =(UNSIGNED32)data_ptr; 
	//flush RX channel
  XMODEM_RX_Flush();

  XMODEM_PUT_Message("Starting XMODEM Upload File From Serial Flash...\r\n");

  //adjust baudrate to better performance
  #ifdef SERIAL_BETTER_PERFORMANCE
  XM_MSTP_SET_BAUD_RATE(serPort[xmodem_Bus],XM_Baudrate_76800);
  #endif
  
  while (xModem_CB->DataLen >= 0)                      
  {       
	OSSetWatchDogMask();
    ret=XMODEM_UploadFrame();
    switch (ret)
    {
      case XMODEM_ABORT:
	    XMODEM_PUT_Message("Upload has been aborted!\n\r");
        return (XMODEM_ABORT);

      case XMODEM_TIMEOUT:
		XMODEM_PUT_Message("XMODEM Upload has been timeout, return to factory test command mode...\r\n");
         return (XMODEM_TIMEOUT);

      case XMODEM_COMPLETED:
	    XMODEM_PUT_Message("Upload has been completed!\n\r");
        return(XMODEM_COMPLETED);                   

      case XMODEM_OK:
        *upld_len += xModem_CB->PacketLen;
         break;
    }
  }
  XMODEM_PUT_Message("Upload has been completed!\n\r");
  return(XMODEM_COMPLETED);                   
}  
