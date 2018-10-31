#ifndef _CAN1110_HAL_H_
#define _CAN1110_HAL_H_

#include "CAN1110_Configure.h"

#define INT_VECT_INT0    0  // 03h, EXINT0_INT_VECT (low/fall)
#define INT_VECT_TIMER0  1  // 0Bh
#define INT_VECT_INT1    2  // 13h, EXINT1_INT_VECT (low/fall)
#define INT_VECT_TIMER1  3  // 1Bh
#define INT_VECT_UART0   4  // 23H
#define INT_VECT_I2C     5  // 2Bh, TIMER2
#define INT_VECT_COMP    8  // 43h, EXINT7_INT_VECT, (rise), SDA
#define INT_VECT_HWI2C   9  // 4Bh, EXINT2_INT_VECT,~(fall/rise), MISO
#define INT_VECT_P0     10  // 53h, EXINT3_INT_VECT,~(fall/rise)
#define INT_VECT_UPDRX  11  // 5Bh, EXINT4_INT_VECT, (rise)
#define INT_VECT_UPDTX  12  // 63h, EXINT5_INT_VECT, (rise)
#define INT_VECT_SRC    13  // 6Bh, EXINT6_INT_VECT, (rise)

#define INIT_TMR0()  (TR0 = 0, TMOD = (TMOD & 0xFC) | 0x01, ET0 = 1)
#define STOP_TMR0()  (TR0 = 0)
#define STRT_TMR0(v) (TH0 = ((- v) >> 8), \
                      TL0 = ( - v),       \
                      TR0 = 1)

#define INIT_TMR1()  (TR1 = 0, TMOD = (TMOD & 0xCF) | 0x10, ET1 = 1)
#define STOP_TMR1()  (TR1 = 0)
#define STRT_TMR1(v) (TH1 = ((- v) >> 8), \
                      TL1 = ( - v),       \
                      TR1 = 1)

#define SET_ISR_GROUP1_LEVEL1()       (IP0 |= 0x02)

#ifdef CONFIG_CAN1110C0
#define DAC_2V6  650
#define DAC_1V6  400
#define DAC_0V8  200
#define DAC_0V4  100
#define DAC_0V2  50
#else
#define DAC_2V6  0x3FF
#define DAC_1V6  800
#define DAC_0V8  400
#define DAC_0V4  200
#define DAC_0V2  100
#endif

#define DAC_0V08 40
#define DAC_VSAFE_0V_MAX        DAC_0V08 // VIN:0.8V

#define IS_DAC1COMP_BUSY()      (DACCTL &  0x01)
#define START_SARADC_ONCE()     (DACCTL |= 0x01)
#define START_DAC1COMP()        (DACCTL  = 0x0F) // DAC1/COMP starts (some SFRs are to be protected)
#define STOP_DAC1COMP()         (DACCTL &=~0x01) // stop DAC1/COMP
#define STOP_DAC1_LOOP()        (DACCTL &=~0x03) // stop DAC1/COMP and turn off DACMUX loop
#define SET_DAC1COMP_FAST()     (DACCTL &=~0x0C) // 1us cycle DAC1/COMP
#define SET_DAC1COMP_SLOW()     (DACCTL |= 0x0C) // 5us cycle DAC1/COMP
#define SET_ANALOG_SLEEP()      (ATM    |= 0x01) // SLEEP=1
#define SET_ANALOG_AWAKE()      (ATM    &=~0x01) // SLEEP=0
#define SET_ANALOG_DD_SEL_VFB() (DACLSB |= 0x08) // DD_SEL=1
#define SET_OSC_SLOW()          (OSCCTL |= 0x02) // OSC_LOW=1
#define SET_OSC_FAST()          (OSCCTL &=~0x02) // OSC_LOW=0

#define SET_DPDN_SHORT()        (PWRCTL |= 0x08) // let DP/DN short
#define SET_DPDN_NOT_SHORT()    (PWRCTL &=~0x08) // DP/DN not short
#define SET_DN_PULLDWN()        (PWRCTL |= 0x10) // DN pulldown
#define SET_DN_NOT_PULLDWN()    (PWRCTL &=~0x10) // DN not pulldown
#define SET_2V7_ON()            (PWRCTL |= 0x80) // turn on DP 2.7V driver
#define SET_2V7_OFF()           (PWRCTL &=~0x80) // turn off DP 2.7V driver
#define SET_2V7_SHORT_OFF()     (PWRCTL &=~0x88) // turn off DP 2.7V driver and DP/DN short
#define SET_2V7_DNDWN_OFF()     (PWRCTL &=~0x90) // turn off DP 2.7V driver and DN pulldown
#define SET_DPDN_ALL_OFF()      (PWRCTL &=~0x98) // clear DPDN_*

#define ENABLE_CONST_CURR()        (CVCTL &=~0x01) // CC_ENB = 0 to enable CC
#define DISABLE_CONST_CURR()       (CVCTL |= 0x01) // CC_ENB = 1 to disable CC while enabling OCP
#define INIT_ANALOG_CURSNS_EN()    (CVCTL |= 0x04) // enable current sense
#define INIT_ANALOG_FB_SEL_IFB()   (CVCTL |= 0x08)
#define INIT_ANALOG_CABLE_DETECT() {CDCTL = 0x08; CDCTL = 0x00;}
#define INIT_ANALOG_DAC_EN()       (DACLSB = 0x05) // DAC_EN 
/** Gary modify at 180523(fixed bug: OTP register setting error(Original: 0x0C ==> 22uA), change to 0x0B(100uA)) */
#define INIT_ANALOG_PK_SET_NTC()   (CCTRX |= 0x0B) // PK_SET = 1, 100uA NTC
#define INIT_ANALOG_SELECT_CL()    (REGTRM5 |= 0x02) // re-route OTPI to Current Limit
#define SET_ANALOG_CABLE_COMP()    (CMPOPT  = (CMPOPT & ~0x07) | GET_OPTION_CCMP()) // cable compensation
#define CLR_ANALOG_CABLE_COMP()    (CMPOPT &= (~0x07))

#define COMP_SWITCH(v)              (v ? (CMPOPT |= 0x80) : (CMPOPT &= 0x7F))
#define GET_COMP_SWITCH_VAL()       ((CMPOPT & 0x80) >> 7)
#define SET_V5OCP_EN()       (PROCTL = 0x20) // V5OCP will gating VCONN

#define SET_RCV_SOPP()       (RXCTL |= 0x02)
#define SET_DONT_RCV_SOPP()  (RXCTL &=~0x02)

#ifdef CONFIG_TS_DEFAULT_PDP
#define SET_ADC_CHANNEL()    (SAREN  = 0xFF)
#else
#define SET_ADC_CHANNEL()    (SAREN  = 0xF7)
#endif // CONFIG_TS_DEFAULT_PDP

/* Ming add at 181009 (add PWM function)*/
#ifdef CONFIG_PWM 
#define INCREASE_DUTY()    	(PWMD=0x19)
#define SET_PGATE_OFF()    	{PWMP=0x00;PWMP=0x3B;}
#define GET_NGATE_STATUS()  (DACLSB &  0x10)
#define GET_COMPCV_STATUS() (DDBND  &  0x08)
#define SET_NGATE_OFF()    	(DACLSB |= 0x10)
#define SET_NGATE_ON()    	(DACLSB &=~0x10)
#define ENABLE_ISENSE_CHANNEL()(DACEN  |= 0x01)
#define ENABLE_INITIAL_CHANNEL(){SAREN  |= 0xF7;DACEN |= 0xC7;}
#define ENABLE_ONLY_ISENSE_CHANNEL()	{SAREN	&=~0xFE;DACEN &=~0xFE;}
#endif


#define INI_DAC_CHANNEL()    (DACEN  = 0xC0)
#define ENABLE_TS_SARADC()   (SAREN |= 0x08)
#define DISABLE_TS_SARADC()  (SAREN &=~0x08)

#define IS_UNDER288()      (IS_VBUS_ON() && DACV1<36) // VBUS < 2.88V(80mV*36)
#define IS_UNDER312()      (IS_VBUS_ON() && DACV1<39) // VBUS < 3.12V(80mV*39)
#define DAC_OFS_PWR_I()      (PDO_V_DAC[2]>>12)
#define DAC_CODE_OTP_HI()  (((PDO_V_DAC[0]>>8) & 0xF0) | (PDO_V_DAC[1]>>12))
#define DAC_CODE_OTP_LO()  (((PDO_V_DAC[3]>>8) & 0xF0) | (PDO_V_DAC[4]>>12))
#define SET_TS_HI_THRESHOLD() (DACV3 = DAC_CODE_OTP_HI()) // to update DACV* needs stop DAC1/COMP in advance
#define SET_TS_LO_THRESHOLD() (DACV3 = DAC_CODE_OTP_LO())

#define SET_FLIP()       (CCCTL |= 0x01) // to select CC2 (flipped)
#define NON_FLIP()       (CCCTL &=~0x01) // to select CC1 (non-flipped)
#define IS_FLIP()        (CCCTL &  0x01) // orientation (0/1: non-flipped/flipped)
#define TOGGLE_FLIP()    (CCCTL ^= 0x01)

#define RESET_AUTO_GOODCRC()        (PRLTX  = 0x07) // POR value

#define ENABLE_BOTH_CC_CHANNELS()   (DACEN |= 0xC0)
#define STOP_SENSE_VCONN_CHANNEL()  (DACEN &= (IS_FLIP()) ?~0x40 :~0x80)
#define STOP_SENSE_CC_CHANNEL()     (DACEN &= (IS_FLIP()) ?~0x80 :~0x40)
#define GO_SENSE_CC_CHANNEL()       (DACEN |= (IS_FLIP()) ? 0x80 : 0x40)
#define ENABLE_DPDN_CHANNELS()      (DACEN |= 0x30)
#define DISABLE_DPDN_CHANNELS()     (DACEN &=~0x30)
#define ENABLE_TS_CHANNEL()         (DACEN |= 0x08)
#define DISABLE_TS_CHANNEL()        (DACEN &=~0x08)

#ifdef CONFIG_CAN1110C0
#define ENABLE_VBUS_VIN_CHANNELS()  (DACEN |= 0x07)

#ifdef CONFIG_PWM
#define DISABLE_VBUS_VIN_CHANNELS() (DACEN &=~0x06)
#else
#define DISABLE_VBUS_VIN_CHANNELS() (DACEN &=~0x07)
#endif

#define PWR_GOOD_DACV() DACV2
#else
#define ENABLE_VBUS_VIN_CHANNELS()  (DACEN |= 0x03)
#define DISABLE_VBUS_VIN_CHANNELS() (DACEN &=~0x03)
#define PWR_GOOD_DACV() DACV0
#endif

#define SET_RD_OFF()     (CCCTL |= 0x0C)
#define SET_RP_VAL(v)    (CCCTL  = (CCCTL &~0x30) | (v << 4))
#define SET_RP_ON()      (CCCTL |= 0xC0)
#define SET_RP_OFF()     (CCCTL &=~0xC0)
#define SET_RP1_OFF()    (CCCTL &=~0x40)
#define SET_RP2_OFF()    (CCCTL &=~0x80)

#define SET_DRV0_ON()    (CCCTL |= 0x02)
#define SET_DRV0_OFF()   (CCCTL &=~0x02)
#define IS_DRV0_ON()     (CCCTL &  0x02)

#define ENABLE_SR()      (SRCCTL |= 0x10) // SR_EN=1
//                        SRCTL  &=~0x80; // SRCP_EN=0

#define IS_VCONN_ON1()   (SRCCTL &  0x04)
#define IS_VCONN_ON2()   (SRCCTL &  0x08)
#define IS_VCONN_ON()    (SRCCTL &  0x0C)
#define SET_VCONN_ON()   (IS_FLIP() ? (SRCCTL |= 0x04) : (SRCCTL |= 0x08))
#define SET_VCONN_OFF()  (SRCCTL &=~0x0C)

#define DISCHARGE_ENA()  (SRCCTL |= 0x02)
#define DISCHARGE_OFF()  (SRCCTL &=~0x02)
#define IS_DISCHARGE()   (SRCCTL &  0x02)

#define SET_VBUS_ON()    (SRCCTL |= 0x01)
#define SET_VBUS_OFF()   (SRCCTL &=~0x01)
#define IS_VBUS_ON()     (SRCCTL &  0x01)

#define GET_PWR_I()      (PWR_I & 0x7F)
#define SET_PWR_I(v)     (PWR_I = (PWR_I & 0x80) | (v & 0x7F))
#define SET_PWR_V(v)     (PWR_V = (v >> 2), PWRCTL = (PWRCTL & ~0x03) | v & 0x03)
#define GET_PWR_V()      ((WORD)PWR_V << 2) | (PWRCTL & 0x03)

#define SET_SCPINT(v)    (v ?(PROCTL |= 0x10) :(PROCTL &=~0x10))
#define SET_OVPINT(v)    (v ?(PROCTL |= 0x04) :(PROCTL &=~0x04))
#define OVP_ACTIVE()         (PROCTL &  0x04)

#define IS_CLVAL()       (PROVAL &  0x08) // OTPI can becomes CL in CAN1110C0
#define IS_OTPVAL()      (~COMPI &  0x08) // NTC resistance is below the threshold
#define IS_UVPVAL()      (PROVAL &  0x01)
#define IS_OCPVAL()      (PROVAL &  0x02)
#define IS_OVPVAL()      (PROVAL &  0x04)
#define IS_SCPVAL()      (PROVAL &  0x10)
#define IS_F2OFFVAL()    (PROVAL &  0x14) // fault-to-off, SCP/OVP
#define IS_V5OCPSTA()    (PROSTA &  0x20)
#define IS_F2OFFSTA()    (PROSTA &  0x14)
#define CLR_V5OCPSTA()   (PROSTA |= 0x20)
#define CLR_OVPSTA()     (PROSTA |= 0x04) // clear OVP status before resume OVP
#define CLR_F2OFFSTA()   (PROSTA |= 0x14) // fault-to-off, SCP/OVP
#define IS_PWREN()       (SRCCTL &  0x01) // power switch

#define IS_OCP_EN()      (CVCTL  & 0x01) // CC_ENB=1 (CC disabled) == OCP enabled

#endif // _CAN1110_HAL_H_
