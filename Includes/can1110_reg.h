/*******************************************************************************
 *  FILE NAME
 *  ---------
 *  reg_can1110.h
 *
 *  DESCRIPTION                                                        
 *  -----------
 *  CAN1110 propritary registers definition.
 *                                                                            
 *  Ver.  Date(Y-M-D)  Author  Description                             
 *  ----  -----------  ------  -----------
 *  1.0   2017-07-09   LYH     Initial version                         
 *
 *
 *  CopyRight Canyon Semiconductor Inc. 2017
 ******************************************************************************/
#ifndef _CAN1110_REG_H_
#define _CAN1110_REG_H_

#include <reg52.h>
#include "CAN1110_Configure.h"

/* R8051XC2 */
sfr WDTREL  = 0x86;

sfr S0CON   = 0x98;
sfr S0BUF   = 0x99;
sfr IEN0    = 0xA8; // IE in REG52.H
sfr IP0     = 0xA9;
sfr S0RELL  = 0xAA;
sfr IEN1    = 0xB8; // R80515
sfr S0RELH  = 0xBA;

sfr IRCON   = 0xC0;
sfr ADCON   = 0xD8;
sfr SRST    = 0xF7;

sfr I2CDAT  = 0xDA;
sfr I2CADR  = 0xDB;
sfr I2CCON  = 0xDC;
sfr I2CSTA  = 0xDD;

/* P0 */
sbit P0_7   = P0^7;
sbit P0_6   = P0^6;
sbit P0_5   = P0^5;
sbit P0_4   = P0^4;
sbit P0_3   = P0^3;
sbit P0_2   = P0^2;
sbit P0_1   = P0^1;
sbit P0_0   = P0^0;

sbit DB     = ADCON^7; // baud rate doubler

//it ET2    = IEN0^5; // defined in REG52.H
//it EI2    = IEN0^5; // in SK8623

//it EX0    = IEN0^0; // defined in REG52.H
//it ET0    = IEN0^1; // defined in REG52.H
//it EX1    = IEN0^2; // defined in REG52.H
//it ET1    = IEN0^3; // defined in REG52.H
sbit ES0    = IEN0^4;
//it ET2    = IEN0^5; // defined in REG52.H
sbit WDT    = IEN0^6; 
sbit EAL    = IEN0^7; // interrupt enable
//it EA     = IEN0^7; // in REG52.H

sbit EX7    = IEN1^0;
sbit EX2    = IEN1^1;
sbit EX3    = IEN1^2;
sbit EX4    = IEN1^3;
sbit EX5    = IEN1^4;
sbit EX6    = IEN1^5;
sbit SWDT   = IEN1^6;
//it EXEN2  = IEN1^7; // Timer2 external reload interrupt enable, defined in REG52.H

/* CAN1110 */
#define _PROVAL  0x9B
#define _FCPDAT  0x9C
#define _FCPSTA  0x9D
#define _FCPCTL  0x9E
#define _CMPOPT  0x9F

#define _CDCTL   0xA1

#define _PWR_I   0xA3
#define _PWMP    0xA4
#define _PWMD    0xA5
#define _PROCTL  0xA6
#define _PROSTA  0xA7

#define _CVCTL   0xAB
#define _DTR     0xAC
#define _DTF     0xAD
#define _DDCTL   0xAE
#define _DDBND   0xAF

#define _TXCTL   0xB0
#define _FFCTL   0xB1
#define _FFIO    0xB2
#define _STA0    0xB3
#define _STA1    0xB4
#define _MSK0    0xB5
#define _MSK1    0xB6
#define _FFSTA   0xB7

#define _RXCTL   0xBB
#define _MISC    0xBC
#define _PRLS    0xBD
#define _PRLTX   0xBE
#define _GPF     0xBF

#define _I2CCMD  0xC1
#define _OFS     0xC2
#define _DEC     0xC3
#define _PRLRXL  0xC4
#define _PRLRXH  0xC5
#define _TRXS    0xC6
#define _REVID   0xC7

#define _I2CCTL  0xC9
#define _I2CDEVA 0xCA
#define _I2CMSK  0xCB
#define _I2CEV   0xCC
#define _I2CBUF  0xCD
#define _PCL     0xCE
#define _NVMIO   0xCF

#define _NVMCTL  0xD1
#define _RWBUF   0xD2
#define _EXGP    0xD3
#define _OSCCTL  0xD4
#define _GPIOP   0xD5
#define _GPIOSL  0xD6
#define _GPIOSH  0xD7

#define _ATM     0xD9

#define _P0MSK   0xDE
#define _P0STA   0xDF

#define _COMPI   0xE1
#define _CMPSTA  0xE2
#define _SRCCTL  0xE3
#define _PWRCTL  0xE4
#define _PWR_V   0xE5
#define _CCRX    0xE6
#define _CCCTL   0xE7

#define _SRCTL   0xE8
#define _REGTRM0 0xE9
#define _REGTRM1 0xEA
#define _REGTRM2 0xEB
#define _REGTRM3 0xEC
#define _REGTRM4 0xED
#define _REGTRM5 0xEE


#define _DACCTL  0xF1
#define _DACEN   0xF2
#define _SAREN   0xF3
#define _SARLO   0xF4
#define _SARUP   0xF5
#define _DACLSB  0xF6

#define _DACV0   0xF8
#define _DACV1   0xF9
#define _DACV2   0xFA
#define _DACV3   0xFB
#define _DACV4   0xFC
#define _DACV5   0xFD
#define _DACV6   0xFE
#define _DACV7   0xFF

sfr PROVAL  = _PROVAL;
sfr FCPDAT  = _FCPDAT;
sfr FCPSTA  = _FCPSTA;
sfr FCPCTL  = _FCPCTL;
sfr CMPOPT  = _CMPOPT;

sfr CDCTL   = _CDCTL;
sfr CVCTL   = _CVCTL;
sfr PWR_I   = _PWR_I;
sfr PWMP    = _PWMP;
sfr PWMD    = _PWMD;
sfr PROCTL  = _PROCTL;
sfr PROSTA  = _PROSTA;

sfr DTR     = _DTR;
sfr DTF     = _DTF;
sfr DDCTL   = _DDCTL;
sfr DDBND   = _DDBND;

sfr TXCTL   = _TXCTL;
sfr FFCTL   = _FFCTL;
sfr FFIO    = _FFIO;
sfr STA0    = _STA0;
sfr STA1    = _STA1;
sfr MSK0    = _MSK0;
sfr MSK1    = _MSK1;
sfr FFSTA   = _FFSTA;
sfr RXCTL   = _RXCTL;
sfr MISC    = _MISC;
sfr PRLS    = _PRLS;
sfr PRLTX   = _PRLTX;
sfr GPF     = _GPF;

sfr I2CCMD  = _I2CCMD;
sfr OFS     = _OFS;
sfr DEC     = _DEC;
sfr PRLRXL  = _PRLRXL;
sfr PRLRXH  = _PRLRXH;
sfr TRXS    = _TRXS;
sfr REVID   = _REVID;
sfr I2CCTL  = _I2CCTL;
sfr I2CDEVA = _I2CDEVA;
sfr I2CMSK  = _I2CMSK;
sfr I2CEV   = _I2CEV;
sfr I2CBUF  = _I2CBUF;
sfr PCL     = _PCL;
sfr NVMIO   = _NVMIO;

sfr NVMCTL  = _NVMCTL;
sfr RWBUF   = _RWBUF;
sfr EXGP    = _EXGP;
sfr OSCCTL  = _OSCCTL;
sfr GPIOP   = _GPIOP;
sfr GPIOSL  = _GPIOSL;
sfr GPIOSH  = _GPIOSH;
sfr ATM     = _ATM;
sfr P0MSK   = _P0MSK;
sfr P0STA   = _P0STA;

sfr COMPI   = _COMPI;
sfr CMPSTA  = _CMPSTA;
sfr SRCCTL  = _SRCCTL;
sfr PWRCTL  = _PWRCTL;
sfr PWR_V   = _PWR_V;
sfr CCRX    = _CCRX;
sfr CCCTL   = _CCCTL;
sfr SRCTL   = _SRCTL;
sfr REGTRM0 = _REGTRM0;
sfr REGTRM1 = _REGTRM1;
sfr REGTRM2 = _REGTRM2;
sfr REGTRM3 = _REGTRM3;
sfr REGTRM4 = _REGTRM4;
sfr REGTRM5 = _REGTRM5;
sfr CCTRX   = 0xEF;

sfr DACCTL  = _DACCTL;
sfr DACEN   = _DACEN;
sfr SAREN   = _SAREN;
sfr SARLO   = _SARLO;
sfr SARUP   = _SARUP;
sfr DACLSB  = _DACLSB;
sfr DACV0   = _DACV0;
sfr DACV1   = _DACV1;
sfr DACV2   = _DACV2;
sfr DACV3   = _DACV3;
sfr DACV4   = _DACV4;
sfr DACV5   = _DACV5;
sfr DACV6   = _DACV6;
sfr DACV7   = _DACV7;


#endif // _CAN1110_REG_H_