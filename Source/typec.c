
#include "global.h"

#define TYPEC_RP_CURR_80        0
#define TYPEC_RP_CURR_180       1
#define TYPEC_RP_CURR_330       2

volatile BYTE u8CcStaPri;
volatile BYTE bdata u8CcStaCur _at_ 0x21;
sbit bCc1Sta0     = u8CcStaCur^0;
sbit bCc1Sta1     = u8CcStaCur^1;
sbit bCc1Sta2     = u8CcStaCur^2;
sbit bCc2Sta0     = u8CcStaCur^4;
sbit bCc2Sta1     = u8CcStaCur^5;
sbit bCc2Sta2     = u8CcStaCur^6;

volatile BYTE bdata u8TypeCBits _at_ 0x22;
sbit bTCAttch     = u8TypeCBits^0;
sbit bTCSleep     = u8TypeCBits^1;
sbit bTCDbnc      = u8TypeCBits^2;
sbit bCableCap    = u8TypeCBits^3;

volatile WORD u16TCTimer _at_ 0x1E; // 0x1F, for sleep timer

volatile BYTE u8CcThresholdHi;
volatile BYTE u8CcThresholdLo;

BYTE u8TypeC_PWR_I;

/* Gary modify at 180608 (fixed bug: TDA 2.1.3.1 BMC Termination Impedance Test (Fixed)) */
#define IS_TO_DETACH() (IS_FLIP() ? bCc2Sta2 : bCc1Sta2)
#define IS_TO_ATTACH() (bCc2Sta1 ^ bCc1Sta1)

void SetSnkTxOK(void)
{
   SET_RP_VAL(2);
   u8CcThresholdHi = DAC_2V6 >> 2;
   u8CcThresholdLo = DAC_0V8 >> 2;
}

void SetSnkTxNG(void)
{
   SET_RP_VAL(1);
   u8CcThresholdHi = DAC_1V6 >> 2;
   u8CcThresholdLo = DAC_0V4 >> 2;
}

void SetSourceAdvertisement_Default(void)
{
    SET_RP_VAL(0);
    u8CcThresholdHi = DAC_1V6 >> 2;
    u8CcThresholdLo = DAC_0V2 >> 2;
}

void TypeCResetRp (void)
{
    BYTE RpSel = GET_OPTION_RP();

    if (TYPEC_RP_CURR_330 == RpSel)
    {
        SetSnkTxOK();
        u8TypeC_PWR_I = 60;
    }
    else if (TYPEC_RP_CURR_180 == RpSel)
    {
        SetSnkTxNG();
        u8TypeC_PWR_I = 30;
    }
    else
    {
        SetSourceAdvertisement_Default();
        u8TypeC_PWR_I = 10;
    }
}

BYTE CcVol2CcSta (BYTE CcVol)
{
    return
        (CcVol >= u8CcThresholdHi) ? 0x04 :
                                          (CcVol >= u8CcThresholdLo) ? 0x02 : 0x01;
}

void UpdCcInDetached ()
{
   u8CcStaCur = CcVol2CcSta(DACV6) // CC1
                | CcVol2CcSta(DACV7) << 4; // CC2
}

void TypeCCcDetectSleep () // for timer1 ISR in SLEEP
{
    u8CcStaPri = u8CcStaCur; // save the prior status of CC1/2
    UpdCcInDetached();
   
    if (u8CcStaCur != u8CcStaPri) // CC changed
    {
        bEventTC = 1;
    }
}

void TypeCCcDetectTmr1(void)
{
    u8CcStaPri = u8CcStaCur; // save the prior status of CC1/2
    if (bTCAttch)
    {
        u8CcStaCur = (IS_FLIP())
                        ? (u8CcStaCur & 0x0F) | ((REVID & 0x80) ? CcVol2CcSta(DACV7) << 4 : 0x20)  // update CC2
                        : (u8CcStaCur & 0xF0) | ((REVID & 0x80) ? CcVol2CcSta(DACV6)    : 0x02); // update CC1
    }
    else
    {
        UpdCcInDetached();
    }

    if (u16TCTimer && !--u16TCTimer // timeout
        || u8CcStaCur != u8CcStaPri) // CC changed
    {
        bEventTC = 1;
    }
}

void TypeCCcDetectProc ()
{
    if (u8CcStaCur != u8CcStaPri)
    {
        if (bTCSleep && IS_TO_ATTACH()) // wake-up when one Rd detected in SLEEP
        {
            STOP_TMR1();
            STOP_DAC1COMP();
            SET_OSC_FAST();
            SET_ANALOG_AWAKE();
            SET_DAC1COMP_SLOW();
            START_DAC1COMP();
            STRT_TMR1(TIMEOUT_TMR1);
            bTCSleep = 0;
        }
      
        if (!bTCSleep) // start debounce timer if awake or awaked
        {
            if (bTCAttch)
            {
                u16TCTimer = T_PD_DEBOUNCE; // re-start PD debounce
            }
            else
            {
                u16TCTimer = T_CC_DEBOUNCE; // re-start CC debounce
                bTCDbnc = 1;
            }
        }
    } /* from CC changed (higher priority) */
    else
    {
        if (bTCAttch)
        {
            if  (IS_TO_DETACH())
            {
                bTCAttch = 0;
                TypeCResetRp(); // restore Rp for attach/detach detection
                ENABLE_BOTH_CC_CHANNELS();
                if (IS_OPTION_SLPEN())
                {
                    u16TCTimer = T_CC_SLEEP; // start sleep timer if becomes detached
                }
            }
        } /* Attach State */
        else if (bTCDbnc)
        {
            bTCDbnc = 0;
            if (IS_TO_ATTACH())
            {
                bTCAttch = 1;
#ifdef CONFIG_SUPPORT_EMARKER
                bDevSta_CBL = bCc1Sta0 | bCc2Sta0;
                bCableCap = bDevSta_CBL;
#endif
                (bCc1Sta1) ? NON_FLIP() : SET_FLIP();
                STOP_SENSE_VCONN_CHANNEL();
            }
            else if (IS_OPTION_SLPEN())
            {
                u16TCTimer = T_CC_SLEEP; // start sleep timer if stay detached
            }
        } /* Attach debunce */
        else
        {
            bTCSleep = 1;
            STOP_TMR1();
            STOP_DAC1COMP();
            SET_DAC1COMP_FAST();
            SET_ANALOG_SLEEP();
            SET_OSC_SLOW();
            START_DAC1COMP();
            STRT_TMR1(TIMEOUT_TMR1_SLP);
        } /* Sleep */
    } /* from timeout */
    bEventTC = 0;
}