
#include "global.h"

// =============================================================================
// global variablies
PE_STATE_BYTE u8StatePE _at_ 0x15;

#define PE_STATUS_INIT 0x03 // DFP, VCONN source
volatile BYTE bdata u8PEStaBits _at_ 0x26;
sbit bPESta_DFP   = u8PEStaBits^0; // UFP/DFP, changed at attaching, DR_Swap
sbit bPESta_VCSRC = u8PEStaBits^1; // VCONN source, changed at attaching, VCONN_Swap
sbit bPESta_CONN  = u8PEStaBits^2; // PD connected
sbit bPESta_EXPL  = u8PEStaBits^3; // Explicit Contract
sbit bPESta_TD    = u8PEStaBits^4; // Test Data BIST
sbit bPESta_RcvHR = u8PEStaBits^5; // Hard Reset received
sbit bPESta_PD2   = u8PEStaBits^6; // back to PD2.0

volatile WORD u16PETimer _at_ 0x1C; // 0x1D
volatile BYTE u8PETimer  _at_ 0x19;

BYTE u8CurrentVal; // Detect Current

extern bit bDevSta_PPSReq;

// =============================================================================
// AMS (send a command and then to wait for responded)
typedef enum {
   AMS_CMD_SENDING,
   AMS_RSP_RCVING,
   AMS_RSP_RCVD,
   AMS_CRC_TIMEOUT,
   AMS_SND_TIMEOUT,
   AMS_PENDING,
   AMS_SINK_TX_TIME
}AMS_STATE_BYTE;

AMS_STATE_BYTE u8PEAmsState _at_ 0x12;

// PE_AMS_Discover_Id()
// PE_AMS_Source_Cap()
// PE_AMS_Send_Soft_Reset()
// PE_DRS_Send_Swap()
// PE_INIT_PORT_VDM_Identity_Request()
TRUE_FALSE AMS_Cmd_n_Respond(void)
{
    switch (u8PEAmsState)
    {
    case AMS_PENDING:
        u8PEAmsState = AMS_SINK_TX_TIME;
        break;
    case AMS_SINK_TX_TIME:
        u8PEAmsState = AMS_CMD_SENDING;
        bEventPRLTX = 1;
        break;
    case AMS_CMD_SENDING:
        if (bPrlSent)
        {
            if (u8StatePE == PE_SRC_Send_Source_Alert)
            {
                return TRUE; // to end this AMS
            }
            else
            {
                u8PEAmsState = AMS_RSP_RCVING;
                u8PETimer = T_SENDER_RESPONSE; // start SenderResponseTimer
            }
        }
        else
        {
            u8PEAmsState = AMS_CRC_TIMEOUT;
            return TRUE; // to end this AMS
        }
        break;
    case AMS_RSP_RCVING:
        u8PETimer = 0; // stop SenderResponseTimer
        u8PEAmsState = (bPrlRcvd) ? AMS_RSP_RCVD : AMS_SND_TIMEOUT; // SenderResponseTimer timeout
        return TRUE; // to end this AMS
    }
    return FALSE; // to continie this AMS
}

TRUE_FALSE IsAmsPending ()
{
   return u8PEAmsState==AMS_PENDING;
}

void AMS_Start(void)
{
    u8PEAmsState = AMS_PENDING;
    /* Gary modify at 180424 (fixed bug: TD.4.9.2 USB Type-C Current Advertisement for Ellisys) */
    if (bPESta_EXPL)
    {
        SetSnkTxNG();
        u8PETimer = T_SINK_TX;
    }
    else
    {
        SetSnkTxOK();
        u8PETimer = 1; // To trigger the status
    }
}

void Do_PE_INIT_PORT_VDM_Identity_Request ()
{
   u8StatePE = PE_INIT_PORT_VDM_Identity_Request;
   u8PEAmsState = AMS_CMD_SENDING;
   bEventPRLTX = 1;
}

void Do_PE_DRS_Send_Swap ()
{
   u8StatePE = PE_DRS_Send_Swap;
   u8PEAmsState = AMS_CMD_SENDING;
   bEventPRLTX = 1;
}

void Do_PE_DRS_Change ()
{
   bPESta_DFP = !bPESta_DFP;
   (bPESta_DFP)
      ? SET_RCV_SOPP()
      : SET_DONT_RCV_SOPP();
}

BYTE u8HardResetCounter;
#ifdef CONFIG_SRC_CAP_COUNT
BYTE u8CapsCounter;
#define RESET_CAPS_COUNTER() (u8CapsCounter = 0)
#define INC_CAPS_COUNTER() if (!bPESta_EXPL) u8CapsCounter++
#else
#define RESET_CAPS_COUNTER()
#define INC_CAPS_COUNTER()
#endif
// =============================================================================
// state machine
void Go_PE_SRC_Send_Capabilities ()
{
#ifdef CONFIG_SUPPORT_QC
    if (bQcSta_InQC)
    {
        u8StatePE = PE_SRC_Disabled;
        RESET_AUTO_GOODCRC();
    }
    else
#endif
    {
        (u8StatePE == PE_SRC_Send_Soft_Reset && !bPESta_PD2) ?
                                                             AMS_Start() : (u8PEAmsState =  AMS_CMD_SENDING);
        u8StatePE = PE_SRC_Send_Capabilities;
        INC_CAPS_COUNTER();
        bEventPRLTX = 1;
    }
}

void Go_PE_SRC_Negotiate_Capability(void)
{
   u8StatePE = IsRdoAgreed() ?  PE_SRC_Transition_Accept /* to Accept */
                                                         : PE_SRC_Capability_Response; // to Reject

   bEventPRLTX = 1;
#ifdef CONFIG_PPS
   u16PETimer = bDevSta_PPSReq ? T_PPS_TIMEOUT : 0; // stop/(re-)start SourcePPSCommTimer
#endif
}

void Go_PE_SRC_Hard_Reset ()
{
   u8StatePE = PE_SRC_Hard_Reset;
   u8HardResetCounter++;
   bEventPRLTX = 1;
   u8PETimer = T_PS_HARD_RESET;
   PhysicalLayerReset();
}

void Go_PE_SRC_Send_Soft_Reset ()
{
   u8StatePE = PE_SRC_Send_Soft_Reset;
   u8PEAmsState = AMS_CMD_SENDING;
   bEventPRLTX = 1;
}

void Go_PE_SRC_Error_Recovery ()
{
   PolicyEngineReset();
   bEventPE = 0; // if PE decide to issue Source_Cap/DiscoverID, cancel it
   u8StatePE = PE_SRC_Error_Recovery;
}

void PolicyEngineReset ()
{
   u8PETimer = 0;
   u16PETimer = 0;
   u8StatePE = PE_SRC_Startup;
   u8PEAmsState = AMS_CMD_SENDING;
   u8PEStaBits = PE_STATUS_INIT;
#ifdef CONFIG_TS_DYNAMIC_PDP
    bTsUpdateCurrentEvent = 0;
#endif
}

void PolicyEngineTmr1()
{
    if (u8PETimer  && !--u8PETimer)
        bEventPE = 1;

    if (u16PETimer && !--u16PETimer)
        bEventPE = 1;
}

void PolicyEngineProc ()
{
    if (bDevSta_SndHR)
    {
        Go_PE_SRC_Hard_Reset();
        bDevSta_SndHR = 0;
    }
    else if (bPESta_RcvHR)
    {
        u8StatePE = PE_SRC_Hard_Reset_Received;
        u8PETimer = T_PS_HARD_RESET;
        u16PETimer = 0;
        PhysicalLayerReset();
        STOP_TMR0();
        bPESta_RcvHR = 0;
    }
    else
    {
        switch (u8StatePE)
        {
        case PE_SRC_Startup:
            u8PEStaBits = PE_STATUS_INIT;
            /* Gary modify at 180611 (fixed bug: PE state mechanism need check type c  attached on PE_SRC_Startup) */
            if (bTCAttch)
            {
                if (AMS_PENDING == u8PEAmsState)
                {
                    PhysicalLayerStartup();
                    PRL_Tx_PHY_Layer_Reset();
                    RESET_CAPS_COUNTER();
#ifdef CONFIG_SUPPORT_EMARKER
                    if (bDevSta_CBL)
                    {
                        u8StatePE = PE_SRC_VDM_Identity_Request;
                        u8PEAmsState = AMS_CMD_SENDING;
                        bEventPRLTX = 1;
                    }
                    else
#endif
                    {
                        Go_PE_SRC_Send_Capabilities();
                    }
                }
                else
                {
                    AMS_Start();
                }
            }
            break;
        case PE_SRC_Send_Capabilities:
            if (AMS_Cmd_n_Respond())
            {
                switch (u8PEAmsState)
                {
                case AMS_RSP_RCVD:
                   if (!bPESta_EXPL && (RxHdr&0x00C0)==0x0040) // SpecRev=PD2.0 in this any received message
                   {
                      bPESta_PD2 = 1;
                      /* Gary Add at 180517 (fixed bug: When PD3 transform to PD2, the PDO type is only Fixed) */
                      u8NumSrcPdo = u8NumSrcFixedPdo;
                      TypeCResetRp();
                   }
                   ((RxHdr&0xF01F)==0x1002) // Request
                                            ? Go_PE_SRC_Negotiate_Capability() : Go_PE_SRC_Send_Soft_Reset();
                   break;
                case AMS_SND_TIMEOUT: // SenderResponseTimer timeout
                   Go_PE_SRC_Hard_Reset();
                   break;
                case AMS_CRC_TIMEOUT:
#ifdef CONFIG_SRC_CAP_COUNT
                   if (PE_SRC_CAP_COUNT <= u8CapsCounter)
                   {
                      u8StatePE = PE_SRC_Disabled;
                      RESET_AUTO_GOODCRC(); // so that PRLRX won't trigger bEventPE again
                   }
                   else
#endif
                   if (bPESta_EXPL)
                   {
                      Go_PE_SRC_Send_Soft_Reset();
                   }
                   else
                   {
                      u8StatePE = PE_SRC_Discovery;
                      u8PETimer = T_C_SENDSRCCAP;
#ifdef CONFIG_TS_DYNAMIC_PDP
                        if (bTsUpdateCurrentEvent)
                        {
                            UpdatePdoCurrent();
                            bTsUpdateCurrentEvent = 0;
                        }
#endif
                   }
                   break;
                }
            }
            else // GoodCRC received
            {
                RESET_CAPS_COUNTER();
                u8HardResetCounter = 0;
                bPESta_CONN = 1;
                u16PETimer = 0; // stop NoResponseTimer
            }
            break;
        case PE_SRC_Discovery:
        case PE_SRC_Wait_New_Capabilities:
            Go_PE_SRC_Send_Capabilities();
            break;
        case PE_BIST_Carrier_Mode: // PD_R30V11_6.6.7.2 BISTContModeTimer,
            u8StatePE = PE_SRC_Ready; // return to normal operation
            break;
        case PE_SRC_Capability_Response:
            if (bPESta_EXPL)
            {
                u8StatePE = PE_SRC_Ready;
            }
            else
            {
                u8StatePE = PE_SRC_Wait_New_Capabilities;
                bEventPM = 1;
            }
            break;
        case PE_SRC_Transition_Accept: // sending Accept
            if (bPrlSent) // Accept sent
            {
                u8StatePE = PE_SRC_Transition_Supply;
                bEventPM = 1;
            }
            else // CRCReceiveTimer timeout, interrputed, busy
            {
                /* Gary add at 180609 (fixed bug: A failure to see a GoodCRC Message in response to any Message within tReceive (after nRetryCount retries),
                                                  when a Port Pair is Connected, is indicative of a communications failure resulting in a Soft Reset) */
                // Go_PE_SRC_Hard_Reset();
                Go_PE_SRC_Send_Soft_Reset();
            }
            break;
        case PE_SRC_Transition_Supply: // wait for transition
            if (bPrlRcvd) // interrputed
            {
                Go_PE_SRC_Hard_Reset();
            }
            else
            {
                u8StatePE = PE_SRC_Transition_PS_RDY;
                bEventPRLTX = 1;
            }
            break;
        case PE_SRC_Transition_PS_RDY: // sending PS_RDY
            if (bPrlSent) // PS_RDY sent
            {
                u8StatePE = PE_SRC_Ready;
                bEventPM = 1;
                bPESta_EXPL = 1;
#ifdef CONFIG_PPS
                bDevSta_PPSRdy = bDevSta_PPSReq;
#endif
            }
            else // CRCReceiveTimer timeout, interrputed, busy
            {
                Go_PE_SRC_Hard_Reset();
            }
            break;
        case PE_VCS_Accept_Swap:
            if (bPrlSent) // Accept sent
            {
                if (bPESta_VCSRC)
                {
                    u8StatePE = PE_VCS_Wait_for_VCONN;
                    u8PETimer = T_VCONN_SOURCE_ON; // start VCONNOnTimer
                }
                else
                {
                    u8StatePE = PE_VCS_Turn_On_VCONN;
                    bEventPM = 1;
                }
            }
            else // CRCReceiveTimer timeout, interrputed, busy
            {
                Go_PE_SRC_Send_Soft_Reset();
            }
            break;

        case PE_VCS_Turn_On_VCONN:
            u8StatePE = PE_VCS_Send_PS_RDY;
            bEventPRLTX = 1;
            bPESta_VCSRC = 1; // becomes VCONN source
            SET_RCV_SOPP();
            break;

        case PE_VCS_Wait_for_VCONN:
            u8PETimer = 0; // stop VCONNOnTimer
            if (bPrlRcvd && (RxHdr&0xF01F)==6) // PS_RDY
            {
                u8StatePE = PE_SRC_Ready;
                SET_VCONN_OFF();
                bPESta_VCSRC = 0; // becomes not VCONN source
                SET_DONT_RCV_SOPP();
            }
            else // VCONNOnTimer timeout or interrupted
            {
                Go_PE_SRC_Hard_Reset();
            }
            break;

        case PE_SRC_Send_Soft_Reset:
            if (AMS_Cmd_n_Respond())
            {
                switch (u8PEAmsState)
                {
                case AMS_RSP_RCVD:
                    ((RxHdr&0xF01F)==3) /* Accept */ ? Go_PE_SRC_Send_Capabilities()
                                                                                    : Go_PE_SRC_Hard_Reset();
                   break;
                case AMS_CRC_TIMEOUT:
                case AMS_SND_TIMEOUT: // SenderResponseTimer timeout
                   Go_PE_SRC_Hard_Reset();
                   break;
                }
            }
            break;

        case PE_SRC_VDM_Identity_Request:
            if (AMS_Cmd_n_Respond())
            {
                PolicyEngineReset();
                bEventPM = 1;
            }
            break;

        case PE_DRS_Accept_Swap: // don't response GoodCRC to SOP' as a UFP, (LeCroy USBIFWS#106)
            if (bPrlSent)
            {
                u8StatePE = PE_SRC_Ready;
                Do_PE_DRS_Change ();
            }
            else
            {
                Go_PE_SRC_Send_Soft_Reset();
            }
            break;

        case PE_SRC_Disabled:
            if (u8HardResetCounter <= nHardResetCount)
            {
                Go_PE_SRC_Hard_Reset(); // NoResponseTimer timeout
            }
            break;

        case PE_SRC_Soft_Reset:
            (bPrlSent) ? Go_PE_SRC_Send_Capabilities()
                                                      : Go_PE_SRC_Hard_Reset();
            break;

        case PE_SRC_Hard_Reset_Received:
        case PE_SRC_Hard_Reset:
            u8StatePE = PE_SRC_Transition_to_default;
            bEventPM = 1;
            break;

        case PE_SRC_Error_Recovery:
            PolicyEngineReset();
            SET_RP_ON();
            break;

        case PE_SRC_Transition_to_default:
            PolicyEngineReset();
            u16PETimer = T_NO_RESPONSE;
            break;
        case PE_SRC_Chunk_Recieved:
            u8StatePE = PE_SRC_Send_Not_Supported;
            bEventPRLTX = 1;
            break;
        case PE_SRC_Give_Source_Cap_Ext:
        case PE_SRC_Give_Source_Status:
        case PE_SRC_Give_PPS_Status:
        case PE_SRC_Send_Not_Supported:
        case PE_SRC_Send_Reject:
        case PE_RESP_VDM_Send_Identity:
        case PE_RESP_VDM_Get_Identity_NAK:
        case PE_VCS_Send_PS_RDY:
            (bPrlSent) ? (u8StatePE = PE_SRC_Ready)
                                                    : Go_PE_SRC_Send_Soft_Reset();
            break;
        case PE_SRC_Ready:
            if (bPrlRcvd & ~bPESta_TD)
            {
                if (RxHdr & 0x8000) // Extended Messages
                {
                   if (RxData[1] & 0x80 && // chunked
                      ((((WORD)RxData[1]<<8 | RxData[0]) & 0x01FF) > 26)) // Data Size > MaxExtendedMsgLegacyLen
                   {
                      u8StatePE = PE_SRC_Chunk_Recieved;
                      u8PETimer = T_CHUNK_NOT_SUPPORTED;
                   }
                   else // non-chunked
                   {
                      u8StatePE = PE_SRC_Send_Not_Supported;
                      bEventPRLTX = 1;
                   }
                }
                else if (RxHdr & 0x7000) // Data Messages
                {
                    switch (RxHdr&0x1F)
                    {
                    case 0x02: // Request
                        Go_PE_SRC_Negotiate_Capability();
                        break;
                    case 0x03: // BIST
                        if (u8RDOPositionMinus == 0) // VBUS=Vsafe5V
                        {
                            switch (RxData[3])
                            {
                            case 0x50: // Carrier Mode 2
                                u8StatePE = PE_BIST_Carrier_Mode;
                                bEventPRLTX = 1;
                                break;
                            case 0x80: // Test Data
                                bPESta_TD = 1;
                                break;
                            }
                        }
                        break;
                    case 0x0F: // VDM
                        if ((RxData[1] & 0x80) == 0x80)
                        {
                            /* Gary add at 180515 (fixed bug: TD.PD.VDMD.E4 Applicability and TD.PD.VDDI3.E3 VDM Identity) */
                            if (u8NumVDO > 0)
                            {
                                if ((bPESta_PD2) && (bPESta_DFP))
                                    break;

                                if ((RxData[0] & 0xDF) == 0x01 // initiate DiscoverID
                                        && *((WORD*)&RxData[2]) == BIG_ENDIAN(SID_STANDARD))
                                {
                                    u8StatePE = PE_RESP_VDM_Send_Identity;
                                }
                                else
                                {
                                    u8StatePE = PE_RESP_VDM_Get_Identity_NAK;
                                }
                            } /* Support VDM */
                            else
                            {
                                if (bPESta_PD2)
                                    break;
                                else
                                    u8StatePE = PE_SRC_Send_Not_Supported;
                            } /* Not Support VDM */
                            bEventPRLTX = 1;
                        } /* structured VDM */
                        else
                        {
                        #ifdef CONFIG_QC4
                            if (!bPESta_DFP
                                && RxData[0]==3 // CMD0
                                && (RxData[1]==0x06 || RxData[1]==0x10 ||
                                RxData[1]==0x0B || RxData[1]==0x0C || RxData[1]==0x0E) // CMD1
                                && *((WORD*)&RxData[2])==BIG_ENDIAN(VID_QUALCOMM_QC40))
                            {
                                u8StatePE = PE_RESP_VDM_Send_QC4_Plus;
                                bEventPRLTX = 1;
                            }
                            else
                        #endif // CONFIG_QC4
                            if (!bPESta_PD2) // ignore if PD20
                            {
                                u8StatePE = PE_SRC_Send_Not_Supported;
                                bEventPRLTX = 1;
                            }
                        } /* unstructured VDM */
                        break;
                    case 0x05: // Battery_Status
                    case 0x06: // Alert
                    case 0x07: // Get_Country_Info
                        if (!bPESta_PD2)
                        {
                            u8StatePE = PE_SRC_Send_Not_Supported;
                            bEventPRLTX = 1;
                        }
                        break;
                //             case 0x01: // Source_Capabilities
                //             case 0x04: // Sink_Capabilities
                    default:
                        u8StatePE = PE_SRC_Send_Not_Supported; // or Reject in updprl.c
                        bEventPRLTX = 1;
                         break;
                    }
                }
                else // Control Messages
                {
                   switch (RxHdr&0x1F)
                   {
                   case 0x07: // Get_Source_Cap
                      Go_PE_SRC_Send_Capabilities();
                      break;
                   case 0x09: // DR_Swap
                      u8StatePE = PE_DRS_Accept_Swap;
                      bEventPRLTX = 1;
                      break;
                   case 0x0B: // VCONN_Swap
                      u8StatePE = PE_VCS_Accept_Swap;
                      bEventPRLTX = 1;
                      break;
                   case 0x0D: // Soft Reset
                      u8StatePE = PE_SRC_Soft_Reset;
                      bEventPRLTX = 1;
                      break;
                //             case 0x01: // GoodCRC
                   case 0x03: // Accept
                   case 0x04: // Reject
                   case 0x06: // PS_RDY
                      Go_PE_SRC_Send_Soft_Reset(); // un-expected
                      break;
#ifdef CONFIG_PPS
                   case 0x14: // Get_PPS_Status
                      if (!bPESta_PD2)
                      {
                            /*
                                Gary modify at 180616 (modify When fixed/Augmented all the send PE_SRC_Give_PPS_Status message
                                u8StatePE = bDevSta_PPSReq
                                                            ? PE_SRC_Give_PPS_Status
                                                            : PE_SRC_Send_Not_Supported;
                            */
                            u8StatePE = PE_SRC_Give_PPS_Status;
                            bEventPRLTX = 1;
                        }
                      break;
#endif
                   case 0x12: // Get_Status
                      if (!bPESta_PD2)
                      {
                         u8StatePE = PE_SRC_Give_Source_Status;
                         bEventPRLTX = 1;
                      }
                      break;
                   case 0x11: // Get_Source_Cap_Extended
                      if (!bPESta_PD2)
                      {
                         u8StatePE = (u8NumVDO>0)
                            ? PE_SRC_Give_Source_Cap_Ext
                            : PE_SRC_Send_Not_Supported;
                         bEventPRLTX = 1;
                      }
                      break;
                   case 0x13: // FR_Swap
                   case 0x15: // Get_Country_Codes
                      if (!bPESta_PD2)
                      {
                         u8StatePE = PE_SRC_Send_Not_Supported;
                         bEventPRLTX = 1;
                      }
                      break;
                //             case 0x02: // GotoMin
                //             case 0x05: // Ping
                //             case 0x08: // Get_Sink_Cap
                //             case 0x0A: // PR_Swap
                //             case 0x0C: // Wait
                   default:
                      u8StatePE = PE_SRC_Send_Not_Supported; // or Reject in updprl.c
                      bEventPRLTX = 1;
                      break;
                   }
                }
            }
#ifdef CONFIG_PPS
            else if (bDevSta_PPSReq) // SourcePPSCommTimer timeout
            {
                Go_PE_SRC_Hard_Reset();
            }
#endif
            break;
        case PE_DRS_Send_Swap:
            if (AMS_Cmd_n_Respond())
            {
                switch (u8PEAmsState)
                {
                case AMS_RSP_RCVD:
                    switch (RxHdr&0xF01F)
                    {
                    case 3: // Accept
                        Do_PE_DRS_Change();
                    case 4: // Reject
                        u8StatePE = PE_SRC_Ready;
                        bEventPM = 1;
                        break;
                    default:
                        Go_PE_SRC_Send_Soft_Reset();
                        break;
                    }
                    break;
                case AMS_CRC_TIMEOUT:
                case AMS_SND_TIMEOUT: // SenderResponseTimer timeout
                   Go_PE_SRC_Send_Soft_Reset();
                   break;
                }
            }
            break;
        case PE_INIT_PORT_VDM_Identity_Request:
            if (AMS_Cmd_n_Respond())
            {
                u8StatePE = PE_SRC_Ready;
                bEventPM = 1;
            }
            break;
        case PE_SRC_Send_Source_Alert:
            if (AMS_Cmd_n_Respond())
            {
                (bPrlSent)
                        ? (u8StatePE = PE_SRC_Ready)
                                    : Go_PE_SRC_Send_Soft_Reset();
                SetSnkTxOK();
            }
            break;
        case PE_RESP_VDM_Send_QC4_Plus:
            u8StatePE = PE_SRC_Ready;
            break;
        }
    }
    bEventPE = 0;
}