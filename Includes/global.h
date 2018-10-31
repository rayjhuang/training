#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include "can1110_reg.h"
#include "can1110_hal.h"

#define T_RECEIVE 870 // 0.9~1.1ms (tReceive), TxBuffer-to-FIFO takes time!!
#define T_BISTCM2 1330 // 1.33ms, inform PrlTx before FIFO empty
#define TIMEOUT_TMR1 (1000-12)

#ifdef CONFIG_SIMULATION // for Simulation
#define PM_TIMER_VBUSON_DELAY  4
#define PM_TIMER_DISCHG_DELAY  2
#define PM_TIMER_PWRGD_DELAY   2
#define PM_TIMER_PWRGD_TIMEOUT 7
#define PM_TIMER_SMALL_STEP    4
#define PM_TIMER_LARGE_STEP    8
#define PM_TIMER_VCONNON       1
#define PM_TIMER_WAIT_CAP      5
#define T_DETACH_DISCHG_MIN    1
#define T_CC_DEBOUNCE          6
#define T_CC_SLEEP             25
#define T_SENDER_RESPONSE      5
#define T_VCONN_SOURCE_ON      5
#define T_PS_HARD_RESET        3
#define T_SRC_TRANSITION       3
#define T_SRC_RECOVER          10
#define T_C_SENDSRCCAP         7
#define T_NO_RESPONSE          70 // > (T_C_SENDSRCCAP + 8) * PE_SRC_CAP_COUNT
#define T_PPS_TIMEOUT          90
#define T_F2HR_2V8             3
#define T_F2HR_DEBNC           3
#define T_F2OFF_DEBNC          4
#define T_CURRENT_LIMIT        3
#define T_SINK_TX              3
#define T_CHUNK_NOT_SUPPORTED  4
#define PE_SRC_CAP_COUNT 4
#define N_BISTCM2 100 // 5.33ms
#define STEP_UP 6
#define STEP_DN -3
#define T_STEPPING 80
#define TIMEOUT_TMR1_SLP 20
#else // for real chip
#define PM_TIMER_VBUSON_DELAY  200 // Max.50ms (TypeC - tVCONNStable)
#define PM_TIMER_DISCHG_DELAY  20
#define PM_TIMER_PWRGD_DELAY   10
#define PM_TIMER_PWRGD_TIMEOUT 300
#define PM_TIMER_SMALL_STEP    23 // 0~25ms (tPpsSrcTransSmall)
#define PM_TIMER_LARGE_STEP    260 // 0~275ms (tPpsSrcTransLarge)
#define PM_TIMER_VCONNON       5
#define PM_TIMER_WAIT_CAP      400
#define T_DETACH_DISCHG_MIN    5
#define T_CC_DEBOUNCE          120 // 100~200ms (tCCDebounce)
#define T_CC_SLEEP             3000
#define T_SENDER_RESPONSE      27 // 24~30ms (tSenderResponse)
#define T_VCONN_SOURCE_ON      50
#define T_PS_HARD_RESET        30 // 25~35ms (tPSHardReset)
#define T_SRC_TRANSITION       30 // 25~35ms (tSrcTransition)
#define T_SRC_RECOVER          800 // 0.66~1s (tSrcRecover)
#define T_C_SENDSRCCAP         120 // 100~200ms (tTypeCSendSourceCap)
#define T_NO_RESPONSE          5000 // 4.5~5.5s (tNoResponse)
#define T_PPS_TIMEOUT          13000 // ~15s (tPPSTimeout)
#define T_F2HR_2V8             20
#define T_F2HR_DEBNC           30
#define T_F2OFF_DEBNC          30
#define T_CURRENT_LIMIT        30
#define T_SINK_TX              17 // 16~20 (tSinkTx)
#define T_CHUNK_NOT_SUPPORTED  43 // 40~50ms (tChunkingNotSupported)
#define N_BISTCM2 800 // 42.67ms, N_BISTCM2*2*8 bits x3.33us, 30~60ms (tBISTContMode)
#define STEP_UP 2
#define STEP_DN -2
#define T_STEPPING 250
#define TIMEOUT_TMR1_SLP 165 // 20ms sample rate
#endif

#define T_PD_DEBOUNCE 2
#define nRetryCount 3
#define nHardResetCount 2

#define SID_STANDARD      0xFF00
#define VID_APPLE         0x05AC
#define VID_QUALCOMM_QC40 0x05C6
#define VID_CANYON_SEMI   0x2A41 // (10817)

#define BIG_ENDIAN(v) ((v<<8)|(v>>8))

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef bit TRUE_FALSE;
typedef bit SCUUESS_FAILED;
enum {
   FALSE = 0,
   TRUE = 1,
   SUCCESS = 0,
   FAILED = 1
}; // true-or-false, success-or-failed check


extern BYTE xdata* lpMemXdst;
extern BYTE xdata* lpMemXsrc;
extern BYTE code*  lpMemCsrc;

void MemCopyC2X (BYTE cnt);
void MemCopyX2X (BYTE cnt);
#define MEM_COPY_C2X(dst,src,cnt) (lpMemXdst=dst,lpMemCsrc=src,MemCopyC2X(cnt))
#define MEM_COPY_X2X(dst,src,cnt) (lpMemXdst=dst,lpMemXsrc=src,MemCopyX2X(cnt))

void MtTablesInit ();
void ReloadPdoTable ();

#ifdef CONFIG_TS_DEFAULT_PDP

#if (TS_DETERMINE_PDP_COUNT >= 2)
void UpdatePdoCurrent(void);
#endif

#ifdef CONFIG_TS_DYNAMIC_PDP
void TsDetectTmr1(void);
extern bit bTsUpdateCurrentEvent;
#endif

#endif /* CONFIG_TS_DEFAULT_PDP */

extern bit bEventPM;
extern bit bEventPE;
extern bit bEventPRLTX;
extern bit bEventPRLRX;
extern bit bEventPHY;
extern bit bEventTC;
#ifdef CONFIG_QC3
extern bit bEventQC3;
#endif

extern bit bTmr0PM;
extern bit bTmr0TO;


extern WORD u16BistCM2Cnt;

extern BYTE xdata TxData [];
extern BYTE xdata RxData [];
extern WORD RxHdr;
extern BYTE TxHdrL, TxHdrH;

void PhysicalLayerReset ();
void PhysicalLayerStartup ();
void PhysicalLayerProc ();


extern bit bPrlSent;
extern bit bPrlRcvd;
extern bit bPhySent;
extern bit bPhyRetry;
extern bit bPhyPatch;

void PRL_Tx_PHY_Layer_Reset ();
void ProtocolTxProc ();
void ProtocolRxProc ();


typedef enum {
   PE_SRC_Startup,
   PE_SRC_Send_Capabilities,     // 0x01
   PE_SRC_Negotiate_Capability,  // 0x02
   PE_SRC_Hard_Reset,            // 0x03
   PE_SRC_Disabled,              // 0x04
   PE_SRC_Discovery,             // 0x05
   PE_SRC_Transition_Accept,     // 0x06
   PE_SRC_Transition_Supply,     // 0x07
   PE_SRC_Transition_PS_RDY,     // 0x08
   PE_SRC_Ready,                 // 0x09
   PE_SRC_Get_Sink_Cap,          // 0x0A
   PE_SRC_Capability_Response,   // 0x0B
   PE_SRC_Wait_New_Capabilities, // 0x0C
   PE_SRC_Transition_to_default, // 0x0D
   PE_SRC_Hard_Reset_Received,   // 0x0E
   PE_SRC_VDM_Identity_Request,  // 0x0F
   PE_SRC_Send_Soft_Reset,       // 0x10
   PE_SRC_Soft_Reset,            // 0x11
   PE_SRC_Send_Reject,           // 0x12
   PE_SRC_Send_Source_Alert,     // 0x13
   PE_SRC_Send_Not_Supported,    // 0x14
   PE_SRC_Chunk_Recieved,        // 0x15
   PE_SRC_Give_Source_Status,    // 0x16
   PE_SRC_Give_PPS_Status,       // 0x17
   PE_SRC_Give_Source_Cap_Ext,   // 0x18
   PE_SRC_Error_Recovery,        // 0x19
   PE_VCS_Accept_Swap,           // 0x1A
   PE_VCS_Wait_for_VCONN,        // 0x1B
   PE_VCS_Turn_On_VCONN,         // 0x1C
   PE_VCS_Send_PS_RDY,           // 0x1D
   PE_BIST_Carrier_Mode,         // 0x1E
   PE_RESP_VDM_Send_QC4_Plus,    // 0x1F
   PE_RESP_VDM_Send_Identity,    // 0x20
   PE_RESP_VDM_Get_Identity_NAK, // 0x21
   PE_DRS_Accept_Swap,           // 0x22
   PE_DRS_Send_Swap,             // 0x23
   PE_INIT_PORT_VDM_Identity_Request,
   PE_INIT_PORT_VDM_Identity_ACKed,
   PE_INIT_PORT_VDM_Identity_NAKed
}  PE_STATE_BYTE;
extern PE_STATE_BYTE u8StatePE;

extern bit bPESta_PD2;
extern bit bPESta_DFP;
extern bit bPESta_CONN;
extern bit bPESta_RcvHR;

extern BYTE u8PETimer;
extern WORD u16PETimer;
extern BYTE u8CurrentVal;

void PolicyEngineReset ();
void PolicyEngineTmr1();
void PolicyEngineProc ();
void Go_PE_SRC_Send_Capabilities ();
void Go_PE_SRC_Error_Recovery ();
void Do_PE_DRS_Send_Swap ();
void Do_PE_INIT_PORT_VDM_Identity_Request ();
void AMS_Start ();
TRUE_FALSE IsAmsPending ();


extern BYTE u8RDOPositionMinus;
extern BYTE u8NumSrcPdo;
extern BYTE u8NumSrcFixedPdo;
extern BYTE u8NumSrcPdoTab;
extern BYTE u8SrcPDP;
extern BYTE code* lpPdoTable;
extern WORD xdata PDO_V_DAC  [];
extern BYTE xdata SRC_PDO    [][4];
extern BYTE xdata OPTION_REG [];

#define OPTION_RP1          0x01
#define OPTION_RP2          0x02
#define OPTION_OVP          0x04
#define OPTION_SCP          0x08
#define OPTION_OTP          0x10
#define OPTION_UVP          0x20
#define OPTION_CCUR         0x40
#define OPTION_RP           (OPTION_RP1 | OPTION_RP2)

#define OPTION_CCMP         0x07
#define OPTION_DCP          0x08
#define OPTION_APPLE        0x10
#define OPTION_QC           0x20
#define OPTION_CAPT         0x40
#define OPTION_SR           0x80

#define OPTION_FB_SEL       0x02
#define OPTION_DD_SEL       0x04
#define OPTION_SLPEN        0x08
#define OPTION_QC_PWR       0x10
#define OPTION_QCMIN        0x60

#define OPTION_DRSWP        0x01
#define OPTION_PDO2         0x02
#define OPTION_V5OCP        0x04 /* Gary add at 180720 */

#define OPTION_D4_0         OPTION_RP1
#define OPTION_D4_1         OPTION_DCP
#define OPTION_D4_2         0x00
#define OPTION_D4_3         0x00

#define IS_OPTION_OVP()    (OPTION_REG[0] & OPTION_OVP)
#define IS_OPTION_SCP()    (OPTION_REG[0] & OPTION_SCP)
#define IS_OPTION_OTP()    (OPTION_REG[0] & OPTION_OTP)
#define IS_OPTION_UVP()    (OPTION_REG[0] & OPTION_UVP)
#define IS_OPTION_CCUR()   (OPTION_REG[0] & OPTION_CCUR)

#define IS_OPTION_DCP()    (OPTION_REG[1] & OPTION_DCP)
#define IS_OPTION_APPLE()  (OPTION_REG[1] & OPTION_APPLE)
#define IS_OPTION_QC()     (OPTION_REG[1] & OPTION_QC)
#define IS_OPTION_CAPT()   (OPTION_REG[1] & OPTION_CAPT)
#define IS_OPTION_SR()     (OPTION_REG[1] & OPTION_SR)
#define IS_OPTION_IFB()    (OPTION_REG[2] & OPTION_FB_SEL)
#define IS_OPTION_EX_VFB() (OPTION_REG[2] & OPTION_DD_SEL)
#define IS_OPTION_SLPEN()  (OPTION_REG[2] & OPTION_SLPEN)
#define IS_OPTION_QC_PWR() (OPTION_REG[2] & OPTION_QC_PWR)

#define IS_OPTION_DRSWP()  (OPTION_REG[3] & OPTION_DRSWP)
#define IS_OPTION_PDO2()   (OPTION_REG[3] & OPTION_PDO2)
#define IS_OPTION_V5OCP()   (OPTION_REG[3] & OPTION_V5OCP) /* Gary add at 180720 */

#define GET_OPTION_RP()    (OPTION_REG[0] & OPTION_RP)
#define GET_OPTION_CCMP()  (OPTION_REG[1] & OPTION_CCMP)
#define GET_OPTION_VREF()  (OPTION_REG[2] & OPTION_VREF)
#define GET_OPTION_QCMIN() (OPTION_REG[2] & OPTION_QCMIN)
#define GET_OPTION_QCPWR() ((OPTION_REG[2] & OPTION_QC_PWR) >> 4)

extern BYTE code* lpVdmTable;
extern BYTE u8NumVDO;

extern bit bDevSta_CBL;
extern bit bDevSta_SndHR;
extern bit bDevSta_PPSReq;
extern bit bDevSta_PPSRdy;

extern bit bPMExtra_Pdo2nd;
extern signed char s8TransStep;

void PolicyManagerTmr0 ();
void PolicyManagerTmr1 ();
void PolicyManagerInit ();
void PolicyManagerProc ();

extern WORD u16TargetVol;

TRUE_FALSE IsRdoAgreed ();
BYTE GetPwrIxPdoPos ();
void SetPwrI (BYTE);
WORD GetPwrV ();
void SetPwrV(WORD);
BYTE GetTemperature (TRUE_FALSE low);

extern bit bTCAttch;
extern bit bTCSleep;
extern bit bCableCap;
extern BYTE u8TypeC_PWR_I;

void TypeCCcDetectSleep ();
void TypeCCcDetectTmr1 ();
void TypeCCcDetectProc ();
void TypeCResetRp ();
void SetSnkTxNG();
void SetSnkTxOK();

extern bit bProF2Hr;
extern bit bProF2Off;
extern bit bProCLimit;
extern bit bProCLChg;
void ResumeOVP ();
void SrcProtectInit ();
void SrcProtectTmr1 ();

/*Ming add at 181009 (add PWM function)*/
#ifdef CONFIG_PWM	
	void PWMInit(void);
	void PWMOneMinTmr1(void);
	void PWMTwoMinTmr1(void);	
#endif

#ifdef CONFIG_SUPPORT_QC
extern bit bQcSta_InQC;
extern void QcStartup (void);
extern void QcTmr1 (void);

#ifdef CONFIG_QC3
extern bit bQC3Flag;
extern void QC_Proc_ContMode(void);
#endif
#endif

#endif // _GLOBAL_H_
