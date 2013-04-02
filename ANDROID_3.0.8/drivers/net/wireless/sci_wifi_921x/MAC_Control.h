#ifndef __MAC_CONTROL_H
#define __MAC_CONTROL_H

#define HW_LINUX

typedef enum _WIRELESS_MODE {
    WIRELESS_MODE_A =  0x01,
    WIRELESS_MODE_B =  0x02,
    WIRELESS_MODE_G =  0x04,
    WIRELESS_MODE_BG = 0x06
} WIRELESS_MODE;

#define TRXCONFIG_VAL                0x07

#ifdef HW_WINTEST
#include <windows.h>
#define HW_WINDOWS
#define PRINT(s)                     MessageBox(NULL,L##s,NULL,0)
#define SLEEP(t)                     Sleep(t)
#define MALLOC(size)                 malloc(size)
#define MFREE(mem)                   free(mem)
#define HwPlatformIORead(A,B)        UsbReadReg(A,B)
#define HwPlatformIOWrite(A,B,C)     UsbWriteReg(A,B,C)
#endif

#ifdef HW_WINXP
#include <ndis.h>
#include "pch.h"
#define HW_WINDOWS
#define PRINT(s)                     DBGPRINT(DBG_INFO,(s))
#define SLEEP(t)                     NdisMSleep(t*1000)
#define MALLOC(size)                 ExAllocatePoolWithTag(NonPagedPool,size,SCIMP_POOL_TAG)
#define MFREE(mem)                   ExFreePool(mem)
#define HwPlatformIORead(A,B)        USBReadRegister((PRTMP_ADAPTER)A,B)
#define HwPlatformIOWrite(A,B,C)     RTUSBWriteRegister((PRTMP_ADAPTER)A,B,C)
#endif

#ifdef HW_WIN7
#include "hw_pcomp.h"
#define HW_WINDOWS
#define PRINT(s)                     MpTrace(COMP_INIT_PNP,DBG_SERIOUS,(s))
#define SLEEP(t)                     NdisStallExecution(t*1000)
#define MALLOC(size)                 ExAllocatePool(NonPagedPool,size)//MP_ALLOCATE_MEMORY(pNic->MiniportAdapterHandle,pNic,size,HW11_MEMORY_TAG)
#define MFREE(mem)                   ExFreePool(mem)                  //MP_FREE_MEMORY(mem)
#define HwPlatformIORead(A,B)        HwPlatformIORead4Byte((PNIC)A,B)
#define HwPlatformIOWrite(A,B,C)     HwPlatformIOWrite4Byte((PNIC)A,B,C)

#pragma warning(disable:4200)  // nameless struct/union
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int
#endif

#ifdef HW_LINUX
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/skbuff.h>


#define HANDLE                       UINT
#define DWORD                        unsigned int
#define PRINT(...)                     printk(KERN_INFO __VA_ARGS__)
#define SLEEP(t)                     msleep(t)
#define MALLOC(size)                 kzalloc(size,GFP_ATOMIC)
#define MFREE(mem)                   kfree(mem)
#define HwPlatformIORead(A,B)        sci921x_ioread32((struct sci9211_priv *)A,(__le32 *)B)
#define HwPlatformIOWrite(A,B,C)    sci921x_iowrite32((struct sci9211_priv *)A,(__le32 *)B,C)
#define UNREFERENCED_PARAMETER
#endif

#define VOID    void
#define MAX_LEN_OF_SHARE_KEY   16
#define BSS_TYPE     1
#define IBSS_TYPE    2

#define SYSCFG_MACEN     0x01
#define SYSCFG_BSSTYPE   0x02
#define SYSCFG_GADDREN   0x04
#define SYSCFG_JOINEN    0x08
#define SYSCFG_SWRST     0x10
#define SYSCFG_PSEN      0x20
#define SYSCFG_ASEL      0x00
#define SYSCFG_BSEL      0x80
#define SYSCFG_GSEL      0x40
#define SYSCFG_PHYSLEEP  0x100
#define SYSCFG_BGMIX     0x400
#define SYSCFG_RFSCI     0x1000
#define SYSCFG_RF7230    0x800
//#define SYSCFG_RFOTHER   0x1800

/*MAC Register*/
#define SYSTEMCONFIG           0X0500
#define WMM_KEYTYPE            0x050c
#define KEY0BASE               0X0510
#define KEY1BASE               0X0520
#define KEY2BASE               0X0530
#define KEY3BASE               0X0540
#define CAPINFO                0x0604
#define BCNINTERV              0X0608
#define ATIMPERIOD             0X060C
#define SUPRATELEN             0X0618
#define BCNPRSPLNG             0X061C
#define DSCHANNEL              0X0620
#define IFSREG                 0X0628
#define SUPRATESTART           0X062C
#define TXMODEBASE             0x0634
#define TXMODEOTHERS           0X0638
#define AIFSN                  0x0644
#define CWMIN                  0x0648
#define CWMAX                  0x064c
#define TXOPBEBK               0x0650
#define TXOPVOVI               0x0654
#define BSSID_ADDR0            0x0700
#define BSSID_ADDR1            0x0704
#define MAC_ADDR0              0x0718
#define MAC_ADDR1              0x071C
#define SSIDLEN                0X0720
#define SSIDSTART              0X0724
#define TSFBASE                0X0800
#define TBTTTIMER              0X0808
#define TXDELAY                0x0820
#define EXTRATE                0X082C
#define ERPINFO                0X0830
#define RFIC_Init              0x0838
#define TRXCONFIG              0x0900
#define POWMANAGEMENT          0x090c

/*PHY Register*/
#define ERRCOUNTENABLE         0x0B88
#define TXPOWER                0x0B3C
#define TXPOWDATAREADY         0x0B18
#define TSSIPOWER              0x0B44

/*EEPROM ADDR OFFSET*/
#define EEPROM_LOGO_OFFSET          0x00
#define EEPROM_MACADDR_OFFSET	      0x02
#define EEPROM_RFTYPE_OFFSET		    0x05
#define EEPROM_MACTYPE_OFFSET	      0x06
#define EEPROM_MODULETYPE_OFFSET	  0x07
//#define EEPROM_CALVALUE_OFFSET		  0x07
#define EEPROM_RF_BB_M_OFFSET	      0x08
#define EEPROM_VER_OFFSET	          0x0A
//#define EEPROM_CHANNELCONFIG_OFFSET   0x20/*0xB0*/

#define EEPROM_CH2_1_CAL_GAIN_OFFSET    0x20
#define EEPROM_CH4_3_CAL_GAIN_OFFSET    0x21
#define EEPROM_CH6_5_CAL_GAIN_OFFSET    0x22
#define EEPROM_CH8_7_CAL_GAIN_OFFSET    0x23
#define EEPROM_CH10_9_CAL_GAIN_OFFSET    0x24
#define EEPROM_CH12_11_CAL_GAIN_OFFSET    0x25
#define EEPROM_CH14_13_CAL_GAIN_OFFSET    0x26

#define EEPROM_TXPOWER_OFFSET    0x27
#define EEPROM_CH_1_DIFF_OFFSET   0x28
#define EEPROM_CH_7_DIFF_OFFSET   0x29
#define EEPROM_CH_13_14_DIFF_OFFSET   0x2A
#define EEPROM_CHANNEL_NOISE_OFFSET    0x2B
#define EEPROM_T_CAL_OFFSET    0x30
#define EEPROM_T_SAMPLE_OFFSET    0x31
#define EEPROM_TX_IQ_IMBALANCE_24G_OFFSET    0x34
#define EEPROM_COM_24G_RX_I_DC_OFFSET    0x35
#define EEPROM_COM_24G_RX_Q_DC_OFFSET    0x36

#define EEPROM_STATE_OFFSET	          0x3B
#define EEPROM_RSSI_REFERENCE_OFFSET	          0x3C

#define EEPROM_CH_2_3_DIFF_OFFSET   0x50
#define EEPROM_CH_4_5_DIFF_OFFSET   0x51
#define EEPROM_CH_6_8_DIFF_OFFSET   0x52
#define EEPROM_CH_9_10_DIFF_OFFSET   0x53
#define EEPROM_CH_11_12_DIFF_OFFSET   0x54



typedef unsigned char   UCHAR;
typedef unsigned int    UINT;
typedef unsigned short  USHORT;
typedef unsigned long   ULONG;

typedef enum _WIRELESS_RFTYPE {
    RFIC_MAX2829,      //A/B/G
    RFIC_MAX2830,      //B/G
    RFIC_AL7230,       //A/B/G
    RFIC_SCIS102,      //A/B/G
    RFIC_SCIS1022,     //A/B/G
    RFIC_SCIS1032      //B/G
} WIRELESS_RFTYPE;

typedef enum _WIRELESS_BASEBANDTYPE {
	BASEBAND_SCI9011,
	BASEBAND_SCI9012,
	BASEBAND_SCI9013
} WIRELESS_BASEBANDTYPE;

typedef struct _HW_CONTROL{
    UINT    rftype;
    UINT    phymode;
    HANDLE  hUSB;
    UINT    systemConfig;  //07252011 dmou for ioread32
    ULONG  hwChannelConfig[14][2];
} MAC_CONTROL, *PMAC_CONTROL;

typedef struct _CHANNELINFO{
    ULONG   RFType;
    ULONG   Addr;
    ULONG   ParamData[14][2];
} CHANNELINFO, *PCHANNELINFO;

#ifdef HW_WINDOWS
#pragma pack(1)
#endif

typedef struct  _HW_RXD_STRUC  {
    // Word 0
    ULONG       DataByteCnt:12;
    ULONG       Rsv0:4;
    ULONG       RxInfo:1;
    ULONG       TxInfo:1;
    ULONG       AtimInfo:1;
    ULONG       Rsv1:13;

    // word 1
    ULONG       RxRate:4;
    ULONG       PlcpRssi:8;         // RSSI reported by BBP
    ULONG       QosCtl_Up:3;
    ULONG       Rsv2:1;

    ULONG       Type :2;
    ULONG       Subtype :4;
    ULONG       MoreFrag :1;
    ULONG       Retry :1;
    ULONG       WEP :1;
    ULONG       Order :1;
    ULONG       WEP_CRCOK :1;
    ULONG       AES_MICOK :1;
    ULONG       RX_FRAG :4;

    // Word 2
    ULONG       FrameSequence:12;
    ULONG       TxResult:1;
    ULONG       RetryTxCount:6;
    ULONG       Rsv3:5;
    ULONG       AtimTxResult:1;
    ULONG       Rsv4:7;

    UCHAR       RxDA[6];
    UCHAR       RxSA[6];
#ifdef HW_WINDOWS
} HW_RXD_STRUC, *PHW_RXD_STRUC;
#endif
#ifdef HW_LINUX
}__attribute__((packed)) HW_RXD_STRUC, *PHW_RXD_STRUC;
#endif

typedef struct  _HW_TXD_STRUC {
    // word 0
    ULONG       DataByteCnt:12;
    ULONG       Reserved1:4;
    ULONG       DataTxRate:4;
    ULONG       RtsCtsTxRate:4;
    ULONG       Burst:1;            // 1: Contiguously used current End Ponit, eg, Fragment packet should turn on.
                                    //    Tell EDCA that the next frame belongs to the same "burst" even though TXOP=0
    ULONG       IsGroup:1;              // 1: ACK is required
    ULONG       NeedRts:1;
    ULONG       NeedCts:1;
    ULONG       Reserved2:4;
    // Word 1
    ULONG       LenEndBurst:12;
    ULONG       LengthBurst:12;
    ULONG       FragsBurst:4;
    ULONG       Reserved3:4;
    // Word 2
    ULONG       RtsCtsDur:16;
    ULONG       FrameSequence:12;
    ULONG       Reserved4:4;
    // Word 3
    ULONG       RtsDAL;
    // Word 4
    ULONG       RtsDAH:16;
    ULONG       QosCtl_Up:3;
    ULONG       QosCtl_Ack:2;
    ULONG       Reserved5:11;
    // Word 5
    ULONG       Type :2;
    ULONG       Subtype :4;
    ULONG       ToDs :1;
    ULONG       MoreFrag :1;
    ULONG       PwrMgt :1;
    ULONG       WEP :1;
    ULONG       Order :1;

    ULONG       Dur:16;
    ULONG       KeyId:2;
    ULONG       Reserved6:3;
#ifdef HW_WINDOWS
}   HW_TXD_STRUC, *PHW_TXD_STRUC;
#endif
#ifdef HW_LINUX
} __attribute__((packed))  HW_TXD_STRUC, *PHW_TXD_STRUC;
#endif

typedef struct  _EEPROM_STATE_STRUC {
    // word 0
    unsigned char       PRE_CAL_TXIQ_DONE:1;
    unsigned char       RX_POWER_CAL_DONE:1;
    unsigned char       TX_POWER_CAL_DONE:1;
    unsigned char       TARGET_POWER_DONE:1; 
#ifdef HW_WINDOWS
}   EEPROM_STATE_STRUC;
#endif
#ifdef HW_LINUX
} __attribute__((packed))  EEPROM_STATE_STRUC;
#endif

typedef struct _TXIQIMBALANCE{
    unsigned char COM_24G_P_TX;
    unsigned char COM_24G_G_TX;
} TXIQIMBALANCE;

typedef struct _RXDCIMBALANCE{
    short COM_24G_RX_I_DC;
    short COM_24G_RX_Q_DC;
} RXDCIMBALANCE;

typedef struct _CH_DIFF_POWER{
    int8_t CH1_DIFF_POWER; 
    int8_t CH2_DIFF_POWER; 
    int8_t CH3_DIFF_POWER; 
    int8_t CH4_DIFF_POWER; 
    int8_t CH5_DIFF_POWER; 
    int8_t CH6_DIFF_POWER; 
    int8_t CH7_DIFF_POWER; 
    int8_t CH8_DIFF_POWER; 
    int8_t CH9_DIFF_POWER; 
    int8_t CH10_DIFF_POWER;
    int8_t CH11_DIFF_POWER;
    int8_t CH12_DIFF_POWER;
    int8_t CH13_DIFF_POWER;
    int8_t CH14_DIFF_POWER;
} CH_DIFF_POWER;

typedef struct _CH_QAM64_GAIN{
    unsigned char CH1_QAM64_GAIN; 
    unsigned char CH2_QAM64_GAIN; 
    unsigned char CH3_QAM64_GAIN; 
    unsigned char CH4_QAM64_GAIN; 
    unsigned char CH5_QAM64_GAIN; 
    unsigned char CH6_QAM64_GAIN; 
    unsigned char CH7_QAM64_GAIN; 
    unsigned char CH8_QAM64_GAIN; 
    unsigned char CH9_QAM64_GAIN; 
    unsigned char CH10_QAM64_GAIN;
    unsigned char CH11_QAM64_GAIN;
    unsigned char CH12_QAM64_GAIN;
    unsigned char CH13_QAM64_GAIN;
    unsigned char CH14_QAM64_GAIN;

} CH_QAM64_GAIN;


#ifdef HW_WINDOWS
#pragma pack()
#endif

//typedef struct  _WMM_KEYTYPE_STRUC {
//    ULONG       PkeyType:3;
//    ULONG       GkeyType:3;
//    ULONG       EncryptEn:1;
//    ULONG       WmmEn:1;
//    ULONG       Reserved:24;
//} WMM_KEYTYPE_STRUC, *PWMM_KEYTYPE_STRUC;

#ifdef __cplusplus
extern "C"  {
#endif

//Function Prototype
PMAC_CONTROL HWInit(unsigned char moduleType,unsigned char RFType,unsigned char BBType,unsigned int PhyMode,unsigned short ver,HANDLE hUSB);
PMAC_CONTROL HWOpen(UINT RFType,UINT PhyMode,HANDLE hUSB);
VOID HWResetNIC(PMAC_CONTROL hw_control);
void HWInitPHY(PMAC_CONTROL hw_control,unsigned char moduleType,unsigned char RFType,unsigned char BBType,unsigned short ver);
void HWInitMAC(unsigned int PhyMode,unsigned char BBType,PMAC_CONTROL hw_control);
void HWInitRF(UINT RFType,UINT PhyMode,PMAC_CONTROL hw_control);
VOID HWClose(PMAC_CONTROL mcon);
VOID HWSwitchChannel(PMAC_CONTROL mcon,UINT channel);
#if 0
VOID MAX2829WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data);
VOID MAX2830WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data);
VOID AL7230WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data);
VOID S102WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data);
VOID S1022WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data);
#endif
VOID S1032WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data);
#if 0
VOID S1022AdjustingBF_BG(PMAC_CONTROL hw_control);
VOID S1022AdjustingLO_BG(PMAC_CONTROL hw_control);
VOID S1022AdjustingRX_BG(PMAC_CONTROL hw_control);
VOID S1022AdjustingTX_BG(PMAC_CONTROL hw_control);

VOID S1022AdjustingBF_A(PMAC_CONTROL hw_control);
VOID S1022AdjustingLO_A(PMAC_CONTROL hw_control);
VOID S1022AdjustingRX_A(PMAC_CONTROL hw_control);
VOID S1022AdjustingTX_A(PMAC_CONTROL hw_control);

VOID S1032AdjustingBF_BG(PMAC_CONTROL hw_control);
VOID S1032AdjustingLO_BG(PMAC_CONTROL hw_control);
VOID S1032AdjustingRX_BG(PMAC_CONTROL hw_control);
VOID S1032AdjustingTX_BG(PMAC_CONTROL hw_control);
#endif

VOID HWDbgPrintReg(PMAC_CONTROL hw_control);
VOID HWSetMacAddr(PMAC_CONTROL hw_control,UCHAR mac_addr[6]);
VOID HWSetBSSIDAddr(PMAC_CONTROL hw_control,UCHAR bssid[6]);
VOID HWSetKeyAddr(PMAC_CONTROL hw_control,UCHAR KeyIdx,UCHAR *Key);
VOID HWSetWmmKeyType(PMAC_CONTROL hw_control,UINT KeyType);
UINT HWGetWmmKeyType(PMAC_CONTROL hw_control);
VOID HWResetKeyAddr(PMAC_CONTROL hw_control,UCHAR KeyIdx);
UINT HWGetCapInfo(PMAC_CONTROL hw_control);
VOID HWSetCapInfo(PMAC_CONTROL hw_control,UINT val);
VOID HWSetBcnInerv(PMAC_CONTROL hw_control,UINT val);
VOID HWSetAtimPeriod(PMAC_CONTROL hw_control,UINT val);
VOID HWSetDsChannel(PMAC_CONTROL hw_control,UINT val);
VOID HWSetSsidLen(PMAC_CONTROL hw_control,UINT val);
UINT HWGetErpInfo(PMAC_CONTROL hw_control);
VOID HWSetErpInfo(PMAC_CONTROL hw_control,UINT val);
VOID HWSetBcnPrspLng(PMAC_CONTROL hw_control,UINT val);
VOID HWSetTbttTimer(PMAC_CONTROL hw_control,UINT low,UINT high);
UINT HWGetSystemConfig(PMAC_CONTROL hw_control);
VOID HWSetSystemConfig(PMAC_CONTROL hw_control,UINT val);
UINT HWGetTxRxConfig(PMAC_CONTROL hw_control);
VOID HWSetTxRxConfig(PMAC_CONTROL hw_control,UINT val);
UINT HWGetTsfBaseLow(PMAC_CONTROL hw_control);
UINT HWGetTsfBaseHig(PMAC_CONTROL hw_control);
VOID HWSetSsid(PMAC_CONTROL hw_control,UCHAR ssid_len,UCHAR *ssid);
VOID HWSetSuppRate(PMAC_CONTROL hw_control,UCHAR supp_rates_len,UCHAR *support_rate);
VOID HWSetExtRate(PMAC_CONTROL hw_control,UCHAR ext_rates_len,UCHAR *ext_rate);
void HWEEPROMWriteEnable(PMAC_CONTROL hw_control);
void HWEEPROMWriteDisable(PMAC_CONTROL hw_control);
void HWEEPROMWrite(PMAC_CONTROL hw_control,ULONG offset,ULONG value);
ULONG HWEEPROMRead(PMAC_CONTROL hw_control,ULONG offset);
//ULONG HWEEPROMReadAGC(PMAC_CONTROL hw_control);
int HWEEPROMReadMACAddr(PMAC_CONTROL hw_control, UCHAR *mac_addr,ULONG moduletype); //06072012 dmou
ULONG HWEEPROMReadRF_BB_M(PMAC_CONTROL hw_control);  //06072012 dmou
unsigned short HWEEPROMReadVER(PMAC_CONTROL hw_control);  //06072012 dmou
ULONG HWEEPROMReadRFType(PMAC_CONTROL hw_control);
ULONG HWEEPROMReadMACType(PMAC_CONTROL hw_control);
//ULONG HWEEPROMReadCALValue(PMAC_CONTROL hw_control);
//void HWMACWriteAGC(PMAC_CONTROL hw_control,UCHAR gain);
//int HWEEPROMReadChannelConfig(PMAC_CONTROL hw_control);
ULONG HWEEPROMReadModuleType(PMAC_CONTROL hw_control);
//VOID HWJoinEnable(PMAC_CONTROL hw_control,UINT CurrentBSSType);
//VOID HWCleanChannelConfig(PMAC_CONTROL HwControl);
VOID HWSetTXDelay(PMAC_CONTROL hw_control,UINT  bWep);
//VOID BeaconReceiveEnable(PMAC_CONTROL hw_control);
//VOID BeaconReceiveDisable(PMAC_CONTROL hw_control);

struct _EEPROM_STATE_STRUC HWEEPROMReadState(PMAC_CONTROL hw_control);
unsigned short HWEEPROMReadTXPower(PMAC_CONTROL hw_control,unsigned short ver);
unsigned char HWEEPROMReadNoise(PMAC_CONTROL hw_control,unsigned short ver);
struct _TXIQIMBALANCE HWEEPROMReadTXIQIMBALANCE(PMAC_CONTROL hw_control,unsigned short ver);
struct _RXDCIMBALANCE HWEEPROMReadRXDCIMBALANCE(PMAC_CONTROL hw_control,unsigned short ver);
struct _CH_QAM64_GAIN HWEEPROMReadCH_QAM64_GAIN(PMAC_CONTROL hw_control);
struct _CH_DIFF_POWER HWEEPROMReadCH_DIFF(PMAC_CONTROL hw_control,unsigned short ver);

#ifdef  __cplusplus
}
#endif

//#ifdef HW_WIN7
//#pragma warning(default:4200)
//#pragma warning(default:4201)
//#pragma warning(default:4214)
//

#endif    //#ifndef __MAC_CONTROL_H

