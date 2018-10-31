
#include "global.h"

BYTE u8NumSrcPdo;
BYTE u8NumSrcPdoTab;
BYTE u8NumSrcFixedPdo;
BYTE code* lpPdoTable;
BYTE xdata SRC_PDO[7][4] _at_ 0x040;
WORD xdata PDO_V_DAC[7]  _at_ 0x060;
BYTE xdata OPTION_REG[4] _at_ 0x070;

#define DPM_T_PPS_CL_PROGRAM_SETTLE 188

#define I_PPS_CL_1A                 20
#define OCP_3A                      60
#define OCP_1P5A                    30
#define OCP_500MA                   10

#define DPM_FIX_MAX_CURR_UNIT       10
#define DPM_VAR_MAX_CURR_UNIT       10
#define DPM_AUGMENTED_MAX_CURR_UNIT 50

#define DPM_PDO_TYPE_FIXED          0x00
#define DPM_PDO_TYPE_BATTERY        0x40
#define DPM_PDO_TYPE_VARIABLE       0x80
#define DPM_PDO_TYPE_AUGMENTED      0xC0

BYTE u8RDOPositionMinus;
BYTE u8SrcPDP; /* Gary add at 180222 (TD.PD.VNDI3.E9 Source Capabilities Extended for Ellisys) */
signed char s8TransStep;
WORD u16PresentVol;
WORD u16TargetVol; // also for Power-Good check
BYTE u8PresentCurr;
BYTE u8TargetCurr;
WORD RDO_OutVolTmp; // pass Output Voltage from IsRdoAgreed() to PWRTransCalc()
BYTE u8CurrTransStep;

BYTE u8NumVDO; // not include VDM Header
BYTE code* lpVdmTable;

volatile BYTE bdata u8PMStaBits _at_ 0x23;
sbit bDevSta_CBL     = u8PMStaBits^0; // turn-on when Ra found, turn-off if VCONN fail or cable checked
sbit bDevSta_SndHR   = u8PMStaBits^1; // to send Hard Reset
sbit bDevSta_5ACap   = u8PMStaBits^2; // device is allowed to exceed 3A
sbit bDevSta_LgStep  = u8PMStaBits^3; // PPS large step transition
sbit bDevSta_PPSReq  = u8PMStaBits^4; // prior Request is PPS one
sbit bDevSta_PPSRdy  = u8PMStaBits^5; // to change voltage stepping behavior
sbit bDevSta_OpCInc  = u8PMStaBits^7; // change OpC before voltage transition

BYTE bdata u8PMExBits _at_ 0x28;
sbit bPMExtra_DrSwp  = u8PMExBits^0;
sbit bPMExtra_PdoOpt = u8PMExBits^1;
sbit bPMExtra_Pdo2nd = u8PMExBits^2;
sbit bPMExtra_SameAPDO = u8PMExBits^3; // Gary add at 180209(fixed bug: Small step time issue)
sbit bPMExtra_RecordPDOType = u8PMExBits^4;
sbit bPMExtra_VolDone = u8PMExBits^5;
sbit bPMExtra_CurrDone = u8PMExBits^6;
sbit bPMExtra_PWRTransOK = u8PMExBits^7;

enum {
   PM_STATE_DETACHED,
   PM_STATE_DETACHING,             // 0x01, discharge VBUS
   PM_STATE_DISCHARGE,             // 0x02, discharge VIN
   PM_STATE_ATTACHING,             // 0x03
   PM_STATE_PWRGOOD,               // 0x04
   PM_STATE_ATTACHED,              // 0x05
   PM_STATE_ATTACHED_TRANS_PRE,    // 0x06
   PM_STATE_ATTACHED_TRANS,        // 0x07
   PM_STATE_ATTACHED_TRANS_PWRGD,  // 0x08
   PM_STATE_ATTACHED_TRANS_DISCHG, // 0x09
   PM_STATE_ATTACHED_TRANS_PPS,    // 0x0A
   PM_STATE_ATTACHED_TRANS_PST,    // 0x0B
   PM_STATE_ATTACHED_WAIT_CAP,     // 0x0C
   PM_STATE_ATTACHED_SEND_HR,      // 0x0D
   PM_STATE_ATTACHED_IPHONE,       // 0x0E
   PM_STATE_RECOVER                // 0x0F
} u8StatePM _at_ 0x14;

volatile WORD u16PMTimer _at_ 0x1A; // 0x1B, for tSrcRecover
volatile BYTE u8DischgTimer; // for discharge

#define APPX_DIV5(v) (v>>2)-(v>>5)-(v>>6)-(v>>8) // about 0.199

BYTE DivBy5_10b (WORD val) { BYTE tmp = val>>2; tmp -= ((tmp>>3) + (tmp>>4) + (tmp>>6)); return tmp; }
#ifdef CONFIG_PPS
WORD MulBy5     (BYTE val) { return 5*val; }
#endif
WORD GetPwrV    ()         { WORD tmp = GET_PWR_V(); return (tmp >= 512) ? (tmp - 1) : tmp; }
BYTE GetPwrI    ()         { return (GET_PWR_I() - DAC_OFS_PWR_I());}
void SetPwrV    (WORD v)   { WORD tmp = (v>=512 && v<1023) ? v+1 : v; SET_PWR_V(tmp); }
void SetPwrI    (BYTE c)   { SET_PWR_I(c + DAC_OFS_PWR_I()); }

#ifdef CONFIG_QC4
BYTE GetTemperature (TRUE_FALSE low)
// low : 0/1-degree lower bound
// only for 103AT NTC (10K@25)
{
   BYTE sav_dac_ch, sav_ts_v, temp_v;
   STOP_DAC1_LOOP();
   sav_dac_ch = DACEN;
   sav_ts_v = DACV3;
   ENABLE_TS_SARADC();

   DACEN = 0x08; // only TS channel
   START_SARADC_ONCE();
   while (IS_DAC1COMP_BUSY()); // 40us
   temp_v = DACV3;
   if (temp_v>72) // < 40 degree
   {
      temp_v = DivBy5_10b(temp_v);
      temp_v = (temp_v>=52) ? low : 52 - temp_v;
   }
   else
   {
      temp_v = 91 - (temp_v>>1) - (temp_v>>2);
   }

   DISABLE_TS_SARADC();
   DACV3 = sav_ts_v;
   DACEN = sav_dac_ch;
   START_DAC1COMP();
   return temp_v;
}
#endif

TRUE_FALSE IsPwrGood ()
{
   BYTE dacv = u16TargetVol >> 2, // in 80mV
        diff = (dacv >> 4) - (dacv >> 7); // 5.5%
   return (PWR_GOOD_DACV() >= dacv-diff &&
           PWR_GOOD_DACV() <= (WORD)dacv+diff);
}

WORD GetPDOLowWord (BYTE pos_minus)
{
    WORD tmp = *((WORD xdata*)&SRC_PDO[pos_minus][0]);
    return (tmp >> 8) | (tmp << 8);
}

BYTE GetPwrIxPdoPos ()
// get max. curr from the PDO
// convert 10mA step to 50mA step
{
    WORD tmp = GetPDOLowWord(u8RDOPositionMinus) & 0x3FF;
    BYTE pwri = (bDevSta_PPSReq)
                                ? (tmp & 0x7F) // 50mA
                                : DivBy5_10b(tmp); // get Operating Current from PDO[0] B9-B2 (drop 2 bits)

    if (!bDevSta_5ACap && pwri > APPX_DIV5(3000/10))
    {
        pwri = APPX_DIV5(3000/10);
    }

    /* Gary add at 180628 (fixed bug: Request current 0A, will sent HardReset issue) */
    if (0x00 == pwri)
        pwri = 10;
    return (pwri);
}
#ifdef CONFIG_PPS
static void CurrentSteping(void)
{
    u8PresentCurr += (bDevSta_OpCInc ? 1 : -1);
    SetPwrI(u8PresentCurr);
    u8CurrTransStep--;
    if (u8CurrTransStep == 0)
        bPMExtra_CurrDone = 1;
}
#endif
/*
    this function will be prepare following:
    1. Trans Step
    2. Trans Volt
    3. Trans Target in 20mV
    4. Large Step in PPS
*/
void PWRTransCalc (BYTE pos_minus)
{
    if (pos_minus < 7)
    {
        u8RDOPositionMinus = pos_minus;
#ifdef CONFIG_PPS
        bDevSta_PPSRdy = 0;

        if (bDevSta_PPSReq && pos_minus > 0) // pos_minus=0 must not be APDO
        {
            //u16TargetVol = RDO_OutVolTmp;
            //bDevSta_LgStep = (u16TargetVol > (u16TransVolt + 500/20))
             //                   || (u16TargetVol < (u16TransVolt - 500/20));
            /* Gary add at 180504 (fixed bug: VBUS exceeding 20V can cause overflow) */
            if (u16TargetVol > 0x3FF)
            {
                u16TargetVol = 0x3FF;
            } /* HW only Support t0 20.46V*/
        } /* Request APDO */
        else
#endif
        {
            u16TargetVol = PDO_V_DAC[u8RDOPositionMinus] & 0x3FF;
            u8TargetCurr = GetPwrIxPdoPos();
        } /* PDO Request */
    }

    u16PresentVol = GetPwrV();
    if (u16TargetVol == u16PresentVol)
    {
        s8TransStep = 0;
    }
    else if (u16TargetVol > u16PresentVol)
    {
        s8TransStep = STEP_UP;
    }
    else
        s8TransStep = STEP_DN;

    u8PresentCurr = GetPwrI();
    bDevSta_OpCInc = u8TargetCurr > u8PresentCurr;

#ifdef CONFIG_PPS
    if (bDevSta_PPSReq)
    {
        if (bDevSta_OpCInc)
            u8CurrTransStep = u8TargetCurr - u8PresentCurr;
        else
            u8CurrTransStep = u8PresentCurr - u8TargetCurr;
    }
    else
#endif
        u8CurrTransStep = 0;
}

void PWRTransStart()
{
    if (s8TransStep < 0)
    {
        /* Gary modify at 180614 (modify falling too fast issue) */
        if (u8CurrentVal < 10)
        {
            DISCHARGE_ENA();
        }
        SET_OVPINT(0);
    } /* Decrease the voltage */

    bPMExtra_PWRTransOK = 0;
    bTmr0PM = 1;
    STRT_TMR0(T_STEPPING); // start the first stepping
}

TRUE_FALSE IsRdoAgreed () // to initialize a transition
{
    WORD RDO_OpC, PDO_Type, PDO_MaxC;
    BYTE NewPosMinus = ((RxData[3] & 0x70) >> 4) - 1;

    RDO_OutVolTmp = (WORD)RxData[2] << 8 | RxData[1];

    PDO_Type = *((WORD xdata*)&SRC_PDO[NewPosMinus][2]);
    PDO_Type = (PDO_Type >> 8) | (PDO_Type << 8);
    PDO_MaxC = GetPDOLowWord(NewPosMinus);

#ifdef CONFIG_PPS
    if ((PDO_Type & 0xF000) == 0xC000) // request to a PPS APDO
    {
        RDO_OutVolTmp = (RDO_OutVolTmp >> 1) & 0x7FF; // PPS RDO[19:9], 20mV

        if (RDO_OutVolTmp > MulBy5(PDO_Type >> 1)     // APDO[24:17], max. in 100mV
            || RDO_OutVolTmp < MulBy5(PDO_MaxC >> 8)) // APDO[15:8],  min. in 100mV
        {
            return FALSE;
        }
        else
        {
            RDO_OpC = RxData[0] & 0x7F; // PPS RDO[6:0]
            PDO_MaxC = PDO_MaxC & 0x7F; // APDO[6:0], max. in 50mA
        }
    } /* APDO */
    else
#endif
    {
        RDO_OpC = (RDO_OutVolTmp >> 2) & 0x3FF;
        PDO_MaxC = PDO_MaxC & 0x3FF; // max. in 10mA
    } /* Fixed */

    if (NewPosMinus < u8NumSrcPdo && PDO_MaxC >= RDO_OpC)
    {
#ifdef CONFIG_PPS
        bPMExtra_SameAPDO = 0x00; /* Gary add at 20180209 (fixed bug: Small step time issue) */
        bDevSta_PPSReq = (PDO_Type & 0xF000) == 0xC000; // request to a PPS APDO

        /* Set operation current */
        if (bDevSta_PPSReq)
        {
            bDevSta_OpCInc = 1; // PPS always change PWR_I before voltage transition

            /* Gary add at 180213 (fixed bug: request min current is 1A in PPS mode) */
            if (RDO_OpC < I_PPS_CL_1A)
                u8TargetCurr = I_PPS_CL_1A;
            else
                u8TargetCurr = RDO_OpC;

            /* Gary add at 180209 (fixed bug: Small step time issue) */
            if (NewPosMinus == u8RDOPositionMinus)
                bPMExtra_SameAPDO = 0x01;

            u16TargetVol = RDO_OutVolTmp;
            bDevSta_LgStep = (u16TargetVol > (u16PresentVol + 500/20))
                                || (u16TargetVol < (u16PresentVol - 500/20));
        }
#endif
        PWRTransCalc(NewPosMinus);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

#ifdef CONFIG_SUPPORT_EMARKER
TRUE_FALSE IsCable5ACapable(void)
{
   return
      (bPrlRcvd && // DiscoverID responded
          ( (RxHdr&0xF01F)==0x500F // VDM with NDO=5
         && (RxData[0]&0xDF)==0x41 // DiscoverID ACK respond
         && (RxData[1]&0x80)==0x80 // structured VDM
         && *((WORD*)&RxData[2])==BIG_ENDIAN(SID_STANDARD)
         && (RxData[16]&0x60)==0x40 )) // current 5A
                                                                ? TRUE : FALSE;
}
#endif

#ifdef CONFIG_SUPPORT_EMARKER
void ModifyPDO3A(void) //  if SRC_PDO[] max.current above 3A, modify it 3A
{
    BYTE ii, type;
    WORD tmp;

    /* Fix it by using maximum Source capability PDO sizes */
    for (ii = 0; ii < 7; ii++)
    {
        tmp = GetPDOLowWord(ii);
        type = SRC_PDO[ii][3] & 0xF0;
        /* Gary modify at 180420 (fixed bug: TD.PD.SRC3.E22 Cable Type Detection for Ellisys) */
#ifdef CONFIG_PPS
        if (DPM_PDO_TYPE_AUGMENTED == type)
        {
            if ((tmp & 0x7F) > (3000/DPM_AUGMENTED_MAX_CURR_UNIT))
            {
                tmp = (tmp & 0xFF80) | (3000/DPM_AUGMENTED_MAX_CURR_UNIT);
            }
        } /* APDO */
        else
#endif
        {
            type &= 0xC0;
            if ((DPM_PDO_TYPE_FIXED == type) || (DPM_PDO_TYPE_VARIABLE == type)) // Fixed or Variable
            {
                if ((tmp & 0x3FF) > 3000/DPM_FIX_MAX_CURR_UNIT)
                {
                    tmp = tmp & 0xFC00 | 3000/DPM_FIX_MAX_CURR_UNIT;
                }
            }
        } /* Other */
        SRC_PDO[ii][0] = tmp;
        SRC_PDO[ii][1] = tmp >> 8;
   }
}
#endif

void DisableConstCurrent ()
{
    if (!IS_OPTION_CCUR())
    {
        DISABLE_CONST_CURR(); // CV mode with OCP
    }
    SET_ANALOG_CABLE_COMP();
}

void DetachToDefault ()
{
    BYTE RpSel = GET_OPTION_RP(); // Gary add at 180213 (fixed bug: when loading is 3A, OCP error of type-c)
    WORD tmp = PDO_V_DAC[0] & 0x3FF;
    DisableConstCurrent();
    u16TargetVol = tmp; // initial for PM_STATE_ATTACHING Power-Good check
    u8RDOPositionMinus = 0;
    SetPwrV(tmp);

   /* Gary add at 180213 (fixed bug: when loading is 3A, OCP error of type-c) */
    if (RpSel == 2)
        SetPwrI(OCP_3A);
    else if (RpSel == 1)
        SetPwrI(OCP_1P5A);
    else
        SetPwrI(OCP_500MA);

    bPMExtra_DrSwp = IS_OPTION_DRSWP();
    bPMExtra_PdoOpt = IS_OPTION_PDO2();
    bPMExtra_Pdo2nd = 0;

#ifdef CONFIG_PPS
    bPMExtra_SameAPDO = 0; /* Gary add at 180209 (fixed bug: Small step time issue) */
    bPMExtra_RecordPDOType = 0;
#endif
}

// =============================================================================
// state machine
void Go_PM_STATE_DETACHING ()
{
    u8StatePM = PM_STATE_DETACHING;
    u8DischgTimer = T_DETACH_DISCHG_MIN; // (N+1)-ms discharge at least
    SET_VBUS_OFF();
    DetachToDefault();
    if (u8StatePE == PE_SRC_Transition_to_default
        || u8StatePE == PE_SRC_Hard_Reset
        || u8StatePE == PE_SRC_Error_Recovery)
    {
        u16PMTimer = T_SRC_RECOVER; // start tSrcRecover from VBUS off
    }
   DISCHARGE_ENA(); // to discharge VBUS
}

void Go_PM_STATE_DISCHARGE_SKIP () // discharge VIN or skip (go DETACHING)
{
    u8StatePM = PM_STATE_DISCHARGE;
    if (IS_VCONN_ON())
    {
        SET_VCONN_OFF();
        if (IS_VCONN_ON1())
            SET_RP1_OFF();
        if (IS_VCONN_ON2())
            SET_RP2_OFF();
        TOGGLE_FLIP();
        SET_DRV0_ON(); // VCONN discharge starts
    }
    // ===== codes before VBUS-off to earn more discharging time for VCONN =====
    STOP_TMR0();
    CLR_ANALOG_CABLE_COMP();
    PhysicalLayerReset();

    u8PMStaBits = 0; // clear PPS status

    SetPwrI(u8TypeC_PWR_I);
    PWRTransCalc(0); // transition to PDO1 (VSafe_5V_Max)
    u8TargetCurr = u8TypeC_PWR_I;

    (s8TransStep >= 0 || bProF2Off) // don't voltage stepping
                                    ? Go_PM_STATE_DETACHING()
                                    : PWRTransStart();
    // ===== codes before VBUS-off to earn more discharging time for VCONN =====
    if (IS_DRV0_ON())
    {
        SET_DRV0_OFF(); // VCONN discharge ends
        TOGGLE_FLIP();
        SET_RP_ON();
    }
}

void PolicyManagerInit ()
{
    u8StatePM = PM_STATE_DETACHED;

    INIT_ANALOG_CABLE_DETECT();
    INIT_ANALOG_DAC_EN();
    INIT_ANALOG_PK_SET_NTC();

    INIT_ANALOG_CURSNS_EN();
    INIT_ANALOG_SELECT_CL();

    DetachToDefault();

    if (IS_OPTION_IFB())
    {
        INIT_ANALOG_FB_SEL_IFB();
    }

    if (IS_OPTION_EX_VFB())
    {
        SET_ANALOG_DD_SEL_VFB();
    }

    if (IS_OPTION_SR())
    {
        ENABLE_SR();
    }

    /* Gary add at 180720 (Add check function: Vconn OCP Mode */
    if (IS_OPTION_V5OCP())
        SET_V5OCP_EN();
    SET_ADC_CHANNEL();
}

void PolicyManagerTmr0 ()
{
    bPMExtra_VolDone = (u16PresentVol == u16TargetVol);

    if (bPMExtra_VolDone
        || u8StatePM != PM_STATE_DISCHARGE && bProF2Hr || bProF2Off)
    {
        bTmr0PM = 0;
        bEventPM = 1; // VoltTransEnds
    }
    else
    {
        STRT_TMR0(T_STEPPING); // start the next stepping
        u16PresentVol += s8TransStep;
        if (s8TransStep > 0)
        {
            if (u16PresentVol > u16TargetVol)
                u16PresentVol = u16TargetVol;
        }
        else
        {
            if (u16PresentVol < u16TargetVol)
                u16PresentVol = u16TargetVol;
        }
        SetPwrV(u16PresentVol);
    }
}

void PolicyManagerTmr1 ()
{
    if (u16PMTimer && !--u16PMTimer)
        bEventPM = 1;

    u8PresentCurr = GetPwrI();
    bPMExtra_CurrDone = (u8PresentCurr == u8TargetCurr);

    if ((bPMExtra_VolDone) && (bPMExtra_CurrDone))
        bPMExtra_PWRTransOK = 1;

    if ((!bPMExtra_PWRTransOK) && (!bPMExtra_CurrDone))
    {
#ifdef CONFIG_PPS
        if (bPMExtra_RecordPDOType)
        {
            CurrentSteping();
        }
        else
#endif
        {
            if (bPMExtra_VolDone)
            {
                SetPwrI(u8TargetCurr);
            }
        }
    }

    switch (u8StatePM)
    {
    case PM_STATE_DETACHED:
        if (bTCAttch)
            bEventPM = 1; // to attach
        break;
    case PM_STATE_ATTACHING:
    case PM_STATE_ATTACHED_TRANS_PWRGD:
        if (IsPwrGood())
        {
            bEventPM = 1;
        }
        else if (bEventPM) // PwrGood timer (PM_TIMER_PWRGD_TIMEOUT) timeout
        {
            ResumeOVP();
            bEventPM = 0;
        }
      // continue to check attachment and protect, instead of 'break'
    case PM_STATE_ATTACHED:
    case PM_STATE_PWRGOOD:
    case PM_STATE_ATTACHED_IPHONE:
    case PM_STATE_ATTACHED_WAIT_CAP:
    case PM_STATE_ATTACHED_TRANS_PRE:
    case PM_STATE_ATTACHED_TRANS:
    case PM_STATE_ATTACHED_TRANS_DISCHG:
    case PM_STATE_ATTACHED_TRANS_PPS:
    case PM_STATE_ATTACHED_TRANS_PST:
        if (~bTCAttch | // to detach
             bProF2Hr | bProF2Off // to HR/OFF
             | bProCLChg & bDevSta_PPSReq & (u8StatePE==PE_SRC_Ready)
#ifdef CONFIG_TS_DYNAMIC_PDP
            || (bTsUpdateCurrentEvent && (u8StatePE==PE_SRC_Ready))
#endif
        )
        {
            bEventPM = 1;
        }
        break;
    case PM_STATE_DETACHING:
        if (DACV1 <= (DAC_VSAFE_0V_MAX>>2)-3 && (
        // VIN/VBUS detected '0' during the begining of discharging
        //        DACV1 >  (DAC_VSAFE_0V_MAX>>2>>1) ||
            u8DischgTimer == 0))
        {
            bEventPM =1;
        }
        else if (u8DischgTimer > 0)
        {
            u8DischgTimer--;
        }
        break;
    }
}

void PolicyManagerProc(void)
{
#ifdef CONFIG_SUPPORT_EMARKER
    BYTE tmp, tmp1, tmpCount;
#endif
    switch (u8StatePM)
    {
    case PM_STATE_ATTACHING: // from power-good/ms
        if (!bTCAttch)
        {
            u8StatePM = PM_STATE_DETACHED;
        }
        else if (!(bProF2Hr | bProF2Off) && IsPwrGood()) // wait for those FAULT disappeared
        {
            if (IS_OPTION_CAPT())
            {
                bDevSta_CBL = 0;
                bDevSta_5ACap = 1;
            }
            else
            {
#ifdef CONFIG_SUPPORT_EMARKER
                if (bDevSta_CBL)
                {
                    TR1 = 0;
                    tmp = 0xAf;
                    tmpCount = 5;
                    while (tmp--)
                    {
                        CLR_V5OCPSTA();
                        /* Switch Vconn on & off every time */
                        if( tmp & 0x01)
                        {
                            SET_VCONN_OFF();
                            tmp1 = 5;
                        }
                        else
                        {
                            SET_VCONN_ON();
                            tmp1 = tmpCount;
                        }

                        if( 0==(tmp & 0x07)) /* Add raising time */
                        {
                            tmpCount++;
                        }

                        while (tmp1--);
                    }

                    TR1 = 1;
                }
#endif
            }

            u8StatePM = PM_STATE_PWRGOOD;
//       QcStartup(); // shall startup QC in ISR of timer1
            if (PE_SRC_Transition_to_default == u8StatePE)
                bEventPE = 1; // start NoResponseTimer
            u16PMTimer = PM_TIMER_VBUSON_DELAY; // power good to Source_Capabilities delay
            SET_ANALOG_CABLE_COMP();
            CLR_F2OFFSTA(); // clear SCP/OVP status
            SET_VBUS_ON();
        }
        break; /* PM_STATE_ATTACHING */

    case PM_STATE_PWRGOOD: // from u16PMTimer, power good delay timeout
        if (bTCAttch && !(bProF2Hr || bProF2Off)) // don't check IsPwrGood() once VBUS on
        {
            /* Check V5OCP status to decide 5A to 3A current */
            if (!bDevSta_CBL ||
                bDevSta_CBL && IS_V5OCPSTA())
            {
                SET_VCONN_OFF(); // VCONN OCP
                CLR_V5OCPSTA();
                bDevSta_CBL = 0;
                ModifyPDO3A();
            }

            u8StatePM = PM_STATE_ATTACHED;
            bEventPE = 1;
        }
        else // from TC
        {
            if (bTCAttch) Go_PE_SRC_Error_Recovery();
            Go_PM_STATE_DISCHARGE_SKIP();
        }
        break; /* PM_STATE_PWRGOOD */
    case PM_STATE_ATTACHED:
    case PM_STATE_ATTACHED_IPHONE:
    case PM_STATE_ATTACHED_WAIT_CAP:
    case PM_STATE_ATTACHED_TRANS_PRE:   // tSrcTransition
    case PM_STATE_ATTACHED_TRANS:       // stepping
    case PM_STATE_ATTACHED_TRANS_DISCHG:
    case PM_STATE_ATTACHED_TRANS_PWRGD: // wait for PwrGood
    case PM_STATE_ATTACHED_TRANS_PPS:   // PPS stepping
    case PM_STATE_ATTACHED_TRANS_PST:   // delay
        if (!bTCAttch || bProF2Off || bProF2Hr && !bPESta_CONN)
        {
            if (bTCAttch)
                Go_PE_SRC_Error_Recovery();
            Go_PM_STATE_DISCHARGE_SKIP(); // this may refer to PE_SRC_Error_Recovery
        }
        else if (bProF2Hr) // && bPESta_CONN
        {
            u8StatePM = PM_STATE_ATTACHED_SEND_HR;
            bEventPE = 1;
            bDevSta_SndHR = 1;
        }
        else // from PE/QC
        {
            switch (u8StatePE)
            {
            case PE_SRC_Transition_to_default:
                Go_PM_STATE_DISCHARGE_SKIP();
                break;
            case PE_VCS_Turn_On_VCONN:
                switch (u8StatePM)
                {
                case PM_STATE_ATTACHED:
                    u8StatePM = PM_STATE_ATTACHED_TRANS_PST;
                    SET_VCONN_ON();
                    u16PMTimer = PM_TIMER_VCONNON; // start VCONN-ON delay
                    break;
                case PM_STATE_ATTACHED_TRANS_PST:
                    u8StatePM = PM_STATE_ATTACHED;
                    bEventPE = 1;
                    break;
                }
                break;
            case PE_SRC_Disabled: // QC
                switch (u8StatePM)
                {
                case PM_STATE_ATTACHED:
                    PWRTransCalc(-1); // transition to u16TargetVol/u8TargetCurr
                    u8StatePM = PM_STATE_ATTACHED_TRANS;
                    PWRTransStart();
                    break;
                case PM_STATE_ATTACHED_TRANS:
                    u8StatePM = PM_STATE_ATTACHED;
                    DISCHARGE_OFF();
                    ResumeOVP();
                    break;
                }
                break;
            case PE_SRC_Transition_Supply:
                switch (u8StatePM)
                {
                case PM_STATE_ATTACHED:
                    //PWRTransCalc(u8RDOPositionMinus); // transition to the requested PDO
                    /* Gary add at 180209 (fixed bug: Small step time issue and APDO 5V to APDO 3V issue) */
                    if (bPMExtra_SameAPDO)
                    {
                        u8StatePM = PM_STATE_ATTACHED_TRANS_PPS;
                        PWRTransStart();
                        u16PMTimer = bDevSta_LgStep
                                     ? PM_TIMER_LARGE_STEP : PM_TIMER_SMALL_STEP;
                    }
                    else
                    {
                        u8StatePM = PM_STATE_ATTACHED_TRANS_PRE;
                        u16PMTimer = T_SRC_TRANSITION; // start tSrcTransition
                    }
                    break;
                case PM_STATE_ATTACHED_TRANS_PRE:
                    u8StatePM = PM_STATE_ATTACHED_TRANS;
                    PWRTransStart();
                    break;
                case PM_STATE_ATTACHED_TRANS: // fixed transition
                    /* Gary add at 180416 (fixed bug: ADPO convert to PDO issue which PS_RDY time more then 275ms) */
                    if (IS_OCP_EN() || ((!bDevSta_PPSReq) && (!IS_OPTION_CCUR())))
                    {
                        u8StatePM = PM_STATE_ATTACHED_TRANS_PWRGD;
                        u16PMTimer = PM_TIMER_PWRGD_TIMEOUT;
                    }
                    else // CC mode transition
                    {
                        u8StatePM = PM_STATE_ATTACHED_TRANS_PST;
                        u16PMTimer = PM_TIMER_LARGE_STEP - T_SRC_TRANSITION;
                    }
                    break;
                case PM_STATE_ATTACHED_TRANS_PWRGD: // Power Good detected (timeout won't entry)
                    if (IS_DISCHARGE())
                    {
                        u8StatePM = PM_STATE_ATTACHED_TRANS_DISCHG;
                        u16PMTimer = PM_TIMER_DISCHG_DELAY;
                    }
                    else
                    {
                        u8StatePM = PM_STATE_ATTACHED_TRANS_PST;
                        u16PMTimer = PM_TIMER_PWRGD_DELAY;
                    }
                    break;
                case PM_STATE_ATTACHED_TRANS_DISCHG:
                    u8StatePM = PM_STATE_ATTACHED_TRANS_PST;
                    DISCHARGE_OFF();
                    u16PMTimer = PM_TIMER_PWRGD_DELAY;
                    break;

                case PM_STATE_ATTACHED_TRANS_PPS: // PPS stepping done and then go PM_STATE_ATTACHED_TRANS_PST for large/small-step timeout
                    u8StatePM = PM_STATE_ATTACHED_TRANS_PST; // so that large/small-step time must be above the stepping time
                    break;

                case PM_STATE_ATTACHED_TRANS_PST:
                    u8StatePM = PM_STATE_ATTACHED;
                    if (bDevSta_PPSReq)
                    {
                        ENABLE_CONST_CURR();
                        CLR_ANALOG_CABLE_COMP(); /* Gary add at 180214(Cable-Compensation is not disable in PPS mode)  */
                        //SET_OVPINT(0);
                    }
                    else
                    {
                        DisableConstCurrent();
                    }
                    ResumeOVP();
                    DISCHARGE_OFF();
                    bEventPE = 1;
                    break;
                }
                break;
            case PE_SRC_Wait_New_Capabilities:
                if (u8StatePM == PM_STATE_ATTACHED_WAIT_CAP)
                {
                   u8StatePM = PM_STATE_ATTACHED;
                   bEventPE = 1;
                }
                else
                {
                   u8StatePM = PM_STATE_ATTACHED_WAIT_CAP;
                   u16PMTimer = PM_TIMER_WAIT_CAP;
                }
                break;
            case PE_SRC_Startup: // from PE_SRC_VDM_Identity_Request
#ifdef CONFIG_SUPPORT_EMARKER
                IsCable5ACapable() ? (bDevSta_5ACap = 1) : ModifyPDO3A();
                bDevSta_CBL = 0;
#endif
                bEventPE = 1;
                break;
            case PE_SRC_Ready: // once an explicit contract or an AMS end
                if (bPMExtra_PdoOpt)
                {
                    if (bPMExtra_Pdo2nd)
                    {
                        bPMExtra_PdoOpt = 0; // the 2nd nego. done
                    }
                    else if (u8StatePM==PM_STATE_ATTACHED)
                    {
                        u8StatePM = PM_STATE_ATTACHED_IPHONE;
                        Do_PE_INIT_PORT_VDM_Identity_Request();
                    }
                    else // PM_STATE_ATTACHED_IPHONE
                    {
                        if (bPrlRcvd && // DiscoverID responded
                            (RxHdr&0x801F)==0xF // VDM
                            && (RxData[0]&0xDF)==0x41 // DiscoverID ACK respond
                            && (RxData[1]&0x80)==0x80 // structured VDM
                            && *((WORD*)&RxData[4])==BIG_ENDIAN(VID_APPLE))
                        {
// rev.20180411 begin
                            u8NumSrcPdo = 2;
                            MEM_COPY_X2X(SRC_PDO[1], SRC_PDO[6], 4);
                            PDO_V_DAC[1] = (PDO_V_DAC[1] & 0xF000) | (PDO_V_DAC[6] & 0x0FFF);
                        } /* Support Apple ID */
                        else
                        {
                            /* Gary add at 20180604 (modify 2nd PDO mechanism) */
                            if (bPESta_PD2)
                                u8NumSrcPdo = u8NumSrcFixedPdo;
                            else
                                u8NumSrcPdo = u8NumSrcPdoTab;

                            if (u8NumSrcPdo > 6)
                                u8NumSrcPdo--;
                        } /* Does not apple id */
                        bPMExtra_Pdo2nd = 1;
                        Go_PE_SRC_Send_Capabilities();
// rev.20180411 end
                        u8StatePM = PM_STATE_ATTACHED;
                    }
                }
                if (!bPMExtra_PdoOpt && bPMExtra_DrSwp)
                {
                   Do_PE_DRS_Send_Swap();
                   bPMExtra_DrSwp = 0;
                }
                if (!bPMExtra_PdoOpt && !bPMExtra_DrSwp // all the extra functions done
                         && !bPESta_PD2)
                {
                    SetSnkTxOK();
                }
#ifdef CONFIG_TS_DYNAMIC_PDP
                if (!bPMExtra_PdoOpt && !bPMExtra_DrSwp // all the extra functions done
                 && bTsUpdateCurrentEvent)
                {
                    UpdatePdoCurrent();
                    //u8StatePE = PE_SRC_Wait_New_Capabilities;
                    //bEventPE = 1;
                    Go_PE_SRC_Send_Capabilities();
                    bTsUpdateCurrentEvent = 0;
                }
#endif
#ifdef CONFIG_PPS
                if (bProCLChg)
                {
                   bProCLChg = 0;
                   if (bDevSta_PPSReq)
                   {
                      u8StatePE = PE_SRC_Send_Source_Alert;
                      AMS_Start();
                      bEventPRLTX = 1;
                   }
                }
                bPMExtra_RecordPDOType = bDevSta_PPSReq;
#endif
                break;
            }
        }
        break;

    case PM_STATE_ATTACHED_SEND_HR: // tPSHardReset
        Go_PM_STATE_DISCHARGE_SKIP();
        break;
    case PM_STATE_DISCHARGE: // from transition
        Go_PM_STATE_DETACHING();
        break;
    case PM_STATE_DETACHING: // from VSafe_0V_Max/ms, earlier than tSrcRecover
        DISCHARGE_OFF();
        DISABLE_VBUS_VIN_CHANNELS();
        bEventPE = 0; // if PE decide to issue Source_Cap/DiscoverID, cancel it

        switch (u8StatePE)
        {
        case PE_SRC_Error_Recovery: // T_SRC_RECOVER should be set at entering DETACHING
            SET_RP_OFF();
        case PE_SRC_Transition_to_default:
            u8StatePM = PM_STATE_RECOVER;
            break;
        default:
            u8StatePM = PM_STATE_DETACHED;
            PolicyEngineReset();
            break;
        }
        break;
    case PM_STATE_RECOVER: // tSrcRecover
        u8StatePM = PM_STATE_DETACHED;
        bEventPE = 1;
        break;
    case PM_STATE_DETACHED:
        if (bTCAttch)
        {
            bDevSta_CBL = bCableCap;
            u8StatePM = PM_STATE_ATTACHING;
            ENABLE_VBUS_VIN_CHANNELS();
            ReloadPdoTable(); // takes long!!
#if defined(CONFIG_TS_DEFAULT_PDP) && (TS_DETERMINE_PDP_COUNT >= 2)
            UpdatePdoCurrent();
#ifdef CONFIG_TS_DYNAMIC_PDP
            bTsUpdateCurrentEvent = 0;
#endif //CONFIG_TS_DYNAMIC_PDP
#endif
            ResumeOVP();
        }
        break;
    }
    bEventPM = 0;
}