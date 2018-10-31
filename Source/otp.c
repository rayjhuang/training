
#include "global.h"

#define GENERIC_HEADER_BASE_ADDR  0x0900
#define TRIM_TABLE_BASE_ADDR      0x0940
#define OPTION_TABLE_BASE_ADDR    0x0960
#define PDO_TABLE_BASE_ADDR       0x0970
#define VDO_TABLE_BASE_ADDR       0x09E0
#define MAPVOL_TABLE_BASE_ADDR    0x0A20

BYTE code GenHeaderCP      [1][16]   _at_ GENERIC_HEADER_BASE_ADDR;
BYTE code GenHeaderFT      [1][16]   _at_ GENERIC_HEADER_BASE_ADDR+16;
// ?CO?OTP(0x920) in 'Code:' column of 'BL51 Locate' tab
// *** WARNING L16: UNCALLED SEGMENT, IGNORED FOR OVERLAY PROCESS
//     SEGMENT: ?CO?OTP
//// code GenHeaderFW      [1][16]   _at_ GENERIC_HEADER_BASE_ADDR+16+16;
BYTE code GenHeaderFW      [1][16] = {"CY2316" "CA" "V100" ".056"};
BYTE code GenHeaderWriter  [1][16]   _at_ GENERIC_HEADER_BASE_ADDR+16+16+16;
BYTE code TrimTableOTP     [6][5]    _at_ TRIM_TABLE_BASE_ADDR;
BYTE code OptionTableOTP   [3][4]    _at_ OPTION_TABLE_BASE_ADDR;
BYTE code PdoTableOTP      [3][7][4] _at_ PDO_TABLE_BASE_ADDR;
BYTE code MapVolTableOTP   [3][7][2] _at_ MAPVOL_TABLE_BASE_ADDR;
BYTE code VdoTableOTP      [3][4][4] _at_ VDO_TABLE_BASE_ADDR;


BYTE xdata* lpMemXdst;
BYTE xdata* lpMemXsrc;
BYTE code*  lpMemCsrc;

/* Tommy Add at 180726 (TS_Default_Fixed_PDO: init 8bits ADC to determine PDP) */
#ifdef CONFIG_TS_DEFAULT_PDP

extern BYTE code default_src_pdo[]; // Defined in otp_default_settings.c to prevent code address error

#define PDP_DEFAULT_VALUE   45
#define PDP_MINUS_VALUE_1   15
#define PDP_MINUS_VALUE_2   30


#define ADC_VALUE_TS_0V7    (87)    // Real Value to DACV_TS: 700/8
#define ADC_VALUE_TS_1V3    (162)
volatile BYTE u8OtpDefaultPowerSel; /* Tommy Add at 180726: defined default 6 src_pdo for */

#ifdef CONFIG_TS_DYNAMIC_PDP
volatile BYTE u8TsSelPri;
volatile BYTE u8TsTimer;
#define T_TS_DEBOUNCE       200

volatile BYTE bdata u8TsBits _at_ 0x29;
sbit bTsUpdateCurrentEvent = u8TsBits^7;

#endif

void OTP_InitTs(void)
{
    INIT_ANALOG_PK_SET_NTC();
    ENABLE_TS_SARADC();
    ENABLE_TS_CHANNEL();
    DACLSB = 0x04;
    DACCTL = 0x0f;
}

// Determine PDP by DACV_TS
void OTP_GetTsAdcValue(void)
{
#if TS_DETERMINE_PDP_COUNT >= 3
    if (DACV3 > ADC_VALUE_TS_1V3)
        u8OtpDefaultPowerSel = 0;
    else if (DACV3 > ADC_VALUE_TS_0V7)
        u8OtpDefaultPowerSel = 1;
    else
        u8OtpDefaultPowerSel = 2;
#elif TS_DETERMINE_PDP_COUNT >= 2
    if (DACV3 > ADC_VALUE_TS_1V3)
        u8OtpDefaultPowerSel = 0;
    else
        u8OtpDefaultPowerSel = 1;
#elif TS_DETERMINE_PDP_COUNT >= 1
    u8OtpDefaultPowerSel = 0;
#endif
}

#if (TS_DETERMINE_PDP_COUNT >= 2)
void OTP_ModifyFixedPdoCurrent(void)
{
    BYTE ii, type;
    WORD tmp, orgI, pwrI, pwrV;
    // To support captive cable, not sure max current => use orginal current as maximum

    for (ii = 0; ii < u8NumSrcPdoTab; ii++)
    {
        type = SRC_PDO[ii][3] & 0xC0;
        tmp = ( SRC_PDO[ii][1]<<8 | SRC_PDO[ii][0] );
        if (0x00 == type) // Fixed
        {
            pwrV = ((SRC_PDO[ii][2] & 0x0f) <<6) | (SRC_PDO[ii][1] >> 2);

            orgI = IS_OPTION_CAPT()? 500: 300;  // 5A:3A
            pwrI = ( u8SrcPDP * (2000 / pwrV) );
            if (pwrI > orgI) pwrI = orgI;
            tmp = ((tmp & 0xFC00) | pwrI);
        }
#if defined(CONFIG_PPS) && 0
        else    // 0xC0 APDO
        {
            pwrV = ((SRC_PDO[ii][3] & 0x01) <<7) | (SRC_PDO[ii][2] >> 1);

            orgI = IS_OPTION_CAPT()? 100: 60;   // 5A:3A
            pwrI = ( u8SrcPDP * 200 / pwrV);
            if (pwrI > orgI) pwrI = orgI;
            tmp = ((tmp & 0xFF00) | pwrI);
        }
#endif
        SRC_PDO[ii][0] = tmp;
        SRC_PDO[ii][1] = tmp >> 8;
    }
}
#endif  //(TS_DETERMINE_PDP_COUNT >= 2)

#endif  //CONFIG_TS_DEFAULT_PDP

void MemCopyX2X (BYTE cnt) { // 21-byte subroutine
   while (cnt--) {
      lpMemXdst[cnt] = lpMemXsrc[cnt];
   }
}

void MemCopyC2X (BYTE cnt) {
   while (cnt--) {
      lpMemXdst[cnt] = lpMemCsrc[cnt];
   }
}

BYTE MttGetVld (BYTE nByte, BYTE nEntry)
{
    BYTE i, j;

    for (i = 0; i < nEntry; i++)
    {
        for (j = 0; j < nByte; j++)
        {
            if (*(lpMemCsrc + j) != 0xFF)
            {
                goto SCAN_CONTINUOUS;
            }
        }
        goto SCAN_END;
SCAN_CONTINUOUS:
        lpMemCsrc += nByte;
    }
SCAN_END:
    lpMemCsrc -= nByte;
    return i;
}

void ReloadPdoTable(void)
{
    BYTE i;

    if (lpPdoTable) // PDO table exists
    {
        /* Gary Add at 180222 (TD.PD.VNDI3.E9 Source Capabilities Extended for Ellisys) */
        u8SrcPDP = ((*(PDO_V_DAC + 5) >> 8) & 0xF0) | (*(PDO_V_DAC + 6) >> 12);

        MEM_COPY_C2X((BYTE*)SRC_PDO, lpPdoTable, 7 << 2); // lpMemCsrc = lpPdoTable;

        /* Gary add at 20180604 (add variable for record PDO table) */
        u8NumSrcPdoTab = MttGetVld(4,7);
        u8NumSrcPdo = u8NumSrcPdoTab;
        /* Gary Add at 180517 (fixed bug: When PD3 transform to PD2, the PDO type is only Fixed) */
        u8NumSrcFixedPdo = 0;
        for (i = 0; i < u8NumSrcPdo; i++)
        {
            if ((*(*(SRC_PDO + i) + 3) >> 6) == 0x00)
                u8NumSrcFixedPdo++;
            /* Gary Add at 180629 2nd PDO mechanism bug(no default value for u8NumSrcFixedPdo) */
            else
                break;
        } /* Calculate the number of fixed PDOs */

        if (IS_OPTION_PDO2())
        {
            u8NumSrcPdo = 1;
        }
    }
    else // use default instead
    {
#ifdef CONFIG_TS_DEFAULT_PDP
        /* Tommy Add at 180726 (TS_Default_Fixed_PDO: defined default src_pdo for 6 sets) */
        BYTE code *ptr;
        u8NumSrcPdo = 6;
        u8NumSrcPdoTab = 6;
        u8NumSrcFixedPdo = 6;

        ptr = default_src_pdo;
        u8SrcPDP = PDP_DEFAULT_VALUE;

        MEM_COPY_C2X((BYTE*)SRC_PDO, ptr, 6 << 2);

#else // CONFIG_TS_DEFAULT_PDP: only 1 fixed PDO
        u8NumSrcPdo = 1;
        u8NumSrcFixedPdo = 1;
        SRC_PDO[0][0] = 0x2C;
        SRC_PDO[0][1] = 0x91;
        SRC_PDO[0][2] = 0x01;
        SRC_PDO[0][3] = 0x0A; // PDO1: Fixed 5V/3A, DRP
#endif
    }

#ifdef CONFIG_TS_DYNAMIC_PDP
    u8TsSelPri = 0;
#endif
}

#if defined(CONFIG_TS_DEFAULT_PDP) && (TS_DETERMINE_PDP_COUNT >= 2)
void UpdatePdoCurrent(void)
{
    signed char value;
#ifdef CONFIG_TS_DYNAMIC_PDP
    value = u8OtpDefaultPowerSel - u8TsSelPri;
#else
    value = u8OtpDefaultPowerSel;
#endif

    if (!value)
        return;

    if (value > 0)
    {
#if (TS_DETERMINE_PDP_COUNT >= 3)
        if (value == 2)
            u8SrcPDP -= (PDP_MINUS_VALUE_2);
        else
#endif
            u8SrcPDP -= (PDP_MINUS_VALUE_1);
    }
#ifdef CONFIG_TS_DYNAMIC_PDP // if not defined CONFIG_TS_DYNAMIC_PDP => always reload max pdp by attaching
    else
    {
#if (TS_DETERMINE_PDP_COUNT >= 3)
        if (value == -2)
            u8SrcPDP += (PDP_MINUS_VALUE_2);
        else
#endif
            u8SrcPDP += (PDP_MINUS_VALUE_1);
    }
#endif

    OTP_ModifyFixedPdoCurrent();

#ifdef CONFIG_TS_DYNAMIC_PDP
    u8TsSelPri = u8OtpDefaultPowerSel;
#endif
}
#endif

void MtTablesInit (void)
{
#ifdef CONFIG_TS_DEFAULT_PDP
    /* Tommy Add at 180726 (TS_Default_Fixed_PDO: init 8bits ADC to determine PDP) */
    OTP_InitTs();
#endif

   /* trim table */
    lpMemCsrc = TRIM_TABLE_BASE_ADDR;
    if (MttGetVld(5,6)) // find trim table
    {
        REGTRM0 = lpMemCsrc[0];
        REGTRM1 = lpMemCsrc[1];
        REGTRM2 = lpMemCsrc[2];
        REGTRM3 = lpMemCsrc[3];
        REGTRM4 = lpMemCsrc[4];
   }

   /* DAC0/PWR_V mapping table */
   lpMemCsrc = MAPVOL_TABLE_BASE_ADDR;
   if (MttGetVld(7 << 1, 3)) // find PDO_V_DAC
   {
      lpMemXdst = (BYTE*)PDO_V_DAC; // to xdata
      MemCopyC2X(7 << 1);
   }
   else // use default instead
   {
#ifdef CONFIG_TS_DEFAULT_PDP
        PDO_V_DAC[0] = 0x10FA;  // 5V
        PDO_V_DAC[1] = 0x51C2;  // 9V
        PDO_V_DAC[2] = 0x0258;  // 12V
        PDO_V_DAC[3] = 0x12EE;  // 15V
        PDO_V_DAC[4] = 0xC384;  // 18V
        PDO_V_DAC[5] = 0x33E8;  // 20V
#else
        PDO_V_DAC[0] = 0x10FA;  // 5V
#endif
   }
   
   /* Gary Add at 180222 (TD.PD.VNDI3.E9 Source Capabilities Extended for Ellisys) */
    //u8SrcPDP = ((*(PDO_V_DAC + 5) >> 8) & 0xF0) | (*(PDO_V_DAC + 6) >> 12);
    //if (u8SrcPDP>100) u8SrcPDP = 100;

   /* PDO table */
   lpMemCsrc = PDO_TABLE_BASE_ADDR;
   if (MttGetVld(7 << 2, 3)) // PDO table found
   {
      lpPdoTable = lpMemCsrc;
   }
   else
   {
      lpPdoTable = 0;
   }
   ReloadPdoTable();
#ifdef CONFIG_TS_DEFAULT_PDP
    OTP_GetTsAdcValue();    /* Tommy Add at 180726 (TS_Default_Fixed_PDO: defined default src_pdo for 6 sets) */

#if (TS_DETERMINE_PDP_COUNT >= 2)
    UpdatePdoCurrent();
#endif  // (TS_DETERMINE_PDP_COUNT >= 2)

#endif  // CONFIG_TS_DEFAULT_PDP

   /* option table */
   lpMemCsrc = OPTION_TABLE_BASE_ADDR;
   if (MttGetVld(4, 3)) // find OPTION
   {
      lpMemXdst = (BYTE*)(&OPTION_REG);
      MemCopyC2X(4);
   }
   else
   {
      OPTION_REG[0] = OPTION_D4_0;
      OPTION_REG[1] = OPTION_D4_1;
      OPTION_REG[2] = OPTION_D4_2;
      OPTION_REG[3] = OPTION_D4_3;
   }

    /* VDO table */
    lpMemCsrc = VDO_TABLE_BASE_ADDR;
    if (MttGetVld(4 << 2, 3) // find VDM Data Object
        && (lpMemCsrc[0] | lpMemCsrc[1])) // check ID Header, cannot be 0x0000, 0xFFFF (which mean erased)
    {
        lpVdmTable = lpMemCsrc;
        u8NumVDO = MttGetVld(4,4);
    }
    else
    {
        lpVdmTable = 0;
        u8NumVDO = 0;
    }
}

#ifdef CONFIG_TS_DYNAMIC_PDP
void TsDetectTmr1(void)
{
    if (!bTCAttch) bTsUpdateCurrentEvent = 0;
    if (GET_COMP_SWITCH_VAL() || bTsUpdateCurrentEvent) return;

    OTP_GetTsAdcValue();

    if (u8TsSelPri != u8OtpDefaultPowerSel)
    {
        if (++u8TsTimer && u8TsTimer > T_TS_DEBOUNCE)
        {
            bTsUpdateCurrentEvent = 1;
            u8TsTimer = 0;
        }
    }
    else
    {
        u8TsTimer = 0;
    }
}
#endif // CONFIG_TS_DYNAMIC_PDP