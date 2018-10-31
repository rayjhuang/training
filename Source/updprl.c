
#include "global.h"

volatile BYTE bdata u8PrlStatus _at_ 0x24;
sbit bPrlSent  = u8PrlStatus^0;
sbit bPrlRcvd  = u8PrlStatus^1;
sbit bPhySent  = u8PrlStatus^2;
sbit bPhyRetry = u8PrlStatus^3; // for tReceive is tight, tell PHY just send the prior-prepared TxData
sbit bPhyPatch = u8PrlStatus^4;

enum {
   PRL_Tx_Wait_for_Message_Request,       // 0x00
   PRL_Tx_Reset_for_Transmit,             // 0x01
   PRL_Tx_Construct_Message,              // 0x02
   PRL_Tx_Wait_for_PHY_Response_TxDone,   // 0x03
   PRL_Tx_Wait_for_PHY_Done,              // 0x04
   PRL_Tx_Wait_for_PHY_Response,          // 0x05
   PRL_Tx_Match_MessageID,                // 0x06
   PRL_Tx_Message_Sent,                   // 0x07
   PRL_Tx_Check_Retry_Counter,            // 0x08
   PRL_Tx_Transmission_Error              // 0x09
} u8StatePrlTx _at_ 0x16 ;

enum {
   PRL_Rx_Wait_for_PHY_Message,
   PRL_Rx_Layer_Reset_for_Receive,
   PRL_Rx_Send_GoodCRC,
   PRL_Rx_Check_MessageID,
   PRL_Rx_Store_MessageID
} u8StatePrlRx _at_ 0x17;

BYTE u8RetryCounter;
BYTE u8MessageIDCounter;
BYTE u8StoredMessageID;

void PRL_Tx_PHY_Layer_Reset () // PRL_Tx_PHY_Layer_Reset
{
   u8PrlStatus = 0;
   u8StatePrlTx = PRL_Tx_Wait_for_Message_Request;
   u8StatePrlRx = PRL_Rx_Wait_for_PHY_Message;
   u8StoredMessageID = 0xFF;
   u8MessageIDCounter = 0;
}

void InitDiscoverId (BYTE rsp, BYTE ndo)
{
   TxHdrL   |= 0x0F; // VDM
   TxHdrH   |= ndo;
   TxData[0] = rsp; // initiate/ACK/NAK (DiscoverID)
   TxData[1] = bPESta_PD2 ? 0x80 : 0xA0; // structured/SVDM Ver=2.0
   *((WORD*)&TxData[2]) = BIG_ENDIAN(SID_STANDARD);
   if (rsp!=0x01 && // not initiate Discover Identity (PD2/3 not yet determined)
         (bPESta_PD2 || // response a SVDM in PD2 mode
         (RxData[1] & 0x60) == 0)) // response a SVDM with Version = 1.0
   {
      TxData[1] &=~0x20;
   }
}

#define PowerRole 0x01

void ClrTxData (BYTE dat0, BYTE dat1, BYTE cnt)
{
   TxData[0] = dat0;
   TxData[1] = dat1;
   while (cnt--) TxData[cnt+2] = 0;
}

void Go_PRL_Tx_Construct_Message ()
{
   u8StatePrlTx = PRL_Tx_Construct_Message;
   TXCTL = 0x39; // auto-preamble/SOP/CRC32/EOP
#ifndef CONFIG_CAN1110C0
   if (bPhyPatch) // patch auto-TX-GoodCRC
   {
      TxHdrL = 0x41; // GoodCRC, SpecRev=1
      if (bPESta_DFP) TxHdrL |= 0x20; // Data Role
      TxHdrH = PowerRole | (PRLRXH&0x0E); // copy the rcvd MessageID
   }
   else
   {
#endif
   TxHdrL = bPESta_PD2 ? 0x40 : 0x80; // SpecRev
   if (bPESta_DFP) TxHdrL |= 0x20; // Data Role
   TxHdrH = PowerRole | (u8MessageIDCounter<<1);
   switch (u8StatePE)
   {
   case PE_SRC_Send_Soft_Reset:
      TxHdrL |= 0x0D; // Soft Reset
      break;
   case PE_VCS_Send_PS_RDY:
   case PE_SRC_Transition_PS_RDY:
      TxHdrL |= 0x06; // PS_RDY
      break;
   case PE_VCS_Accept_Swap:
   case PE_DRS_Accept_Swap:
   case PE_SRC_Soft_Reset:
   case PE_SRC_Transition_Accept:
      TxHdrL |= 0x03; // Accept
      break;
   case PE_SRC_Send_Reject:
   case PE_SRC_Capability_Response:
      TxHdrL |= 0x04; // Reject
      break;
   case PE_SRC_Send_Source_Alert:
      TxHdrL |= 0x06; // Alert
      TxHdrH |= 0x10; // NDO=1
      ClrTxData(0,0,4);
      TxData[3] = 0x10; // only because of bProCLChg
      break;
   case PE_SRC_Send_Not_Supported:
      TxHdrL |= bPESta_PD2 ? 0x04 : 0x10; // Reject or Not_Supported
      break;
   case PE_SRC_Give_Source_Status:
      TxHdrL |= 0x02; // Status
      TxHdrH |= 0xA0; // extended, NDO=2
      ClrTxData(5,0x80,6); // length of Status Data Block, chunked, number=0, request=0
//    TxData[2] = GetTemperature(1); // Internal Temp
      TxData[3] = 2; // Present Input=External Power
//    TxData[4] = 0; // Present Battery Input
      TxData[5] = bProCLimit ? 0x10 : 0x00; // Event Flags
//    TxData[6] = 0x00; // Temperature Status
//    TxData[7] = 0x00; // padding
      break;
#ifdef CONFIG_PPS
   case PE_SRC_Give_PPS_Status:
      TxHdrL |= 0x0C; // PPS_Status
      TxHdrH |= 0xA0; // extended, NDO=2
      ClrTxData(4,0x80,6); // length of Status Data Block, chunked, number=0, request=0
      TxData[2] = 0xFF; // Output Voltage (low byte)
      TxData[3] = 0xFF; // Output Voltage (high byte)
      TxData[4] = 0xFF; // Output Current
      //TxData[5] = bProCLimit ? 0x10 : 0x00; // Real Time Flags
      /* Gary modify at 180222 (Fixed bug: OMF bit error) */
      TxData[5] = bProCLimit ? 0x08 : 0x00; // Real Time Flags
      break;
#endif
   case PE_SRC_Give_Source_Cap_Ext:
      TxHdrL |= 0x01; // Source_Cap_Ext
      TxHdrH |= 0xF0; // extended, NDO=7
      ClrTxData(24,0x80,26); // length of Status Data Block, chunked, number=0, request=0
      MEM_COPY_C2X(&(TxData[2]),lpVdmTable,12); // VID, 0xXXXX, XID, 0xHWFW, PID
      *((WORD*)&TxData[4]) = *((WORD*)&TxData[12]); // PID
      *((WORD*)&TxData[12]) = 0;
      //TxData[25] = 27; // Source PDP
      TxData[25] = u8SrcPDP; /* Gary modify at 180222 (TD.PD.VNDI3.E9 Source Capabilities Extended for Ellisys) */
      break;
   case PE_SRC_VDM_Identity_Request:
      TXCTL = 0x3A; // auto-preamble/SOP'/CRC32/EOP
      InitDiscoverId(0x01,0x10); // initiate Discover Identity
      TxHdrH &=~0x01; // PwrRole -> Cable shall be '0'
      TxHdrL &=~0x20; // DatRole -> rsvd shall be '0'
      break;
   case PE_SRC_Send_Capabilities:
      TxHdrL |= 0x01; // Source_Capabilities
      TxHdrH |= u8NumSrcPdo << 4;
      MEM_COPY_X2X(TxData, SRC_PDO[0], u8NumSrcPdo * 4);
      break;
   case PE_BIST_Carrier_Mode:
      bTmr0PM = 0;
      bTmr0TO = 0;
      TXCTL = 0x40; // auto encoded K code
      break;
   case PE_SRC_Hard_Reset:
      TXCTL = 0x48; // K-code encoding with preamble
      break;
#ifdef CONFIG_QC4
   case PE_RESP_VDM_Send_QC4_Plus:
      TxHdrL |= 0x0F; // VDM
      TxHdrH |= 2<<4;
      TxData[0] = 0xA0; // ACK
      TxData[1] = RxData[1];
      *((WORD*)&TxData[2]) = BIG_ENDIAN(VID_QUALCOMM_QC40);
      switch (RxData[1] & 0x7F) // CMD1
      {
      case 0x10: // Inquire charger case temperature
         TxHdrH &=~0x70;
         TxHdrH |= 1<<4;
         TxData[0] = 0x50; // NAK
         break;
      case 0x0B: // Inquire charger connector temperature
         TxData[4] = GetTemperature(0); // takes long!?
         TxData[5] = 0x00;
         TxData[6] = 0x00;
         TxData[7] = 0x00;
         break;
      case 0x06: // Inquire voltage at connector
         TxData[4] = 0x00;
         TxData[5] = DACV1<<3;
         TxData[6] = DACV1>>5;
         TxData[7] = 0x00;
         break;
      case 0x0C: // Inquire charger type
         TxData[4] = 0x04; // Charger type = Quick Charge 4
         TxData[5] = 0x00;
         TxData[6] = 0x00;
         TxData[7] = 0x00;
         break;
      case 0x0E: // Inquire charger protocol version
         TxData[4] = bPESta_PD2 ? 0x20 : 0x30; // Charger protocol version
         TxData[5] = 0x00;
         TxData[6] = 0x00;
         TxData[7] = 0x00;
         break;
      }
      break;
#endif // CONFIG_QC4
    case PE_RESP_VDM_Send_Identity:
        InitDiscoverId(0x41, (u8NumVDO + 1) << 4); // plus VDM Header
        MEM_COPY_C2X(&(TxData[4]), lpVdmTable, u8NumVDO * 4);
        /* Gary add at 180515 (fixed bug: TD.PD.VDDI3.E3 VDM Identity for Ellisys. When suport VDM response, Product Types (DFP) need to be defined as power Brick) */
        /* Gary add at 180607 (fixed bug: modify VDO ID B25:B16 to Reserved in PD2) */
        if (!bPESta_PD2)
        {
            TxData[6] = 0x80;
            TxData[7] = 0x01;
        }
        break;
   case PE_RESP_VDM_Get_Identity_NAK:
      InitDiscoverId(0x81,0x10);
      break;
   case PE_INIT_PORT_VDM_Identity_Request:
      InitDiscoverId(0x01,0x10); // initiate Discover Identity
      break;
   case PE_DRS_Send_Swap:
      TxHdrL |= 0x09; // DR_Swap
      break;
   }
#ifndef CONFIG_CAN1110C0
   }
#endif
   bPhyRetry = 0;
   bEventPHY = 1;
   u8StatePrlTx = (TXCTL&0x30)
      ? PRL_Tx_Wait_for_PHY_Response_TxDone
      : PRL_Tx_Wait_for_PHY_Done; // no bEventPE
}

void Go_PRL_Tx_Check_Retry_Counter ()
{
   BYTE RetryLimit;
   u8StatePrlTx = PRL_Tx_Check_Retry_Counter;
   RetryLimit = bPESta_PD2 ? nRetryCount : nRetryCount-1;
   if (u8RetryCounter >= RetryLimit) // || bDevSta_CBL)
   {
      u8StatePrlTx = PRL_Tx_Transmission_Error;
      u8MessageIDCounter++, u8MessageIDCounter &= 0x07;
      bEventPE = 1;
      u8StatePrlTx = PRL_Tx_Wait_for_Message_Request;
   }
   else
   {
//    Go_PRL_Tx_Construct_Message(); // takes long!!
      bPhyRetry = 1; // for re-sending in tReceive
      bEventPHY = 1;
      u8RetryCounter++;
      u8StatePrlTx = PRL_Tx_Wait_for_PHY_Response_TxDone;
   }
   bPrlSent = 0; // not sent (yet)
}

void ProtocolTxProc ()
{
    switch (u8StatePrlTx)
    {
    case PRL_Tx_Wait_for_Message_Request:
        u8RetryCounter = 0;
        bPrlSent = 0; // not sent (yet)
        
        if (u8StatePE == PE_SRC_Send_Soft_Reset)
        {
            u8StatePrlTx = PRL_Tx_Reset_for_Transmit;
            u8MessageIDCounter = 0;
            u8StoredMessageID = 0xFF;
            u8StatePrlRx = PRL_Rx_Wait_for_PHY_Message;
        }
        IsAmsPending() ?
                        bEventPE = 1
                                     : Go_PRL_Tx_Construct_Message();
        break;
        
    case PRL_Tx_Wait_for_PHY_Done: // no GoodCRC response expected (CarrierMode2/HardReset)
        if (u8StatePE == PE_BIST_Carrier_Mode)
        {
            if (u16BistCM2Cnt > 0) // from Tmr0
            {
                bEventPHY = 1; // to continue the BIST Carrier Mode 2 packet
            }
            else // from TxDone->PRLTX
            {
                u8StatePrlTx = PRL_Tx_Wait_for_Message_Request;
                bEventPE = 1;
            }
        }
        else // from TxDone->PRLTX
        {
            u8StatePrlTx = PRL_Tx_Wait_for_Message_Request;
        }
        break;
        
    case PRL_Tx_Wait_for_PHY_Response_TxDone:
        bTmr0PM = 0;
        bTmr0TO = 0;
        STRT_TMR0(T_RECEIVE);
        u8StatePrlTx = PRL_Tx_Wait_for_PHY_Response;
        break;
        
    case PRL_Tx_Wait_for_PHY_Response:
        if (bTmr0TO || !bPhySent)
        {
            Go_PRL_Tx_Check_Retry_Counter();
        }
        else // GoodCRC received
        {
            u8StatePrlTx = PRL_Tx_Match_MessageID;
            if (((PRLRXH>>1)&0x07)==u8MessageIDCounter) // MessageID matched
            {
                u8StatePrlTx = PRL_Tx_Message_Sent;
                u8MessageIDCounter++, u8MessageIDCounter &= 0x07;
                bEventPE = 1;
                bPrlSent = 1;
                bPrlRcvd = 0;
                u8StatePrlTx = PRL_Tx_Wait_for_Message_Request;
            }
            else
            {
                Go_PRL_Tx_Check_Retry_Counter();
            }
        }
        break;
    }
    bEventPRLTX = 0;
}

void ProtocolRxProc ()
{
   BYTE tmp;
   
   switch (u8StatePrlRx)
   {
   case PRL_Rx_Wait_for_PHY_Message:
      if ((PRLS&0x70) == 0x20 && u8StatePE != PE_SRC_VDM_Identity_Request) // SOP' in wrong states
      {
         break;
      }
      
      if ((RxHdr&0xF01F)==13)
      {
         u8StatePrlRx = PRL_Rx_Layer_Reset_for_Receive;
         u8MessageIDCounter = 0;
         u8StoredMessageID = 0xFF;
      }
   case PRL_Rx_Layer_Reset_for_Receive:
      u8StatePrlRx = PRL_Rx_Send_GoodCRC;
   case PRL_Rx_Send_GoodCRC:
      u8StatePrlRx = PRL_Rx_Check_MessageID;
   case PRL_Rx_Check_MessageID:
      tmp = (RxHdr>>9) & 0x07;
      if (u8StoredMessageID == tmp) // rcvd a retried message
      {
         u8StatePrlRx = PRL_Rx_Wait_for_PHY_Message;
         break;
      }
      else
      {
         u8StatePrlRx = PRL_Rx_Store_MessageID;
      }
   case PRL_Rx_Store_MessageID:
//    if (!((RxHdr&0xF01F)==5 && (PRLS&0x70)==0x10)) // not Ping
      {
         u8StoredMessageID = tmp;
         u8StatePrlTx = PRL_Tx_Wait_for_Message_Request; // PRL_Tx_Discard_Message
         u8StatePrlRx = PRL_Rx_Wait_for_PHY_Message;
         bPrlRcvd = 1;
         bPrlSent = 0;
         bEventPE = 1;
      }
      break;
   }
   bEventPRLRX = 0;
}
