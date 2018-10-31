#ifndef _CAN1110_CONFIGURE_H_
#define _CAN1110_CONFIGURE_H_

//#define CONFIG_SIMULATION
//#define CONFIG_FPGA
#define CONFIG_PWM  // enable PWM function
#define CONFIG_CAN1110C0
//#define CONFIG_SRC_CAP_COUNT
//#define CONFIG_PPS

#define CONFIG_SUPPORT_QC // QC function

/* default PDP by TS voltage */
//#define CONFIG_TS_DEFAULT_PDP

#define CONFIG_SUPPORT_EMARKER

#ifdef CONFIG_SRC_CAP_COUNT
    #define PE_SRC_CAP_COUNT        50
#endif

#ifdef CONFIG_SUPPORT_QC
    #define CONFIG_QC3 // enable QC3 function

    #if ((defined CONFIG_PPS) && (defined CONFIG_QC3))
        #define CONFIG_QC4 // enable QC4 function
    #endif
#endif /* Support QC function */

#ifdef CONFIG_TS_DEFAULT_PDP
    #define TS_DETERMINE_PDP_COUNT   2

    #if (TS_DETERMINE_PDP_COUNT < 1) || (TS_DETERMINE_PDP_COUNT > 3)
        #error Please set PDP count 1 to 3!
    #endif

    #if (TS_DETERMINE_PDP_COUNT >= 2)
        #define CONFIG_TS_DYNAMIC_PDP
    #endif
#endif /* CONFIG_TS_DEFAULT_PDP */


#endif // _CAN1110_CONFIGURE_H_



