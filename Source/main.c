#include "global.h"

volatile BYTE bdata u8Event _at_ 0x20;
sbit bEventPM     = u8Event^0; // policy manager
sbit bEventPE     = u8Event^1; // policy engine
sbit bEventPRLTX  = u8Event^2; // protocol TX
sbit bEventPRLRX  = u8Event^3; // protocol RX
sbit bEventPHY    = u8Event^4; // physical
sbit bEventTC     = u8Event^5; // type-C
#ifdef CONFIG_QC3
sbit bEventQC3    = u8Event^6; // QC3
#endif

volatile BYTE bdata u8TmrBits _at_ 0x27;
sbit bTmr0PM      = u8TmrBits^0; // timer0 for Policy Manager (voltage stepping)
sbit bTmr0TO      = u8TmrBits^1; // timer0 timeout

void IsrTimer0 () interrupt INT_VECT_TIMER0
{
   STOP_TMR0(); // timeout
   if (bTmr0PM)
   {
      PolicyManagerTmr0();
   }
   else
   {
      bEventPRLTX = 1;
      bTmr0TO = 1;
   }
}

void IsrTimer1 () interrupt INT_VECT_TIMER1
{
    STOP_TMR1();

    /* Gary add at 180614 (modify falling too fast issue), channel switch*/
    if (GET_COMP_SWITCH_VAL())
	{
      	u8CurrentVal = DACV0;
	/* Ming add at 181009 (add PWM function)*/
#ifdef CONFIG_PWM				 
		PWMTwoMinTmr1();
		ENABLE_INITIAL_CHANNEL();	
#endif		
	}
	/* Ming add at 181009 (add PWM function)*/
#ifdef CONFIG_PWM	
		PWMOneMinTmr1();
#endif
    if (bTCSleep)
    {
        TypeCCcDetectSleep();
        STRT_TMR1(TIMEOUT_TMR1_SLP);
    }
    else
    {
        STRT_TMR1(TIMEOUT_TMR1);

/* DEBUG BEGIN
    PWRCTL = PWRCTL&0x03 | (IS_CLVAL()  ? 0x40 : 0x10); // output to DN
    PWRCTL = PWRCTL&0x03 | (IS_OCPVAL() ? 0x40 : 0x10); // output to DN
DEBUG END */
        SrcProtectTmr1();
#ifdef CONFIG_QC3
        if (!bEventQC3)
#endif
        {
            TypeCCcDetectTmr1();
            PolicyManagerTmr1();
            PolicyEngineTmr1();
#ifdef CONFIG_SUPPORT_QC
            QcTmr1();
#endif

#ifdef CONFIG_TS_DYNAMIC_PDP
            TsDetectTmr1();
#endif
        }
    }
	
	/* Ming add at 181009 (add PWM function)*/
#ifdef CONFIG_PWM
		if (!GET_COMP_SWITCH_VAL())	
			ENABLE_ONLY_ISENSE_CHANNEL();
#endif	
    /* Gary add at 180614 (modify falling too fast issue), channel switch */
    COMP_SWITCH(!GET_COMP_SWITCH_VAL());

}

void main ()
{
		int temp;
		temp += 2;
    MtTablesInit();
    PhysicalLayerReset();
    PolicyManagerInit();

    TypeCResetRp();
    SET_RP_ON();
    SET_RD_OFF();
    ENABLE_BOTH_CC_CHANNELS();

    SrcProtectInit();

    START_DAC1COMP();
		
		/*Ming add at 181009 (add PWM function)*/
#ifdef CONFIG_PWM	
		PWMInit(); 
#endif
// DEBUG BEGIN
// ATM |= 0x20; // TM[1]:DP=DAC1
// DEBUG END

    INIT_TMR0();
    INIT_TMR1();
    STRT_TMR1(TIMEOUT_TMR1);
    EA = 1; // enable interrupts

    while (1)
    {
        if (u8Event)
        {
            if (bEventTC)
                TypeCCcDetectProc();
            if (bEventPM)
                PolicyManagerProc();
#ifdef CONFIG_QC3
            if (bEventQC3)
                QC_Proc_ContMode();
#endif
            if (bEventPE)
                PolicyEngineProc();
            if (bEventPRLTX)
                ProtocolTxProc();
            if (bEventPRLRX)
                ProtocolRxProc();
            if (bEventPHY)
                PhysicalLayerProc();
        }
    }
}