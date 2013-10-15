
#include <My_config.h> 
#include <serialFlashLib.h>
#include "xmodem.h"         // Private JCI header for the xmodem function

// Global variable that hold the sfConfig structure inital status

// Global variable that holds the driver hooks for the active xmodem transfer method
// Move to Internal RAM
#pragma section RTOS
XmodemCB   *xModem_CB;
#pragma section

#ifdef INCLUDE_SERIAL_FLASH

/******************************************************************************
* seriaIF_Time_Delay
* Time Delay before feedback to Xmodem utility , that installed in PC/laptop
*
* Arguments    : None
* Return Value : None
******************************************************************************/
void seriaIF_Time_Delay(void)
{
  OSdelayshort(MSTP_INTERVAL);
  return;
}
/******************************************************************************
* seriaIF_TX_Status
* Check serial interface TX status.
*
* Arguments    : None
* Return Value : 
                TRUE: transmission is done.
				FALSE: transmission is still going on.
******************************************************************************/
UNSIGNED8 seriaIF_TX_Status(void)
{
  return (UNSIGNED8)SCI6.SSR.BIT.TEND;
}
/******************************************************************************
* seriaIF_Config
* Serial port need to be configured before receive or transfer data.
*
* Arguments    : xMode - receive or transfer mode
* Return Value : None
******************************************************************************/
void seriaIF_Config(UNSIGNED8 xMode)
{
  if(xMode == XMODEM_SEND)
	MSTPSI_SetCommEvents(serPort[xmodem_Bus], TRANSMITTER_EMPTY);
	
  if(xMode == XMODEM_RECEIVE)
	MSTPSI_SetCommEvents(serPort[xmodem_Bus], RXCHARS_AVAILABLE | RXERROR);

  return;
}
/******************************************************************************
* seriaIF_Read
* Read data from serial port
*
* Arguments    : buffer - The target address to store the data from serial port
                 size   - The size of data needs to be read 
* Return Value : The real length of data received from serial port
******************************************************************************/
UNSIGNED16 seriaIF_Read(UNSIGNED8 *buffer, UNSIGNED16 size)
{
  return MSTPSI_Read(serPort[xmodem_Bus], buffer, size);
}
/******************************************************************************
* seriaIF_Write
* Send data to serial port
*
* Arguments    : source - The address of data needs to be transfer
                 size   - The size of data needs to be transfered
* Return Value : None
******************************************************************************/
void seriaIF_Write(UNSIGNED8 *source, UNSIGNED16 size)
{
  MSTPSI_Write(serPort[xmodem_Bus], source, size);

  return;
}
/******************************************************************************
* seriaFlash_Program
* Program the data received from serial port to external serial flash in SPI bus
*
* Arguments    : addr - The start address of external serial flash
                 source - the RAM start addr , which store the data recieved from serial port
				 length - the length of data need to be programmed
* Return Value : None
******************************************************************************/
void seriaFlash_Program(UNSIGNED32 addr, UNSIGNED8* source, UNSIGNED32 length)
{
  //FAC3611 is using Micron N25Q serial flash, which needs to be erased before program data to it
  if((addr % SF_SUBSECTOR_SIZE) == 0)
	OSEEpromClear((UNSIGNED8 *)addr,SF_SUBSECTOR_SIZE);
  // Program data to N25Q
  OSEEpromWrite((UNSIGNED8 *)addr,source,length);

  return;
}
#endif

/******************************************************************************
* XMODEM_Config
* Used to setup/initial the xmodem structure hooks based on the interface configuration table.
* Expected to be called by xmodem upload and download
*
* Arguments    : XM_Mode - Xmodem mode support setting, right now it only supports 128 bytes 
                           CRC mode, since serial port MSTP limit 1K Xmodem CRC mode.
                 XM_Type - it define xmodem receive or transfer data mode.
* Return Value : None
******************************************************************************/
void XMODEM_Config (UNSIGNED8 XM_Mode,UNSIGNED8 XM_Type)
{
  if(xModem_CB == NULL)
  {
	xModem_CB = OSallocate(sizeof(XmodemCB));
	#ifdef INCLUDE_SERIAL_FLASH
	xModem_CB->XM_Interface[xmodem_Bus].InterfaceConfig = seriaIF_Config;
	xModem_CB->XM_Interface[xmodem_Bus].InterfaceGetData = seriaIF_Read;
	xModem_CB->XM_Interface[xmodem_Bus].InterfaceSendData = seriaIF_Write;
	xModem_CB->XM_Interface[xmodem_Bus].InterfaceTXEnd = seriaIF_TX_Status;
	xModem_CB->XM_Interface[xmodem_Bus].InterfaceTimeDelay = seriaIF_Time_Delay;
	#endif
	xModem_CB->ProgramData = seriaFlash_Program;
  }
	
  xModem_CB->PktBufPtr = 0;
  xModem_CB->PktNumOffset = 0;
  xModem_CB->PktNum = 0;
  xModem_CB->PktNumSent = 0;
  xModem_CB->TimeOut= UPLOAD_TIMEOUT_SECOND;
  xModem_CB->NAKCount = DOWNLOAD_TIMEOUT_SECOND; 
  xModem_CB->XMode = XM_Mode;
  XMODEM_SetOption(XM_Type);

  return;

}


