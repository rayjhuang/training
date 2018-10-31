
#include "global.h"  


#define ISCurrent_168mA 7
#define u8Timer_10ms 5
#define u8Timer_160ms 80

#ifdef CONFIG_PWM
volatile BYTE xdata u8Timer160ms;
volatile BYTE xdata u8Timer10ms;

void PWMInit()
{
	DDCTL|=0x01; //enable auto-updating mechanism
	PWMP=0x3B;  //PWM frequency 200kHz
	DTR|=0x0A;  //NGATE rise dead time
	DTF|=0x0A;	//NGATE fall dead time
	DACLSB|=0xD0; //enable gate drivers and disable PWM_N
	REGTRM5|=0x01; //solve Isense problem
	ENABLE_ISENSE_CHANNEL();
}

void PWMOneMinTmr1() 
{	
	if (GET_NGATE_STATUS())
	{
		if(!GET_COMPCV_STATUS()) 
		{
			if (u8Timer160ms<=1)
				INCREASE_DUTY();
		}
		else 
		{
			if(u8Timer10ms!=0)
				SET_PGATE_OFF();
		}
	}
}

void PWMTwoMinTmr1()
{
	if(u8CurrentVal<=ISCurrent_168mA)
	{
		u8Timer10ms++;
		if(u8Timer10ms==u8Timer_10ms)
		{
			SET_NGATE_OFF();
			u8Timer10ms=1;
		}
		u8Timer160ms=0;
	}
	else
	{	
		u8Timer160ms++;
		if(u8Timer160ms==u8Timer_160ms)
		{
			SET_NGATE_ON();
			u8Timer160ms=2;
		}
		u8Timer10ms=0;
	}		 
}
#endif
