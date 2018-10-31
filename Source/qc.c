/*
// =============================================================================
// add APPLE/DCP/QC2/QC3 functions into CY2211S_V100 PD20 firmware
// 2017/11/16 Ray Huang

1. in timer1 ISR, call to QcTmr1()
   a. maintain debounce counter (u8DpDnDbCnt), Dp Dn status (u8DpDnVal)
         0: <DAC1_DPDNLO
         1: DAC1_DPDNLO<?<DAC1_DPDNHI
         2: >DAC1_DPDNHI
      if (u8DpDnVal changed)
         reset (re-start) u8DpDnDbCnt
         update u8DpDnVal
      else (u8DpDnDbCnt enabled && not saturation)
         u8DpDnDbCnt++;
   b. maintain QCSTA
      if (u8DpDnDbCnt == expire_value && change QCSTA)
         goto next state
2. add event 'bEventPM' for voltage stepping

// ALL Rights Are Reserved
// =============================================================================
*/
#include "global.h"
#include <intrins.h>

#ifdef CONFIG_SIMULATION
#define QC_T_APPLE2DCP                      3   // 3ms
#define QC_T_DCP2DN_GND                     4 // 1.0~1.5s (T_GLITCH_BC_DONE, 200ms x6)
#define QC_T_DNGND2QC2                      3   // 4ms
#define QC_T_QC2_GO_TRANS                   4  // 20~60ms (T_GLITCH_MODE_CHANGE)
#define QC_T_QC3_GLITCH_CONT_CHANGE         3   /* 50us*2=100us */
#define QC_T_QC3_ACTIVE                     4   // 200us
#define QC_T_QC3_MODECHANGE                 160 // (40000us/50us) / 5
#define QC_T_QC3_CLR_COUNT                  1200 // (60000us/50us) / 5
#define QC_T_QC3_TV_CONT_CHANGE             40  // 40 * 50us = 2ms
#define QC_T_DETACH                         4
#else
#define QC_T_APPLE2DCP                      3   // 3ms
#define QC_T_DCP2DN_GND                     201 // 1.0~1.5s (T_GLITCH_BC_DONE, 200ms x6)
#define QC_T_DNGND2QC2                      4   // 4ms
#define QC_T_QC2_GO_TRANS                   23  // 20~60ms (T_GLITCH_MODE_CHANGE)
#define QC_T_QC3_GLITCH_CONT_CHANGE         3   // 50us*3 = 150us
#define QC_T_QC3_ACTIVE                     4   // 200us
#define QC_T_QC3_INACTIVE                   4   // 200us
#define QC_T_QC3_MODECHANGE                 800 // 40000us/50us
#define QC_T_QC3_CLR_COUNT                  1200 // 60000us/50us
#define QC_T_QC3_TV_CONT_CHANGE             40  // 40 * 50us = 2ms
#define QC_T_DETACH                         11
#endif

#define QC_PMAX_18W                         18000
#define QC_PMAX_27W                         27000

#define QC_CC_3A                            60
#define QC_CC_2P25A                         45
#define QC_CC_2A                            40
#define QC_CC_1P5A                          30
#define QC_CC_FINETUN                       2

#define QC_VOL_LIMIT_3P6                    0x00
#define QC_VOL_LIMIT_4P0                    0x20
#define QC_VOL_LIMIT_4P4                    0x40
#define QC_VOL_LIMIT_5P0                    0x60

#define QC_CURRENT_TRANS                    0x10

#ifdef CONFIG_FPGA // for FPGA
#define DAC0_PWRV5V0                        0x0C8 //  50x4=0x0C8 (8'h32,2'h0)
#define DAC0_PWRV6V0                        0x12C //  60x4=0x0F0 (8'h3C,2'h0)
#define DAC0_PWRV9V0                        0x168 //  90x4=0x168 (8'h5A,2'h0)
#define DAC0_PWRV12V                        0x1E0 // 120x4=0x1E0 (8'h78,2'h0)
#define DAC0_PWRV15V                        0x258 // 150x4=0x258 (8'h96,2'h0)
#define DAC0_PWRVMIN                        36*8 // 3.6V
#define DAC0_PWRVSTP                        2*8 // 200mV, step of continuous mode
#else // for Real chip
#define DAC0_PWRV5V0                        0x0FA // 250=0x0FA (8'h3E,2'h2)
#define DAC0_PWRV6V0                        0x12C // 300=0x12C (8'h4B,2'h0)
#define DAC0_PWRV9V0                        0x1C2 // 450=0x1C2 (8'h70,2'h2)
#define DAC0_PWRV12V                        0x258 // 600=0x258 (8'h96,2'h0)
#define DAC0_PWRV15V                        0x2EE // 750=0x2EE (8'hBB,2'h2)
#define DAC0_PWRVMIN0                       180 // 3.6V
#define DAC0_PWRVMIN1                       200 // 4.0V
#define DAC0_PWRVMIN2                       220 // 4.4V
#define DAC0_PWRVMIN3                       250 // 5.0V
#define DAC0_PWRVSTP                        10 // 200mV, step of continuous mode
#endif

#define DAC1_CODE_DPDNLO                    41 // (325mV/(2mV*4)
#define DAC1_CODE_DPDNHI                    250 // (2000mV/(2mV*4)

#define QC_GET_CODE_DP()                    DACV4
#define QC_GET_CODE_DN()                    DACV5
#define QC_CODE_VDAT_REF                    DAC1_CODE_DPDNLO // 0.325V
#define QC_CODE_SEL_REF                     DAC1_CODE_DPDNHI // 2V

#define Get_DAC_EN_VAL()                    (DACEN)
#define SET_DAC_EN_VAL(val)                 (DACEN = (DACEN & 0x00) | val)
#define SET_DAC_QC_CHANNEL()                (DACEN = (DACEN & 0x00) | 0x32)
#define CLR_DACCTL_SETTING()                (DACCTL = 0x00)
#define SET_DACCTL_QC_START()               (CLR_DACCTL_SETTING(), DACCTL = 0x03)
#define SET_DACCTL_QC_STOP()                (CLR_DACCTL_SETTING(), START_DAC1COMP())

#define QC_DPDN_DCP_MODE                    0x01
#define QC_DPDN_9V_MODE                     0x12
#define QC_DPDN_12V_MODE                    0x11
#define QC_DPDN_CONT_MODE                   0x21

#define QC_DPDN_CONT_VOL_DEC                0x11
#define QC_DPDN_CONT_VOL_INC                0x22

volatile enum {
   QCSTA_IDLE,
   QCSTA_APPLE,
   QCSTA_DCP,
   QCSTA_DNGND,
   QCSTA_QC20,
   QCSTA_QC30
} u8QcSta _at_ 0x18;

typedef enum
{
    QC_VOL_5V,
    QC_VOL_9V,
    QC_VOL_12V,
    QC_VOL_CONT
}TE_VOL_MODE;

#ifdef CONFIG_SUPPORT_QC
volatile BYTE bdata u8QcBits;
sbit bQcSta_InQC    = u8QcBits ^ 0; // 0: entry other mode, 1: entry QC mode
sbit bQcOpt_DCP     = u8QcBits ^ 3; // #define OPTION_DCP   0x08
sbit bQcOpt_APPLE   = u8QcBits ^ 4; // #define OPTION_APPLE 0x10
sbit bQcOpt_QC      = u8QcBits ^ 5; // #define OPTION_QC    0x20
sbit bQcOpt_QC_PWR  = u8QcBits ^ 6; // 0: 18W, 1:27W

volatile WORD u16DpDnDbCnt;
volatile BYTE u8DpDnVal;
volatile BYTE u8DcpCnt;
volatile BYTE u8BackupDACEN;

BYTE QcVoltMin;
BYTE QcVolMode;
WORD QC_u16Vol;

extern BYTE u8TargetCurr;

#ifdef CONFIG_QC3
static void QC_Delay_us(void)
{
    BYTE i;

    for (i = 0; i < 40; i++)
        _nop_();
}

static void QC_ResumeQC3Setting(void)
{
    SET_DAC_EN_VAL(u8BackupDACEN);
    SET_DACCTL_QC_STOP();
    ResumeOVP();
    bEventQC3 = 0;
    TR0 = 1;
    TR1 = 1;
}
#endif

static void QC_Startup(void)
{
    u8QcBits = OPTION_REG[1] & (OPTION_DCP | OPTION_APPLE | OPTION_QC);
    bQcOpt_QC_PWR = GET_OPTION_QCPWR();

    switch (GET_OPTION_QCMIN())
    {
    case QC_VOL_LIMIT_5P0:
        QcVoltMin = DAC0_PWRVMIN3;
        break; /* 5V */
    case QC_VOL_LIMIT_4P4:
        QcVoltMin = DAC0_PWRVMIN2;
        break; /* 4.4V */
    case QC_VOL_LIMIT_4P0:
        QcVoltMin = DAC0_PWRVMIN1;
        break; /* 4V */
    default:
        QcVoltMin = DAC0_PWRVMIN0;
        break; /* 3.6V */
    } /* Please ref. Optaion Table */

    if (bQcOpt_DCP | bQcOpt_APPLE)
    {
        ENABLE_DPDN_CHANNELS();
        SET_DN_NOT_PULLDWN();

        if (bQcOpt_APPLE)
        {
            SET_2V7_ON();

            (bQcOpt_DCP)
                ? SET_DPDN_SHORT()
                : SET_DPDN_NOT_SHORT();
            u8QcSta = QCSTA_APPLE;
        } /* Enable Apple mode */
        else
        {
            SET_DPDN_SHORT();
            u8DcpCnt = 0;
            u8QcSta = QCSTA_DCP;
        } /* Enable DCP mode */

        bQcSta_InQC = 0; // init QC state
        u16DpDnDbCnt = 1;

#ifdef CONFIG_QC3
        if (bEventQC3)
        {
            QC_ResumeQC3Setting();
        }
#endif
    } /* Support DCP/Apple mode */
}

static void QC_VolTrans(void) // voltage in 200mV
{
    u16TargetVol = QC_u16Vol;

#ifdef CONFIG_QC3
    if (QCSTA_QC30 == u8QcSta)
    {
        SetPwrV(u16TargetVol);
    } /* QC3 mode */
    else
#endif
    {
        bEventPM = 1;
    } /* Other mode */
}

static void QC_CurrTrans(void)
{
    BYTE Current = QC_CC_3A;
    WORD QcPDP = QC_PMAX_18W;

#ifdef CONFIG_QC3
    if (QCSTA_QC30 == u8QcSta)
    {
        if (bQcOpt_QC_PWR)
        {
            QcPDP = QC_PMAX_27W;
            if (DAC0_PWRV9V0 > QC_u16Vol)
                goto SET_CC_POINT;
        }
        else
        {
            if (DAC0_PWRV6V0 > QC_u16Vol)
                goto SET_CC_POINT;
        }

        Current = 0;
        while (QcPDP >= QC_u16Vol)
        {
            QcPDP -= QC_u16Vol;
            Current++;
        }
    } /* QC3.0 */
    else
#endif
    {
        if (bQcOpt_QC_PWR)
        {
            if (DAC0_PWRV12V == QC_u16Vol)
                Current = QC_CC_2P25A;
        }
        else
        {
            if (DAC0_PWRV9V0 == QC_u16Vol)
                Current = QC_CC_2A;
            else if (DAC0_PWRV12V == QC_u16Vol)
                Current = QC_CC_1P5A;
        }
    } /* QC2.0 */

#ifdef CONFIG_QC3
SET_CC_POINT:
#endif
    u8TargetCurr = Current + QC_CC_FINETUN;
    SetPwrI(u8TargetCurr);
}

static void QC_ChkDpDetach(void)
{
    if (((u8DpDnVal & 0x0F) == 0x00) && (QC_T_DETACH == u16DpDnDbCnt))
    {
        QC_Startup();
        QC_u16Vol = DAC0_PWRV5V0;
        QC_VolTrans();
        DISABLE_CONST_CURR();
        SET_ANALOG_CABLE_COMP();
    } /* Detach */
}

static void QC_DetectSignal4DPDN(void)
{
    BYTE DpDnNxt;

    DpDnNxt  = (QC_CODE_VDAT_REF > QC_GET_CODE_DP())
                ? 0x00
                : (QC_CODE_SEL_REF > QC_GET_CODE_DP())
                    ? 0x01
                    : 0x02;
    DpDnNxt |= (QC_CODE_VDAT_REF > QC_GET_CODE_DN())
                ? 0x00
                : (QC_CODE_SEL_REF > QC_GET_CODE_DN())
                    ? 0x10
                    : 0x20;

    if (u8DpDnVal != DpDnNxt)
    {
        u8DcpCnt = 0;
        u16DpDnDbCnt = 1;
    }
    else if (
#ifdef CONFIG_QC3
                ((QCSTA_QC30 == u8QcSta) && (u16DpDnDbCnt > QC_T_QC3_CLR_COUNT))
                || ((QCSTA_QC20 == u8QcSta) && (u16DpDnDbCnt > QC_T_QC2_GO_TRANS)))
#else
                ((QCSTA_QC20 == u8QcSta) && (u16DpDnDbCnt > QC_T_QC2_GO_TRANS)))
#endif
    {
        u16DpDnDbCnt = 0;
    }
    else if (u16DpDnDbCnt > 0)
    {
        u16DpDnDbCnt++;
    }

    u8DpDnVal = DpDnNxt;
}

#ifdef CONFIG_QC3
void QC_Proc_ContMode(void)
{
    WORD target;

    QC_Delay_us(); // Waiting scan cycle of DAC (3 channel)
    QC_DetectSignal4DPDN();

    if (QC_CURRENT_TRANS == u16DpDnDbCnt)
    {
        DISCHARGE_OFF();
        ResumeOVP();
        QC_CurrTrans();
    }

    if (QC_T_QC3_GLITCH_CONT_CHANGE == u16DpDnDbCnt)
    {
        target = GetPwrV();

        if (QC_DPDN_CONT_VOL_DEC == u8DpDnVal)
        {
            target -= DAC0_PWRVSTP;
            if (QcVoltMin > target)
            {
                goto END_QC3MODE;
            } /* min voltage = 3.6V */
            DISCHARGE_ENA();
            SET_OVPINT(0);
        } /* Tactive & decrement */
        else if (QC_DPDN_CONT_VOL_INC == u8DpDnVal)
        {
            target += DAC0_PWRVSTP;
            if (DAC0_PWRV12V < target)
            {
                goto END_QC3MODE;
            } /* max voltage = 12V */
        } /* Tactive & increment */
        else
        {
            goto END_QC3MODE;
        }

        QC_u16Vol = target;
        QC_VolTrans();
    } /* signal pulse and multiple */
    else if ((0x01 == u8DpDnVal) && (QC_T_QC3_MODECHANGE < u16DpDnDbCnt))
    {
        u8QcSta = QCSTA_QC20;
        u16DpDnDbCnt = QC_T_QC2_GO_TRANS - 1; // return to QC2 State
        QC_ResumeQC3Setting();
    } /* > 20ms equal to Mode Change, exit Continous mode */

END_QC3MODE:
    QC_ChkDpDetach();
}
#endif

void QcTmr1(void)
{
    if (!bPESta_CONN && IS_VBUS_ON())
    {
        QC_DetectSignal4DPDN();

        switch (u8QcSta)
        {
        case QCSTA_IDLE:
            u8BackupDACEN = Get_DAC_EN_VAL();
#ifdef CONFIG_QC3
            bEventQC3 = 0;
#endif
            QC_Startup();
            break;

        case QCSTA_APPLE:
            if (bQcOpt_DCP && (QC_T_APPLE2DCP == u16DpDnDbCnt) && ((u8DpDnVal & 0x0F) < 2))
            {
                u16DpDnDbCnt = 0;
                u8QcSta = QCSTA_DCP;
                SET_2V7_OFF();
                u8DcpCnt = 0;
                u16DpDnDbCnt = 1;
            } /* DP/DN short if DCP enabled */
            break; /* Apple mode */
            
        case QCSTA_DCP:
            if (QC_T_DCP2DN_GND == u16DpDnDbCnt)
            {
                u16DpDnDbCnt = 0;
                if (u8DcpCnt < 5)
                {
                    u8DcpCnt++;
                    u16DpDnDbCnt = 1;
                }
                else if (bQcOpt_QC && (u8DpDnVal == 0x11))
                {
                    SET_2V7_SHORT_OFF();
                    SET_DN_PULLDWN();
                    u8QcSta = QCSTA_DNGND;
                } /* DP/DN short */
                else if (bQcOpt_APPLE && (u8DpDnVal == 0x00))
                {
                    // DP/DN short
                    // DP/DN=0V after QC_T_DCP2DN_GND x6, which means
                    // 1) DpDnDetach
                    // 2) Device doesn't drive DP/DN any more
                    QC_Startup();
                }
            } /* Loop (QC_T_DCP2DN_GND * 6) */
            break; /* DCP mode */
         
        case QCSTA_DNGND:
            if ((QC_T_DNGND2QC2 == u16DpDnDbCnt) && (u8DpDnVal == 0x01))
            {
                u16DpDnDbCnt = 0;
                QcVolMode = QC_VOL_5V;
                u8TargetCurr = QC_CC_3A + QC_CC_FINETUN;
                SetPwrI(u8TargetCurr);
                ENABLE_CONST_CURR();
                CLR_ANALOG_CABLE_COMP();
                u8QcSta = QCSTA_QC20;
                bQcSta_InQC = 1; // entry QC mode
            }
            QC_ChkDpDetach();
            break;

        case QCSTA_QC20:
            if (QC_T_QC2_GO_TRANS == u16DpDnDbCnt)
            {
                u16DpDnDbCnt = 0;

#ifdef CONFIG_QC3
                if (QC_DPDN_CONT_MODE == u8DpDnVal)
                {
                    TR0 = 0;
                    TR1 = 0;
                    u8BackupDACEN = Get_DAC_EN_VAL();

                    /* DAC sample cycle = 48us */
                    SET_DAC_QC_CHANNEL();
                    SET_DACCTL_QC_START(); // each channel spends 16us(2us*8=16us)
                    u8QcSta = QCSTA_QC30;
                    bEventQC3 = 1;
                }
                else if (QC_DPDN_9V_MODE == u8DpDnVal)
#else
                if (QC_DPDN_9V_MODE == u8DpDnVal)
#endif
                {
                    QC_u16Vol = DAC0_PWRV9V0;
                    QC_VolTrans();
                    QC_CurrTrans();
                    QcVolMode = QC_VOL_9V;
                }
                else if (QC_DPDN_12V_MODE == u8DpDnVal)
                {
                    QC_u16Vol = DAC0_PWRV12V;
                    QC_VolTrans();
                    QC_CurrTrans();
                    QcVolMode = QC_VOL_12V;
                }
                else if (QC_DPDN_DCP_MODE == u8DpDnVal)
                {
                    QC_u16Vol = DAC0_PWRV5V0;
                    QC_VolTrans();
                    QC_CurrTrans();
                    QcVolMode = QC_VOL_5V;
                }
            } /* Mode Change */

            QC_ChkDpDetach();
            break; /* QC2.0  */
        } /* QC state machine */
    } /* entry QC */
    else if (QCSTA_IDLE != u8QcSta)
    {
        // to disable QC
        u8QcSta = QCSTA_IDLE;
        DISABLE_DPDN_CHANNELS();
// rev.20180402 begin
//        (bQcOpt_DCP) ? SET_2V7_DNDWN_OFF() // only DP/DN short remained
        SET_DPDN_ALL_OFF(); // free DP/DN
// rev.20180402 end
    } /* avoid QC state error */
}
#endif