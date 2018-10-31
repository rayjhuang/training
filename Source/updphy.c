
#include "global.h"

BYTE xdata TxData [30] _at_ 0x000;
BYTE xdata RxData [30] _at_ 0x020;
BYTE TxHdrL _at_ 0x0E, TxHdrH _at_ 0x0F;
WORD RxHdr  _at_ 0x10; // 0x11

WORD u16BistCM2Cnt;

void IsrTx() interrupt INT_VECT_UPDTX
{
    if ((STA1 & 0x40) // auto-TX-GoodCRC sent
#ifndef CONFIG_CAN1110C0 // patch auto-TX-GoodCRC
         || bPhyPatch // like auto-TX-GoodCRC sent
#endif
    )
    {
        bPhyPatch = 0;
        bEventPRLRX = 1;
    }
    else if (STA1 & 0x01) // TxDone (and is not of auto-TX-GoodCRC)
    {
        GO_SENSE_CC_CHANNEL(); // if (u8StatePE == PE_BIST_Carrier_Mode)
        bEventPRLTX = 1;
    }

    if (MSK1&0x02 && STA1&0x02) // TX discarded, inform PRLTX once CC idle
    {
        MSK1 &=~0x02;
        bEventPRLTX = 1;
    }

    STA1 = 0x041;
}

void IsrRx() interrupt INT_VECT_UPDRX
{
   BYTE ptr;
   BYTE hdr_l;
   
    if (STA0 & 0x08) // a message rcvd
    {
        hdr_l = PRLRXL & 0x1F; // don't do this to PRLRXH, keep MessageID for returning GoodCRC
        if ((PRLRXH & 0xF0) == 0 && hdr_l == 1) // GoodCRC
        {
            STOP_TMR0(); // stop CRCReceiverTimer
            FFSTA = 0; // reset FIFO
            bEventPRLTX = 1;
        }
        else
        {
            ptr = 0;
            STA1 = 0x30;
            FFCTL = 0x40; // first
            ((BYTE*)&RxHdr)[1] = FFIO;
            ((BYTE*)&RxHdr)[0] = FFIO;
            while (!(STA1&0x30) && ptr<30)
            {
                RxData[ptr++] = FFIO;
            }
#ifndef CONFIG_CAN1110C0 // patch auto-TX-GoodCRC
            if ((PRLRXH & 0xF0) == 0 && hdr_l == 0x11) // Get_Source_Cap_Extended
            {
                bEventPRLTX = 1;
                bPhyPatch = 1;
            }
#endif
        }
    }
    
    if ((STA0 & 0x02) && ((PRLS & 0x70) == 0x60)) // Hard Reset rcvd
    {
        bEventPE = 1;
        bPESta_RcvHR = 1;
    }
    STA0 = 0x0A;
}

void PhysicalLayerStartup ()
{
   DEC = 0xC7;
   RXCTL = 0x23; // Hard Reset, SOP, SOP'
   PRLTX = 0xD7; // auto-TX-GoodCRC, auto-discard, spec=2.0, MsgID=7
   STA0 = 0xFF;
   STA1 = 0xFF;
   EX4 = 1; // UPDRX
   EX5 = 1; // UPDTX
}

void PhysicalLayerReset ()
{
   RXCTL = 0x01; // SOP (for Canyon Mode 0, but, is that right?)
   PRLTX = 0x07; // POR value
   CCRX = 0x48; // SQL_EN, RX_PK_EN=0, ADPRX_EN
   EX4 = 0; // UPDRX
   EX5 = 0; // UPDTX
   MSK0 = 0x0A; // EOP with CRC32 OK, enabled-ordered-set rcvd
   MSK1 = 0x41; // auto-TX-GoodCRC sent, TX done
}

void PhysicalLayerProc ()
{
   BYTE bcnt, ii;
   if (TXCTL==0x40) // BIST Carrier Mode 2
   {
      if (!bTmr0TO)
      {
         STOP_SENSE_CC_CHANNEL(); // stop sensing the CC lane for better eye diagram
         STA1  = 0x30;
         FFSTA = 0x00; // reset FIFO
         FFCTL = 0x40; // set first
         u16BistCM2Cnt = N_BISTCM2 + 1;
      }
      while (--u16BistCM2Cnt && (FFSTA&0x3F)<34) FFIO = 0x44;
      if (!bTmr0TO) FFCTL = 0xBF; // set last/un-lock/numk=0x1F
      FFIO = 0x44;
      if (u16BistCM2Cnt) STRT_TMR0(T_BISTCM2); // inform PrlTx later (must before FIFO empty)
   }
   else
   {
      if (TXCTL==0x48) // Hard Reset
      {
         if (!(REVID&0x80))
         {
            for (ii=0; ii<20; ii++) // wait about 20us
               if (!(REVID&0x80)) ii = 0; // STUCKED if HR cannot be sent??
         }
         STA1  = 0x30;
         FFSTA = 0x00; // reset FIFO
         FFCTL = 0x40; // set first
         FFIO  = 0x55;
         FFCTL = 0x82; // set last, 2-byte ex-code
         FFIO  = 0x65;
      }
      else // SOP/SOP'
      {
         bcnt = ((TxHdrH & 0x70) >> 2); // NDO*4
         bPhySent = (REVID&0x80); // CC idle
         if (bPhySent)
         {
            STA1  = 0x30;
            FFSTA = 0x00; // reset FIFO
            FFCTL = 0x40; // first
            FFIO = TxHdrL;
            if (bPhyRetry || // for re-sending in tReceive
                  bcnt==0) FFCTL = 0xA0; /* tommy: 20180704 (Fixed 6 PDO src_cap message incomplete, turn on FFIO write for retry) */
            FFIO = TxHdrH;
            for (ii=0; ii<bcnt && !(STA1&0x20); ii++)
            {
               if (!bPhyRetry && ii==(bcnt-1)) FFCTL = 0x80; // last
               FFIO = TxData[ii];
            }
            // any error condition here??
                FFCTL &= ~0x20; /* tommy: 20180704 (Fixed 6 PDO src_cap message incomplete, turn off FFIO write for retry) */
         }
         else // CC busy
         {
            STA1 |= 0x02;
            MSK1 |= 0x02; // temporarily enable GO_IDLE INT
         }
      }
   }
   bEventPHY = 0;
}
