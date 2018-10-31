
#include "global.h"

volatile BYTE bdata u8ProBits _at_ 0x25;
sbit bPro2HrSig   = u8ProBits^1;
sbit bPro2OffSig  = u8ProBits^2;
sbit bProCLSig    = u8ProBits^3;
sbit bProF2Hr     = u8ProBits^4;
sbit bProF2Off    = u8ProBits^5;
sbit bProCLimit   = u8ProBits^6;
sbit bProCLChg    = u8ProBits^7;

volatile BYTE u8Fault2OffTimer;
volatile BYTE u8Fault2HrTimer;
volatile BYTE u8CLimitTimer _at_ 0x13;

void IsrProtect() interrupt INT_VECT_SRC
{
    /* Gary add at 180509 (fixed bug: When eMark cable is plug to source, V5OCP occurs error) */
     /* Gary Add at 180719 (fix bug: V5OCP occurs false error) */
    //if (!IS_V5OCPSTA())
    if (IS_F2OFFSTA())
    {
        u8Fault2OffTimer = T_F2OFF_DEBNC;
        bPro2OffSig = 0;
        bProF2Off = 1;
    }
}

void ResumeOVP ()
{
    if (IS_OPTION_OVP())
    {
        if (!IS_OVPVAL())
            CLR_OVPSTA();
        SET_OVPINT(1);
    }
}

void SrcProtectInit ()
{
    CLR_F2OFFSTA(); // clear SCP/OVP status
    if (IS_OPTION_OVP() || IS_OPTION_SCP())
    {
        EX6 = 1; // enable protection interrupt
        if (IS_OPTION_SCP())
            SET_SCPINT(1);
      ResumeOVP(); // if (IS_OPTION_OVP()) SET_OVPINT(1);
      u8Fault2OffTimer = 1; // or, if SCP/OVP kept high since power-on, VBUS stays off
    }

    if (IS_OPTION_OTP())
    {
        ENABLE_TS_CHANNEL();
        SET_TS_HI_THRESHOLD();
    }
}

void SrcProtectTmr1 ()
{
    bit bProSigSav, bProSigTmp;

   // Current Limit
//   if (IS_CLVAL())
//   {
//      (u8CLimitTimer<T_CURRENT_LIMIT)
//         ? u8CLimitTimer++
//         : (bProCLimit = 1);
//   }
//   else
//   {
//      (u8CLimitTimer>0)
//         ? u8CLimitTimer--
//         : (bProCLimit = 0);
//   }

#ifdef CONFIG_SUPPORT_QC
    if (!bQcSta_InQC)
#endif
    {
#ifdef CONFIG_PPS
        bProSigSav = bProCLSig;
        bProCLSig = IS_CLVAL();

        /* Gary Add at 180522 (fixed bug: In the PPS Buck mode and CL occurs,
                                Discharge must be turned off to prevent the voltage from falling too fast) */
        /* Gary modify at 180614 (modify falling too fast issue)
        if (bProCLSig && bDevSta_PPSReq && (STEP_DN == s8TransStep))
            DISCHARGE_OFF();
        */
        if (bProSigSav == bProCLSig)
        {
            if (u8CLimitTimer && !--u8CLimitTimer)
            {
                if (bProCLimit != bProCLSig)
                {
                    if (!bProF2Hr)
                    {
                        bProCLChg = 1;
                        bProCLimit = bProCLSig;
                        /* Gary modify at 180618 (Avoid the simultaneous occurrence of HardRest and CL) */
                        if (u8Fault2HrTimer)
                            u8Fault2HrTimer = (IS_OCP_EN() ? T_F2HR_DEBNC : T_F2HR_2V8);
                    }
                }
            }
        }
        else
        {
            u8CLimitTimer = T_CURRENT_LIMIT;
        }
#endif
    } /* QC mode does not action */

    // Fault-to-Hard-Reset
    // Over Temperature Protection
    // Under Voltage Protection
    // Over Current or Low Voltage Boundary
    bProSigSav = bPro2HrSig;
    bProSigTmp = IS_OPTION_OTP() && IS_OTPVAL();

    bPro2HrSig = bProSigTmp
                    ||
//                      (IS_OPTION_UVP() && IS_UVPVAL()) && !bDevSta_PPSRdy || // CC vs UVP options decided by Writer
                        (IS_OCP_EN() ? IS_OCPVAL()     // in CV mode
                        /* Gary modify at 20180615 (modify under RDO MinAPDO range)
                                         : IS_UNDER288()*/
                                         : IS_UNDER312()); // in CC mode
// rev.20180321 begin
    if (u8Fault2HrTimer > 0)
    {
        if (IS_OCP_EN() && IS_DISCHARGE()) // reset (not disable) the de-bounce in discharging in OCP_EN mode
        {
            u8Fault2HrTimer = T_F2HR_DEBNC;
        }
        else if (bPro2HrSig) // in the active cycle
        {
            if (!--u8Fault2HrTimer)
            {
                bProF2Hr = 1;
#ifndef CONFIG_TS_DEFAULT_PDP
                STOP_DAC1COMP();
                (bProSigTmp) ? SET_TS_LO_THRESHOLD()
                             : SET_TS_HI_THRESHOLD();
                START_DAC1COMP();
#endif
            }
        }
        else if (u8Fault2HrTimer >= (IS_OCP_EN() ? T_F2HR_DEBNC : T_F2HR_2V8))
        {
            u8Fault2HrTimer = 0;
            bProF2Hr = 0;
        }
        else
        {
//              u8Fault2HrTimer = IS_OCP_EN() ? T_F2HR_DEBNC : T_F2HR_2V8;
        
            /* Gary Add at 180222 (fixed bug: OCP noise issue )*/
            u8Fault2HrTimer++;
        }
    }
    else if (bProSigSav != bPro2HrSig) // to start de-bounce
    {
        u8Fault2HrTimer = IS_OCP_EN() ? T_F2HR_DEBNC : T_F2HR_2V8;
    }
// rev.20180321 end

    // Fault-to-Off
    // Over Voltage Protection
    // Short Circuit Protection
    bProSigSav  = bPro2OffSig;
    bProSigTmp  = IS_OPTION_OVP() && IS_OVPVAL() && OVP_ACTIVE() ||
                  IS_OPTION_SCP() && IS_SCPVAL();
    bPro2OffSig = bProSigTmp ||
                  IS_OPTION_UVP() && IS_UVPVAL() && !bDevSta_PPSRdy; // CC vs UVP options decided by Writer
    
    if (bProSigSav == bPro2OffSig)
    {
        if (u8Fault2OffTimer && !--u8Fault2OffTimer)
        {
          bProF2Off = bPro2OffSig;
        }
    }
    else if (!bProSigTmp || bProF2Off) // only debounce for falling
    {
        u8Fault2OffTimer = T_F2OFF_DEBNC;
    }
    else // in case of OVP/SCP happens when disabled (no interrput)
    {
        bProF2Off = bPro2OffSig;
    }
}