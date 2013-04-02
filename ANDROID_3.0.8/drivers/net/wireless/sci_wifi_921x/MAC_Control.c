#include "MAC_Control.h"
#include "sci9211.h"
#include "sci921x.h"

/*CHANNELINFO ChannelInfo[4]={{RFIC_MAX2829,0x0a00,
                             0x80000A03,0x80033334,0x80020A13,0x80008884,0x80030A13,0x8001DDD4,0x80000A13,0x80033334,
                             0x80020A23,0x80008884,0x80030A23,0x8001DDD4,0x80000A23,0x80033334,0x80020A33,0x80008884,
                             0x80030A33,0x8001DDD4,0x8001DDD4,0x80033334,0x80020A43,0x80008884,0x80030A43,0x8001ddd4,
                             0x80000A43,0x80033334},

                            {RFIC_MAX2830,0x0a00,
                             0x8001A783,0x80026664,0x8001A783,0x80036664,0x8001A793,0x80006664,0x8001A793,0x80016664,
                             0x8001A793,0x80026664,0x8001A793,0x80036664,0x8001A7A3,0x80006664,0x8001A7A3,0x80016664,
                             0x8001A7A3,0x80026664,0x8001A7A3,0x80036664,0x8001A7B3,0x80006664,0x8001A7B3,0x80016664,
                             0x8001A7B3,0x80026664},

                            {RFIC_AL7230,0x0a04,
                             0x80003790,0x80133331,0x80003790,0x801B3331,0x80003790,0x80033331,0x80003790,0x800B3331,
                             0x800037A0,0x80133331,0x800037A0,0x801B3331,0x800037A0,0x80033331,0x800037A0,0x800B3331,
                             0x800037B0,0x80133331,0x800037B0,0x801B3331,0x800037B0,0x80033331,0x800037B0,0x800B3331,
                             0x800037C0,0x80133331},

                            {RFIC_SCIS102,0x0a00,
                             0x800028E3,0x80013334,0x800028E3,0x8001B334,0x800028E3,0x80023334,0x800028E3,0x8002B334,
                             0x800028E3,0x80033334,0x800028E3,0x8003B334,0x80002CE3,0x80003334,0x80002CE3,0x8000B334,
                             0x80002CE3,0x80013334,0x80002CE3,0x8001B334,0x80002CE3,0x80023334,0x80002CE3,0x8002B334,
                             0x80002CE3,0x80033334}
};*/
static UCHAR SCIFlag = 0;
ULONG HWChannelConfig[14][2];

/*
VOID HWCleanChannelConfig(PMAC_CONTROL HwControl)
{
    UINT         Channel;

    for(Channel=0;Channel<14;Channel++)
    {
        HWChannelConfig[Channel][0] = 0;
        HWChannelConfig[Channel][1] = 0;
    }
}*/

PMAC_CONTROL HWInit(unsigned char moduleType,
                    unsigned char RFType,
                    unsigned char BBType,
                    unsigned int PhyMode,
                    unsigned short ver,
                    HANDLE hUSB)
{
    PMAC_CONTROL HwControl;

    HwControl = HWOpen(RFType,PhyMode,hUSB);
    HWResetNIC(HwControl);
    HWInitMAC(PhyMode,BBType,HwControl);
    HWInitPHY(HwControl,moduleType,RFType,BBType,ver);
    HWInitRF(RFType,PhyMode,HwControl);

    return HwControl;
}

PMAC_CONTROL HWOpen(UINT RFType,UINT PhyMode,HANDLE hUSB)
{
    PMAC_CONTROL HwControl;

    HwControl = (PMAC_CONTROL)MALLOC(sizeof(MAC_CONTROL));

    if (!HwControl)
    {
        PRINT("Open Error: Out of memory allocating private data\n");
        return NULL;
    }

    HwControl->hUSB = hUSB;
    HwControl->phymode = PhyMode;
    HwControl->rftype = RFType;

    return HwControl;
}

VOID HWResetNIC(PMAC_CONTROL hw_control)
{
    ULONG word=0;
    HANDLE hUSB = hw_control->hUSB;

	word &= ~SYSCFG_MACEN;
	HwPlatformIOWrite(hUSB,SYSTEMCONFIG,word);
	SLEEP(20);

    word |= SYSCFG_SWRST;
    HwPlatformIOWrite(hUSB,SYSTEMCONFIG,word);
}

/*dmou add for new driver_init*/
void HWInitPHY(PMAC_CONTROL hw_control,unsigned char moduleType,unsigned char RFType,unsigned char BBType,unsigned short ver)
{
    HANDLE hUSB = hw_control->hUSB;
    unsigned int ret;
    struct _CH_QAM64_GAIN CHQAM64Gain;
    struct _RXDCIMBALANCE RxDCImbalance;
    struct _TXIQIMBALANCE TXIQImbalance;

    /*befor set PHY reg 
     SET "0X090C" PHY_REG_CLK_EN "1"*/
    ret = HwPlatformIORead(hUSB,0x090C);
    ret |= 0x00100000;
    HwPlatformIOWrite(hUSB,0x090C,ret);

    //different reg set for different rf_bb_m_type 
    if((moduleType == 0x001B) ||( moduleType == 0x001C))
    {
        ret = HwPlatformIORead(hUSB,0x0B00);
        ret &= 0xFF80FFFF;
#if MORE_AP		
        ret |= 0x00600000; //0x00680000
#else
        ret |= 0x00600000;
#endif
        HwPlatformIOWrite(hUSB,0x0B00,ret);  //R_ED_CTL_WORD "0X68"

        ret = HwPlatformIORead(hUSB,0x0B40);
        ret &= 0x80808080;
        CHQAM64Gain = HWEEPROMReadCH_QAM64_GAIN(hw_control);
        ret |= 0x00686256; 
        ret |= (CHQAM64Gain.CH1_QAM64_GAIN << 24);
        HwPlatformIOWrite(hUSB,0x0B40,ret);  

        ret = HwPlatformIORead(hUSB,0X0B4C);
        ret &= 0xFFFFE0FF;
        ret |= 0x00000E00; 
        HwPlatformIOWrite(hUSB,0X0B4C,ret);  //POWER_THRED"0X0E"

        ret = HwPlatformIORead(hUSB,0X0E68);
        ret &= 0x80808080;
        CHQAM64Gain = HWEEPROMReadCH_QAM64_GAIN(hw_control);
        ret |= (CHQAM64Gain.CH1_QAM64_GAIN << 24);
        ret |= (CHQAM64Gain.CH1_QAM64_GAIN << 16);
        ret |= (CHQAM64Gain.CH1_QAM64_GAIN << 8);
        ret |= CHQAM64Gain.CH1_QAM64_GAIN;
        HwPlatformIOWrite(hUSB,0X0E68,ret);  
    }
    else
    {
        ret = HwPlatformIORead(hUSB,0x0B00);
        ret &= 0xFF80FFFF;
        ret |= 0x00700000; 
        HwPlatformIOWrite(hUSB,0x0B00,ret);  //R_ED_CTL_WORD "0X70"

        ret = HwPlatformIORead(hUSB,0x0B40);
        ret &= 0x80808080;
        ret |= 0x305A5A5A; 
        HwPlatformIOWrite(hUSB,0x0B40,ret);  

        ret = HwPlatformIORead(hUSB,0X0B4C);
        ret &= 0xFFFFE0FF;
        ret |= 0x00001700; 
        HwPlatformIOWrite(hUSB,0X0B4C,ret);  //POWER_THRED"0X17"

        ret = HwPlatformIORead(hUSB,0X0E68);
        ret &= 0x80808080;
        ret |= 0x30303030;
        HwPlatformIOWrite(hUSB,0X0E68,ret); 
    }

    if(RFType == RFIC_SCIS1032)
    {
        ret = HwPlatformIORead(hUSB,0X0E60);
        ret &= 0xFFFF00FF;
        ret |= 0x0100;
        HwPlatformIOWrite(hUSB,0X0E60,ret);  //XHP_CHANGE_TIME1 "0X01"
    }
    else
    {
        ret = HwPlatformIORead(hUSB,0X0E60);
        ret &= 0xFFFF00FF;
        ret |= 0x2800;
        HwPlatformIOWrite(hUSB,0X0E60,ret);  //XHP_CHANGE_TIME1 "0X28"
    }

    if(BBType == BASEBAND_SCI9013 )
    {
        ret = HwPlatformIORead(hUSB,0X0E88);
        ret &= 0xFCFCFFFF;
        ret |= 0x03010000;
        HwPlatformIOWrite(hUSB,0X0E88,ret);  //INITIAL_SEL "0X3" SIG_ERROR_METHOD "0X1"
    }
    else if( BBType == BASEBAND_SCI9012 )
    {
        ret = HwPlatformIORead(hUSB,0X0E88);
        ret &= 0xFCFCF8FF;
        ret |= 0x03010400;
        HwPlatformIOWrite(hUSB,0X0E88,ret);  //INITIAL_SEL "0X3" SIG_ERROR_METHOD "0X1" TX_IQ_MATCH"0X4"
    }
    else
    {
        ret = HwPlatformIORead(hUSB,0X0E88);
        ret &= 0xFCFCFFFF;
        ret |= 0x03010000;
        HwPlatformIOWrite(hUSB,0X0E88,ret);  //INITIAL_SEL "0X3" SIG_ERROR_METHOD "0X1"
    }

    //common reg set (no relation with rf_bb_m_type)
    RxDCImbalance = HWEEPROMReadRXDCIMBALANCE(hw_control,ver);
    ret = HwPlatformIORead(hUSB,0X0B54);
    ret &= 0xFC00FC00;
    ret |= RxDCImbalance.COM_24G_RX_I_DC;
    ret |= (RxDCImbalance.COM_24G_RX_Q_DC << 16);
    HwPlatformIOWrite(hUSB,0X0B54,ret);  //Q_RX_DC_COM_2G " COM_24G_RX_Q_DC " I_RX_DC_COM_2G " COM_24G_RX_I_DC "

    TXIQImbalance = HWEEPROMReadTXIQIMBALANCE(hw_control,ver);
    ret = HwPlatformIORead(hUSB,0X0B60);
    ret &= 0xFFFF0000;
    ret |= TXIQImbalance.COM_24G_P_TX;
    ret |= (TXIQImbalance.COM_24G_G_TX << 8);
    HwPlatformIOWrite(hUSB,0X0B60,ret);  //P_TX_COM_2G "COM_24G_P_TX" G_TX_COM_2G "COM_24G_G_TX"
   
    ret = HwPlatformIORead(hUSB,0X0B88);
    //ret &= 0xFFFFFFFD;
    ret |= 0x00000002;
    HwPlatformIOWrite(hUSB,0X0B88,ret);  //SC_RE_EN "1"

    ret = HwPlatformIORead(hUSB,0X0E44);
    ret &= 0xFC00FC00;
    ret |= 0x00840084;
    HwPlatformIOWrite(hUSB,0X0E44,ret);  //ED_THRESHOLD1 "0X084" ED_THRESHOLD2 "0X084"

    ret = HwPlatformIORead(hUSB,0X0E74);
    ret &= 0xFFFFFF0F;
    ret |= 0x000000C0;
    HwPlatformIOWrite(hUSB,0X0E74,ret);  //FS_DEC_METHOD "0XC"

    ret = HwPlatformIORead(hUSB,0X0E94);
    ret &= 0xFFFFFFFE;
    HwPlatformIOWrite(hUSB,0X0E94,ret);  //SIG_MONITOR_SEL "0X0"

    /*end of PHY reg set
    set "0X090C" PHY_REG_CLK_EN "0" */
    ret = HwPlatformIORead(hUSB,0x090C);
    ret &= 0xFFEFFFFF;
    HwPlatformIOWrite(hUSB,0x090C,ret);
}

void HWInitMAC(unsigned int PhyMode,unsigned char BBType,PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    unsigned int ret;

    HwPlatformIOWrite(hUSB,0x0900,0x00000037/*0x00000037*/);   //sniff=1 @ 0x00000047
    ret = HwPlatformIORead(hUSB,0x090C);
    ret |= 0x6C10;
    ret &= 0xFFFFFFDF;
    HwPlatformIOWrite(hUSB,0x090C,ret);
    
    HwPlatformIOWrite(hUSB,0x081C,0x80008000);
    HwPlatformIOWrite(hUSB,0x0820,0x62284014);  //changed to 62xxxxx for TKIP
    HwPlatformIOWrite(hUSB,0x0824,0x01140101);
    HwPlatformIOWrite(hUSB,0x0828,0x0B0B0B0B);
    HwPlatformIOWrite(hUSB,0x0834,0x00400101);
    HwPlatformIOWrite(hUSB,0x063C,0x00000105);

    if(BBType == BASEBAND_SCI9012)
    {
        HwPlatformIOWrite(hUSB,0x0844,0x00000001);
    }

    ret = HwPlatformIORead(hUSB,0x0500);
    ret = 0x00000007;
    HwPlatformIOWrite(hUSB,0x0500,ret);
		
    switch(PhyMode)
    {
        case WIRELESS_MODE_B:
        {
            ret = HwPlatformIORead(hUSB,0x0500);
            ret |= 0x00000080;
            ret &= 0xFFFFFFBF;
            HwPlatformIOWrite(hUSB,0x0500,ret);
						
            HwPlatformIOWrite(hUSB,IFSREG,0x282D8645);//11b
            HwPlatformIOWrite(hUSB,TXMODEBASE,0x00000000);//11ag
            HwPlatformIOWrite(hUSB,TXMODEOTHERS,0x00000000);//11ag
            HwPlatformIOWrite(hUSB,0x0624,0x00008352);

            break;
        }
        case WIRELESS_MODE_G/*PHY_11BG_MIXED*/:
        {
            ret = HwPlatformIORead(hUSB,0x0500);
            ret |= 0x00000440;
            ret &= 0xFFFFFD7F;
            HwPlatformIOWrite(hUSB,0x0500,ret);
            
            HwPlatformIOWrite(hUSB,IFSREG,0x282d8645);//11b
            HwPlatformIOWrite(hUSB,TXMODEBASE,0x00000000);//11Bg  for ad-hoc beacon IE(DS)
            HwPlatformIOWrite(hUSB,TXMODEOTHERS,0x0000000B);//11ag
            HwPlatformIOWrite(hUSB,0x0624,0x00008352);    
            break;
        }
        case WIRELESS_MODE_A:
        {
        	ret = HwPlatformIORead(hUSB,0x0500);
            ret &= 0xFFFFFF3F;
            HwPlatformIOWrite(hUSB,0x0500,ret);
						
            HwPlatformIOWrite(hUSB,IFSREG,0x120BC441);//11a
            HwPlatformIOWrite(hUSB,TXMODEBASE,0x0000000B);//11ag
            HwPlatformIOWrite(hUSB,TXMODEOTHERS,0x0000000B);//11ag

            break;
        }
        default :
        {
            PRINT("Error PhyMode\n");
            return;
        }
    }
}


void HWInitRF(UINT RFType,UINT PhyMode,PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    unsigned int ret;

    /*step 1 Init default control bit*/
    HwPlatformIOWrite(hUSB,0x0838,0x00000020);  //"INIT_MODE" "1"

    ret = HwPlatformIORead(hUSB,SYSTEMCONFIG);
    ret &= 0xFFFFE7FF;
    ret |= 0x00000800;   //"0X0500" "RF_TYPE" = "01"
    HwPlatformIOWrite(hUSB,SYSTEMCONFIG,ret); 
    
    if(RFType == RFIC_SCIS1032)
    {
         //init rf based on different rf_type
        HwPlatformIOWrite(hUSB,0x0A04,0x80200000);  //OPEN 1.2V
        HwPlatformIOWrite(hUSB,0x0A04,0x803F0FE1);
        HwPlatformIOWrite(hUSB,0x0A04,0x80008832);
        HwPlatformIOWrite(hUSB,0x0A04,0x800008E3);

        HwPlatformIOWrite(hUSB,0x0A04,0x80593334);
        HwPlatformIOWrite(hUSB,0x0A04,0x8073FFB5);
        HwPlatformIOWrite(hUSB,0x0A04,0x8019B8F6);
        HwPlatformIOWrite(hUSB,0x0A04,0x800098F7);

        HwPlatformIOWrite(hUSB,0x0A04,0x800E91C8);
        HwPlatformIOWrite(hUSB,0x0A04,0x800740F9);  //open temperture sensor
        HwPlatformIOWrite(hUSB,0x0A04,0x80014CCA);
        HwPlatformIOWrite(hUSB,0x0A04,0x80126EDB);

        HwPlatformIOWrite(hUSB,0x0A04,0x800B403C);
        HwPlatformIOWrite(hUSB,0x0A04,0x8088000D);
        HwPlatformIOWrite(hUSB,0x0A04,0x8010000E);
        HwPlatformIOWrite(hUSB,0x0A04,0x8000280F);

        /*  //Enable Qos  //added by hzzhou on April6,2012
        {
            unsigned int val;
            val = HWGetWmmKeyType(hw_control);
            val |= 0x80;//turn on the WMM_EN bit
            HWSetWmmKeyType(hw_control,val);
        }*/

         /*STEP 2 : Adjust LO LEAKAGE and BF*/
         HwPlatformIOWrite(hUSB,0x0838,0x00000021);  //"RFINIT_SHDN" = "1"

         ret = HwPlatformIORead(hUSB,0x090C);
         ret &= 0xFFFFFFFD;
         HwPlatformIOWrite(hUSB,0x090C,ret);  //"MAC_GATE_EN" = "0"
         HwPlatformIOWrite(hUSB,0x0A04,0x80200080);
         SLEEP(50);

         HwPlatformIOWrite(hUSB,0x0A04,0x80200000);  //adjust BF
         HwPlatformIOWrite(hUSB,0X0838,0x00000025);  //"RFINIT_TX_EN" = "1"
         HwPlatformIOWrite(hUSB,0x0A04,0x80200010); 
         SLEEP(50);

         HwPlatformIOWrite(hUSB,0x0A04,0x80200000);  //adjust LO LEAKAGE
         ret = HwPlatformIORead(hUSB,0x090C);
         ret |= 0x2;
         HwPlatformIOWrite(hUSB,0x090C,ret);  //"MAC_GATE_EN" = "1"
         HwPlatformIOWrite(hUSB,0x0838,0x00000001);  //"RFINIT_TX_EN"="0"，"INIT_MODE"="0"
     }
     else
        PRINT("SCI.error: RF IC NO SUPPORT ! \n");
}

VOID HWClose(PMAC_CONTROL mcon)
{
	if(mcon)
	{
		MFREE(mcon);
	}
}

DWORD s1022conf24G20M[][3] =
{
    {0x009D,0x0A666,0x586},
    {0x009D,0x0B666,0x586},
    {0x009D,0x0C666,0x586},
    {0x009D,0x0D666,0x586},
    {0x009D,0x0E666,0x587},
    {0x009D,0x0F666,0x587},

    {0x009E,0x00666,0x587},
    {0x009E,0x01666,0x587},
    {0x009E,0x02666,0x588},
    {0x009E,0x03666,0x588},
    {0x009E,0x04666,0x588},
    {0x009E,0x05666,0x588},

    {0x009E,0x06666,0x589},
    {0x009E,0x08CCC,0x589},
};

DWORD s1022conf24G40M[][3] =
{
    {0x008E,0x09333,0x586},
    {0x008E,0x09B33,0x586},
    {0x008E,0x0A333,0x586},
    {0x008E,0x0AB33,0x586},
    {0x008E,0x0B333,0x587},
    {0x008E,0x0BB33,0x587},
    {0x008E,0x0C333,0x587},
    {0x008E,0x0CB33,0x587},
    {0x008E,0x0D333,0x588},
    {0x008E,0x0DB33,0x588},
    {0x008E,0x0E333,0x588},
    {0x008E,0x0EB33,0x588},
    {0x008E,0x0F333,0x589},
    {0x008F,0x00666,0x589},
};


DWORD s1032conf24G20M[][3] =
{
    {0x009D,0x5A666,0x1BB8F},
    {0x009D,0x5B666,0x1BB8F},

    {0x009D,0x5C666,0x1BB8F},
    {0x009D,0x5D666,0x1BB8F},
    {0x009D,0x5E666,0x1BB8F},
    {0x009D,0x5F666,0x1BB8F},
    {0x009E,0x50666,0x1BB8F},
    {0x009E,0x51666,0x1BB8F},
    {0x009E,0x52666,0x1BB8F},
    {0x009E,0x53666,0x1BB8F},
    {0x009E,0x54666,0x1BB8F},
    {0x009E,0x55666,0x1BB8F},
    {0x009E,0x56666,0x1BB8F},
    {0x009E,0x58CCC,0x1BB8F},
};

DWORD s1032conf24G26M[][3] =
{
    {0x0096,0x5B13B,0x1BB8F},
    {0x0096,0x5BD89,0x1BB8F},
    {0x0096,0x5C9D8,0x1BB8F},
    {0x0096,0x5D627,0x1BB8F},
    {0x0096,0x5E276,0x1BB8F},
    {0x0096,0x5EEC4,0x1BB8F},
    {0x0096,0x5FB13,0x1BB8F},
    {0x0096,0x50762,0x1BB8F},
    {0x0097,0x513B1,0x1BB8F},
    {0x0097,0x52000,0x1BB8F},
    {0x0097,0x52C4E,0x1BB8F},
    {0x0097,0x5389D,0x1BB8F},
    {0x0097,0x544EC,0x1BB8F},
    {0x0097,0x55D89,0x1BB8F},
};

DWORD s1032conf24G40M[][3] =
{
    {0x008E, 0x59333, 0x19B89/*0x1BB89*/},
    {0x008E, 0x59B33, 0x19B89/*0x1BB89*/},
	{0x008E, 0x5A333, 0x19B89/*0x1BB89*/},
    {0x008E, 0x5AB33, 0x19B89/*0x1BB89*/},
    {0x008E, 0x5B333, 0x19B8A/*0x1BB8A*/},
    {0x008E, 0x5BB33, 0x19B8A/*0x1BB8A*/},
    {0x008E, 0x5C333, 0x19B8B/*0x1BB8B*/},
    {0x008E, 0x5CB33, 0x19B8B/*0x1BB8B*/},
    {0x008E, 0x5D333, 0x19B8C/*0x1BB8C*/},
    {0x008E, 0x5DB33, 0x19B8C/*0x1BB8C*/},
    {0x008E, 0x5E333, 0x19B8D/*0x1BB8D*/},
    {0x008E, 0x5EB33, 0x19B8D/*0x1BB8D*/},
	{0x008E, 0x5F333, 0x19B8E/*0x1BB8E*/},
    {0x008F, 0x50666, 0x19B8F/*0x1BB8F*/},
};


DWORD s1022conf5G40MChn[] =
{
    34,
    36,
    38,
    40,
    42,
    44,
    46,
    48,
    52,
    56,
    60,
    64,
    100,
    104,
    108,
    112,
    116,
    120,
    124,
    128,
    132,
    136,
    140,
    149,
    153,
    157,
    161,
};

DWORD s1022conf5G40M[][3] =
{
    {0x000A,0x0555,0x1354F},            //34,
    {0x000A,0x0AAA,0x1354F},            //36,
    {0x000A,0x1000,0x1354F},            //38,
    {0x000A,0x1555,0x1354F},            //40,
    {0x000A,0x1AAA,0x1354F},            //42,
    {0x000A,0x2000,0x1354F},            //44,
    {0x000A,0x2555,0x1354F},            //46,
    {0x000A,0x2AAA,0x1354F},            //48,
    {0x000A,0x3555,0x1354F},            //52,
    {0x000A,0x0000,0x1354F},            //56,
    {0x000A,0x0AAA,0x1354F},            //60,
    {0x000A,0x1555,0x1354F},            //64,
    {0x000A,0x3555,0x1754F},            //100,
    {0x000B,0x0000,0x1754F},            //104,
    {0x000B,0x0AAA,0x1754F},            //108,
    {0x000B,0x1555,0x1754F},            //112,
    {0x000B,0x2000,0x1754F},            //116,
    {0x000B,0x2AAA,0x1754F},            //120,
    {0x000B,0x3555,0x1754F},            //124,
    {0x000B,0x4000,0x1754F},            //128,
    {0x000B,0x4AAA,0x1754F},            //132,
    {0x000B,0x5555,0x1754F},            //136,
    {0x000B,0x6000,0x1754F},            //140,
    {0x000B,0x7800,0x1B54F},            //149,
    {0x000B,0x82AA,0x1B54F},            //153,
    {0x000B,0x8D55,0x1B54F},            //157,
    {0x000B,0x9800,0x1B54F},            //161,
};

VOID HWSwitchChannel(PMAC_CONTROL mcon,UINT Channel)
{
     PMAC_CONTROL hw_control = mcon;
     //HANDLE hUSB = mcon->hUSB;

     if((hw_control->rftype == RFIC_SCIS1022)||(hw_control->rftype == RFIC_SCIS1032))
     {
		 if ((Channel >= 1) && (Channel <= 14))
		 {
			 if((hw_control->phymode == WIRELESS_MODE_G) || (hw_control->phymode == WIRELESS_MODE_B))
			 {
					/*if(hw_control->rftype == RFIC_SCIS1022)
					{
						if((((HWChannelConfig[Channel-1][0]>>24)&0x00FF)!=0)
							&&(((HWChannelConfig[Channel-1][0]>>24)&0x00FF)!=0xFF))
						{
							HwPlatformIOWrite(hUSB,0xB40,HWChannelConfig[Channel-1][0]);
						}
						if((HWChannelConfig[Channel-1][1]!=0)
							&&(HWChannelConfig[Channel-1][1]!=0xFFFFFFFF))
						{
							HwPlatformIOWrite(hUSB,0xE68,HWChannelConfig[Channel-1][1]);
						}

						S1022WriteReg(mcon,3,s1022conf24G40M[Channel-1][0]);
						S1022WriteReg(mcon,4,s1022conf24G40M[Channel-1][1]);
						S1022WriteReg(mcon,6,s1022conf24G40M[Channel-1][2]);
					}*/
					if(hw_control->rftype == RFIC_SCIS1032)
					{
						/*if((((HWChannelConfig[Channel-1][0]>>24)&0x00FF)!=0)
							&&(((HWChannelConfig[Channel-1][0]>>24)&0x00FF)!=0xFF))
						{
							HwPlatformIOWrite(hUSB,0xB40,HWChannelConfig[Channel-1][0]);
						}
						if((HWChannelConfig[Channel-1][1]!=0)
							&&(HWChannelConfig[Channel-1][1]!=0xFFFFFFFF))
						{
							HwPlatformIOWrite(hUSB,0xE68,HWChannelConfig[Channel-1][1]);
						}*/

						S1032WriteReg(mcon,3,s1032conf24G40M[Channel-1][0]);
						S1032WriteReg(mcon,4,s1032conf24G40M[Channel-1][1]);
						//S1032WriteReg(mcon,6,s1032conf24G40M[Channel-1][2]);
                        HwPlatformIOWrite(mcon->hUSB,0xA04,0x8019b8F6);
                        //PRINT("channel %d rf reg set !\n",Channel);
					}
			 }
			 else if(hw_control->phymode == WIRELESS_MODE_A)
			 {
				/* if(hw_control->rftype == RFIC_SCIS1022)
				 {
					 int i;
					 int len = sizeof(s1022conf5G40MChn)/sizeof(DWORD);
					 for(i=0; i< len;i++)
					 {
						 if(s1022conf5G40MChn[i] == Channel)
						 {
							 break;
						 }
					 }
					 if(i == len)
					 {
						 PRINT("Error Channel\n");
					 }
					 else
					 {
						S1022WriteReg(mcon,3,s1022conf5G40M[i][0]);
						S1022WriteReg(mcon,4,s1022conf5G40M[i][1]);
						S1022WriteReg(mcon,6,s1022conf5G40M[i][2]);
					 }
				 }*/
			 }
			 else
			 {
				PRINT("Error PhyMode\n");
			 }
		 }
     }
}
#if 0
VOID MAX2829WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data)
{
    HANDLE hUSB = mcon->hUSB;
    ULONG addr = 0x0a00;
    ULONG value = 0x80000000 + (data<<4) + reg;

    HwPlatformIOWrite(hUSB,addr,value);
}

VOID MAX2830WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data)
{
    HANDLE hUSB = mcon->hUSB;
    ULONG addr = 0x0a00;
    ULONG value = 0x80000000 + (data<<4) + reg;

    HwPlatformIOWrite(hUSB,addr,value);
}

VOID AL7230WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data)
{
    HANDLE hUSB = mcon->hUSB;
    ULONG addr = 0x0a04;
    ULONG value = 0x80000000 + (data<<4) + reg;

    HwPlatformIOWrite(hUSB,addr,value);
}

VOID S102WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data)
{
    HANDLE hUSB = mcon->hUSB;
    ULONG addr = 0x0a00;
    ULONG value = 0x80000000 + (data<<4) + reg;

    HwPlatformIOWrite(hUSB,addr,value);
}

VOID S1022WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data)
{
    HANDLE hUSB = mcon->hUSB;
    ULONG addr = 0x0a04;
    ULONG value = 0x80000000 + (data<<4) + reg;

    HwPlatformIOWrite(hUSB,addr,value);
}
#endif

VOID S1032WriteReg(PMAC_CONTROL mcon,UCHAR reg,ULONG data)
{
    HANDLE hUSB = mcon->hUSB;
    ULONG addr = 0x0A04;
    ULONG value = 0x80000000 + (data<<4) + reg;

    HwPlatformIOWrite(hUSB,addr,value);
}

#if 0
VOID S1022AdjustingBF_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //BF
    HwPlatformIOWrite(hUSB,0x0838,0x00000021);
    S1022WriteReg(hw_control,0x0,0x00008);
    SLEEP(50);
    S1022WriteReg(hw_control,0x0,0x00000);
}

VOID S1022AdjustingLO_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //LO
    HwPlatformIOWrite(hUSB,0x0838,0x00000021);
    S1022WriteReg(hw_control,0x0,0x00004);
    SLEEP(50);
}

VOID S1022AdjustingRX_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //RX
    HwPlatformIOWrite(hUSB,0x0838,0x00000023);
    S1022WriteReg(hw_control,0x0,0x00002);
    SLEEP(50);
    S1022WriteReg(hw_control,0x0,0x00000);
}

VOID S1022AdjustingTX_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //TX
    HwPlatformIOWrite(hUSB,0x0838,0x00000025);
    S1022WriteReg(hw_control,0x0,0x00001);
    SLEEP(50);
    S1022WriteReg(hw_control,0x0,0x00000);
}

VOID S1022AdjustingBF_A(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //BF
    HwPlatformIOWrite(hUSB,0x0838,0x00000021);
    S1022WriteReg(hw_control,0x0,0x40008);
    SLEEP(50);
    S1022WriteReg(hw_control,0x0,0x40000);
}

VOID S1022AdjustingLO_A(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //LO
    HwPlatformIOWrite(hUSB,0x0838,0x00000021);
    S1022WriteReg(hw_control,0x0,0x40004);
    SLEEP(50);
}

VOID S1022AdjustingRX_A(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //RX
    HwPlatformIOWrite(hUSB,0x0838,0x00000023);
    S1022WriteReg(hw_control,0x0,0x40002);
    SLEEP(50);
    S1022WriteReg(hw_control,0x0,0x40000);
}

VOID S1022AdjustingTX_A(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //TX
    HwPlatformIOWrite(hUSB,0x0838,0x00000025);
    S1022WriteReg(hw_control,0x0,0x40001);
    SLEEP(50);
    S1022WriteReg(hw_control,0x0,0x40000);
}

VOID S1032AdjustingBF_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //BF
    HwPlatformIOWrite(hUSB,0x0838,0x00000021);
    S1032WriteReg(hw_control,0x0,0x00008);
    SLEEP(50);
    S1032WriteReg(hw_control,0x0,0x00000);
}

VOID S1032AdjustingLO_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //LO
    HwPlatformIOWrite(hUSB,0x0838,0x00000021);
    S1032WriteReg(hw_control,0x0,0x00004);
    SLEEP(50);
}

VOID S1032AdjustingRX_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //RX
    HwPlatformIOWrite(hUSB,0x0838,0x00000023);
    S1032WriteReg(hw_control,0x0,0x00002);
    SLEEP(50);
    S1032WriteReg(hw_control,0x0,0x00000);
}

VOID S1032AdjustingTX_BG(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    //TX
    HwPlatformIOWrite(hUSB,0x0838,0x00000025);
    S1032WriteReg(hw_control,0x0,0x00001);
    SLEEP(50);
    S1032WriteReg(hw_control,0x0,0x00000);
}
#endif

VOID HWDbgPrintReg(PMAC_CONTROL hw_control)
{
    HANDLE hUSB = hw_control->hUSB;
    UINT i;
    USHORT Addr=0x0;

    PRINT("======== MAC REG ========\n");   
    Addr=0x500;
    for(i=0;i<=0x50;i=i+4)
    {
       //HwPlatformIORead(hUSB,(Addr+i));
       PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }

    Addr=0x600;
    for(i=0;i<=0x54;i=i+4)
    {
       //HwPlatformIORead(hUSB,(Addr+i));
       PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }

    Addr=0x700;
    for(i=0;i<=0x48;i=i+4)   //MAC Group Address 0 (0x070C -- 0x0710)开始有问题
    {
        //HwPlatformIORead(hUSB,(Addr+i));
        PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }

    Addr=0x800;
    for(i=0;i<=0x40;i=i+4)   //TBTT_TIMER (0x0808 -0x080C)开始有问题
    {
        //HwPlatformIORead(hUSB,(Addr+i));
        PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }

    Addr=0x900;
    for(i=0;i<=0x20;i=i+4)
    {
        //HwPlatformIORead(hUSB,(Addr+i));
        PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }

    Addr=0xA00;
    for(i=0;i<=0x24;i=i+4)
    {
        //HwPlatformIORead(hUSB,(Addr+i));
        PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }

    PRINT("======== PHY REG ========\n");   
    Addr=0xB00;
    for(i=0;i<=0xAC;i=i+4)
    {
        //HwPlatformIORead(hUSB,(Addr+i));
        PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }
    Addr=0xE40;
    for(i=0;i<=0x60;i=i+4)
    {
        //HwPlatformIORead(hUSB,(Addr+i));
        PRINT("0x%x : 0x%x \n",Addr+i,HwPlatformIORead(hUSB,(Addr+i)));  
    }
}

VOID HWSetMacAddr(PMAC_CONTROL hw_control,UCHAR mac_addr[6])
{
    HANDLE hUSB = hw_control->hUSB;

    HwPlatformIOWrite(hUSB,MAC_ADDR0,mac_addr[0] | mac_addr[1]<<8 | mac_addr[2]<<16 | \
                      mac_addr[3]<<24 );
    HwPlatformIOWrite(hUSB,MAC_ADDR1,mac_addr[4] | mac_addr[5]<<8 );
}

VOID HWSetBSSIDAddr(PMAC_CONTROL hw_control,UCHAR bssid[6])
{
    HANDLE hUSB = hw_control->hUSB;

    HwPlatformIOWrite(hUSB,BSSID_ADDR0,bssid[0] | bssid[1]<<8 | bssid[2]<<16 | \
                      bssid[3]<<24 );
    HwPlatformIOWrite(hUSB,BSSID_ADDR1,bssid[4] | bssid[5]<<8 );
}

VOID HWSetKeyAddr(PMAC_CONTROL hw_control,UCHAR KeyIdx,UCHAR *Key)
{
    UINT i;
    USHORT Addr=0x0;

    union _temp {
        ULONG   temp_ul;
        struct _temp_uc{
            UCHAR ch1;
            UCHAR ch2;
            UCHAR ch3;
            UCHAR ch4;
            } temp_uc;
    } Data;

    switch(KeyIdx)
    {
    case 0 : Addr=KEY0BASE; break;
    case 1 : Addr=KEY1BASE; break;
    case 2 : Addr=KEY2BASE; break;
    case 3 : Addr=KEY3BASE; break;
    default: Addr=KEY0BASE;
    };

    for (i=0; i<MAX_LEN_OF_SHARE_KEY; i = i+4)
    {
		Data.temp_uc.ch1 =  Key[i];
        Data.temp_uc.ch2 =  Key[i+1];
        Data.temp_uc.ch3 =  Key[i+2];
        Data.temp_uc.ch4 =  Key[i+3];

        HwPlatformIOWrite(hw_control->hUSB,(Addr + i),Data.temp_ul );
        //HwPlatformIORead(hUSB,(Addr + i));  //for debug
    }
}

VOID HWResetKeyAddr(PMAC_CONTROL hw_control,UCHAR KeyIdx)
{
    UINT i;
    USHORT Addr=0x0;

    switch(KeyIdx)
    {
        case 0 : Addr=KEY0BASE; break;
        case 1 : Addr=KEY1BASE; break;
        case 2 : Addr=KEY2BASE; break;
        case 3 : Addr=KEY3BASE; break;
        default: Addr=KEY0BASE;
    };

    for (i=0; i<MAX_LEN_OF_SHARE_KEY; i = i+4)
    {
        HwPlatformIOWrite(hw_control->hUSB,(Addr+i),0x0 );  //wep_40
    }
}

UINT HWGetWmmKeyType(PMAC_CONTROL hw_control)
{
    UINT val;

    val = HwPlatformIORead(hw_control->hUSB,WMM_KEYTYPE);
    return val;
}

VOID HWSetWmmKeyType(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,WMM_KEYTYPE,val );
}

UINT HWGetCapInfo(PMAC_CONTROL hw_control)
{
    UINT val;

    val = HwPlatformIORead(hw_control->hUSB,CAPINFO);
    return val;
}

VOID HWSetCapInfo(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,CAPINFO,val);
}

VOID HWSetBcnInerv(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,BCNINTERV,val);
}

VOID HWSetAtimPeriod(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,ATIMPERIOD,val);
}

VOID HWSetDsChannel(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,DSCHANNEL,val);
}

VOID HWSetSsidLen(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,SSIDLEN,val);
}

UINT HWGetErpInfo(PMAC_CONTROL hw_control)
{
    UINT val;

    val = HwPlatformIORead(hw_control->hUSB,ERPINFO);
    return val;
}

VOID HWSetErpInfo(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,ERPINFO,val);
}

VOID HWSetBcnPrspLng(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,BCNPRSPLNG,val);
}

VOID HWSetTbttTimer(PMAC_CONTROL hw_control,UINT low,UINT high)
{
    HwPlatformIOWrite(hw_control->hUSB,TBTTTIMER,low);
    HwPlatformIOWrite(hw_control->hUSB,TBTTTIMER+4,high);
}

/*
UINT HWGetSystemConfig(PMAC_CONTROL hw_control)
{
    UINT val;

    val = hw_control->systemConfig;  //07252011 dmou
    //val = HwPlatformIORead(hw_control->hUSB,SYSTEMCONFIG);
    return val;
}

VOID HWSetSystemConfig(PMAC_CONTROL hw_control,UINT val)
{
    hw_control->systemConfig = val;  //07252011 dmou
    HwPlatformIOWrite(hw_control->hUSB,SYSTEMCONFIG,val);
    
}

UINT HWGetTxRxConfig(PMAC_CONTROL hw_control)
{
    UINT val;

    val = HwPlatformIORead(hw_control->hUSB,TRXCONFIG);
    return val;
}

VOID HWSetTxRxConfig(PMAC_CONTROL hw_control,UINT val)
{
    HwPlatformIOWrite(hw_control->hUSB,TRXCONFIG,val);
}*/

UINT HWGetTsfBaseLow(PMAC_CONTROL hw_control)
{
    UINT val;

    val = HwPlatformIORead(hw_control->hUSB,TSFBASE);
    return val;
}

UINT HWGetTsfBaseHig(PMAC_CONTROL hw_control)
{
    UINT val;

    val = HwPlatformIORead(hw_control->hUSB,TSFBASE+4);
    return val;
}

VOID HWSetSsid(PMAC_CONTROL hw_control,UCHAR ssid_len,UCHAR *ssid)
{
    UINT i,tmp_word;

    for(i=0;i<ssid_len;i=i+4)
    {
        tmp_word = (unsigned long)(ssid[i])  |
            (unsigned long)(ssid[i+1] << 8)  |
            (unsigned long)(ssid[i+2] << 16) |
            (unsigned long)(ssid[i+3] << 24);
        HwPlatformIOWrite(hw_control->hUSB,(SSIDSTART + i),tmp_word );
    }
}

VOID HWSetSuppRate(PMAC_CONTROL hw_control,UCHAR supp_rates_len,UCHAR *support_rate)
{
    UINT i;

    union _temp {
        ULONG   temp_ul;
        struct _temp_uc{
            UCHAR sr1;
            UCHAR sr2;
            UCHAR sr3;
            UCHAR sr4;
            } temp_uc;
    } Data;

     for (i=0; i<supp_rates_len; i=i+4)
    {
        Data.temp_uc.sr1 =  support_rate[i];
        Data.temp_uc.sr2 =  support_rate[i+1];
        Data.temp_uc.sr3 =  support_rate[i+2];
        Data.temp_uc.sr4 =  support_rate[i+3];

        HwPlatformIOWrite(hw_control->hUSB,(SUPRATESTART + i),Data.temp_ul );
     }
}

VOID HWSetExtRate(PMAC_CONTROL hw_control,UCHAR ext_rates_len,UCHAR *ext_rate)
{
    UINT i;

    union _temp {
        ULONG   temp_ul;
        struct _temp_uc{
            UCHAR sr1;
            UCHAR sr2;
            UCHAR sr3;
            UCHAR sr4;
            } temp_uc;
    } Data;

     for (i=0; i<ext_rates_len; i=i+4)
    {
        Data.temp_uc.sr1 =  ext_rate[i];
        Data.temp_uc.sr2 =  ext_rate[i+1];
        Data.temp_uc.sr3 =  ext_rate[i+2];
        Data.temp_uc.sr4 =  ext_rate[i+3];

        HwPlatformIOWrite(hw_control->hUSB,(EXTRATE + i),Data.temp_ul );
     }
}

void HWEEPROMWriteEnable(PMAC_CONTROL hw_control)
{
    ULONG ret;
    int timeout =100;
    HwPlatformIOWrite(hw_control->hUSB,0x504,0x20c00000);
    do
    {
        timeout --;
        if(timeout<=0)
            break;
        ret=HwPlatformIORead(hw_control->hUSB,0x504);
        SLEEP(10);
    }while(!(ret&0x40000000));

}

void HWEEPROMWriteDisable(PMAC_CONTROL hw_control)
{
    ULONG ret;
    int timeout =100;
    HwPlatformIOWrite(hw_control->hUSB,0x504,0x10000000);

    do
    {
        timeout --;
        if(timeout<=0)
            break;
        ret=HwPlatformIORead(hw_control->hUSB,0x504);
        SLEEP(10);
    }while(!(ret&0x40000000));
}

void HWEEPROMWrite(PMAC_CONTROL hw_control,ULONG offset,ULONG value)
{
    int timeout =100;
    ULONG data=0x08000000 +(offset<<16 )+ value;
    ULONG ret;

    HWEEPROMWriteEnable(hw_control);
    HwPlatformIOWrite(hw_control->hUSB,0x504,data);
    do
    {
        timeout --;
    if(timeout<=0)
        break;
        ret=HwPlatformIORead(hw_control->hUSB,0x504);
        SLEEP(10);
    }while(!(ret & 0x04000000));

    HWEEPROMWriteDisable(hw_control);
}

ULONG HWEEPROMRead(PMAC_CONTROL hw_control,ULONG offset)
{
    ULONG   value=0;
    ULONG   retValue;
    int timeout =100;

    HwPlatformIOWrite(hw_control->hUSB,0x504,0x02000000 | (offset<<16));

    do
    {
        timeout --;
        if(timeout<=0)
            break;
        value=HwPlatformIORead(hw_control->hUSB,0x504);
        SLEEP(10);
    }while(!(value & 0x01000000));
    retValue=value & 0x0000ffff;

    return retValue;
}

static inline int HWEEPROMGetLogo(PMAC_CONTROL hw_control)
{
	ULONG gain1;
	ULONG gain2;
	
	if(SCIFlag == 0){
		gain1 = HWEEPROMRead(hw_control,EEPROM_LOGO_OFFSET);
		gain2 = HWEEPROMRead(hw_control,EEPROM_LOGO_OFFSET+1);

		if(!(((gain2 & 0xFF) == 'I') && (((gain1 >> 8) & 0xFF) == 'C') && ((gain1 & 0xFF) == 'S')))
		{
			PRINT("EEPROM No Config!!\n");
			return -1;
		}else{
			SCIFlag = 1;
		}
	}
	return 0;
}

int HWEEPROMReadMACAddr(PMAC_CONTROL hw_control, UCHAR *mac_addr,ULONG moduletype)  ////06072012 dmou
{
	ULONG data;
	
	if( HWEEPROMGetLogo(hw_control) ){
		PRINT("SCI.err: error occured when getting chip logo in [%s] \n", __func__);
		return 0;
	}

	if((moduletype == 0x001B) || (moduletype == 0x001C))
	{
		data = HWEEPROMRead(hw_control,EEPROM_MACADDR_OFFSET+2);
		 mac_addr[1] =(UCHAR) (data & 0x00FF);
		 mac_addr[0] =(UCHAR) ((data & 0xFF00)>>8);
	
		 data = HWEEPROMRead(hw_control,EEPROM_MACADDR_OFFSET+1);
	
		 mac_addr[3] = (UCHAR) (data & 0x00FF);
		 mac_addr[2] = (UCHAR) ((data & 0xFF00)>>8);
		 data = HWEEPROMRead(hw_control,EEPROM_MACADDR_OFFSET);
	
		 mac_addr[5] = (UCHAR) (data & 0x00FF);
		 mac_addr[4] = (UCHAR) ((data & 0xFF00)>>8);
	}
	else
	{	
		 data = HWEEPROMRead(hw_control,EEPROM_MACADDR_OFFSET);
		 mac_addr[0] =(UCHAR) (data & 0x00FF);
		 mac_addr[1] =(UCHAR) ((data & 0xFF00)>>8);
	
		 data = HWEEPROMRead(hw_control,EEPROM_MACADDR_OFFSET+1);
	
		 mac_addr[2] = (UCHAR) (data & 0x00FF);
		 mac_addr[3] = (UCHAR) ((data & 0xFF00)>>8);
		 data = HWEEPROMRead(hw_control,EEPROM_MACADDR_OFFSET+2);
	
		 mac_addr[4] = (UCHAR) (data & 0x00FF);
		 mac_addr[5] = (UCHAR) ((data & 0xFF00)>>8);
	 }

	 return 1;
}

ULONG HWEEPROMReadRF_BB_M(PMAC_CONTROL hw_control)
{
   return HWEEPROMRead(hw_control, EEPROM_RF_BB_M_OFFSET);
}

unsigned short HWEEPROMReadVER(PMAC_CONTROL hw_control)
{
   return (unsigned short)HWEEPROMRead(hw_control, EEPROM_VER_OFFSET);
}

ULONG HWEEPROMReadRFType(PMAC_CONTROL hw_control)
{
	if( HWEEPROMGetLogo(hw_control) ){
		PRINT("SCI.err: error occured when getting chip logo in [%s] \n", __func__);
		return 0;
	}

	return HWEEPROMRead(hw_control,EEPROM_RFTYPE_OFFSET) & 0xFFFF;
}

ULONG HWEEPROMReadMACType(PMAC_CONTROL hw_control)
{
	if( HWEEPROMGetLogo(hw_control) ){
		PRINT("SCI.err: error occured when getting chip logo in [%s] \n", __func__);
		return 0;
	}

	return HWEEPROMRead(hw_control, EEPROM_MACTYPE_OFFSET) & 0xFFFF;
}
ULONG HWEEPROMReadModuleType(PMAC_CONTROL hw_control)
{
	return HWEEPROMRead(hw_control, EEPROM_MODULETYPE_OFFSET);
}
/*
ULONG HWEEPROMReadCALValue(PMAC_CONTROL hw_control)
{
	ULONG gain1;
	ULONG gain2;

	gain1 = HWEEPROMRead(hw_control, EEPROM_LOGO_OFFSET);
	gain2 = HWEEPROMRead(hw_control, EEPROM_LOGO_OFFSET + 1);
	if(!(((gain2 & 0xFF) == 'I') && (((gain1 >> 8) & 0xFF) == 'C') && ((gain1 & 0xFF) == 'S')))
	{
		PRINT("EEPROM No Config!!\n");
		return 0;
	}

	return HWEEPROMRead(hw_control, EEPROM_CALVALUE_OFFSET) & 0xFFFF;
}
*/

#if 0
int HWEEPROMReadChannelConfig(PMAC_CONTROL hw_control)
{
	UCHAR i;
	UCHAR channel;
	ULONG data;
	ULONG data1;
	ULONG data2;
	UCHAR QAM64Gain;
	UCHAR QAM16Gain;
	UCHAR QPSKGain;
	UCHAR BPSKGain;
	UCHAR CCKGain;
	 ULONG moduletype;

	HWCleanChannelConfig(hw_control);
    moduletype = HWEEPROMReadModuleType(hw_control);
	i = 0;
	if((moduletype == 0x001B) || (moduletype == 0x001C))									//从0x20-0x26读
	{
		for(channel = 0; channel < 14; i++)
		{
			data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i);				//读两个信道的能量值

			QAM64Gain = (UCHAR)(data & 0xFF);												//前一信道的能量值
			QAM16Gain = QAM64Gain + 4;
			QPSKGain = QAM16Gain + 4;
			BPSKGain = QPSKGain + 4;
			CCKGain = BPSKGain + 4;
			data1 = (QAM64Gain << 24) | 0x685A52;
			data2 = (QAM16Gain << 24) | (QPSKGain << 16) | (BPSKGain << 8) | CCKGain;
			HWChannelConfig[channel][0] = data1;											//前一信道的B40寄存器值
			HWChannelConfig[channel][1] = data2;											//前一信道的E68寄存器值
			channel++;

			QAM64Gain = (UCHAR)((data >> 8) & 0xFF);										//后一信道的能量值
			QAM16Gain = QAM64Gain + 4;
			QPSKGain = QAM16Gain + 4;
			BPSKGain = QPSKGain + 4;
			CCKGain = BPSKGain + 4;
			data1 = (QAM64Gain << 24) | 0x685A52;
			data2 = (QAM16Gain << 24) | (QPSKGain << 16) | (BPSKGain << 8) | CCKGain;
			HWChannelConfig[channel][0] = data1;											//后一信道的B40寄存器值
			HWChannelConfig[channel][1] = data2;											//后一信道的E68寄存器值
			channel++;
		}
	}
	else																					//从0xB0-0xE7读
	{
		for(channel = 0; channel < 14; i += 4, channel++)
		{
			data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + 0x90 + i);		//读信道的B40寄存器值低16位
			data1 = data & 0xFFFF;
			data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + 0x90 + i + 1 );	//读信道的B40寄存器值高16位
			data1 |= (data & 0xFFFF) << 16;

			data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + 0x90 + i + 2 );	//读信道的E68寄存器值低16位
			data2 = data & 0xFFFF;
			data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + 0x90 + i + 3 );	//读信道的E68寄存器值高16位
			data2 |= (data & 0xFFFF) << 16;

			HWChannelConfig[channel][0] = data1;											//信道的B40寄存器值
			HWChannelConfig[channel][1] = data2;											//信道的E68寄存器值
		}
	}
	return 1;
	//for(channel = 0; channel < 14; i += 4, channel++)
	//{
	//	data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i);
	//	data1 = data & 0xFFFF;
	//	data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i + 1 );
	//	data1 |= (data & 0xFFFF) << 16;

	//	data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i + 2 );
	//	data2 = data & 0xFFFF;
	//	data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i + 3 );
	//	data2 |= (data & 0xFFFF) << 16;

	//	HWChannelConfig[channel][0] = data1;
	//	HWChannelConfig[channel][1] = data2;
	//}

	//for(channel = 0; channel < 14; i++)
	//{
	//	data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i);			//每次读两个信道的能量值

	//	data1 = data & 0xFF;														//前一信道的能量值
	//	data1 <<= 24;
	//	//data1 |= 0x685A5A;
	//	data1 |= 0x685A52;
	//	data2 = data & 0xFF;
	//	//data2 |= (data2 << 8) | (data2 << 16) |(data2 << 24);
	//	data2 = 0x29 | (data2 << 8) | (data2 << 16) |(data2 << 24);
	//	HWChannelConfig[channel][0] = data1;										//前一信道的B40寄存器值
	//	HWChannelConfig[channel][1] = data2;										//前一信道的E68寄存器值
	//	channel++;

	//	data1 = (data >> 8) & 0xFF;													//后一信道的能量值
	//	data1 <<= 24;
	//	//data1 |= 0x685A5A;
	//	data1 |= 0x685A52;
	//	data2 = (data >> 8) & 0xFF;
	//	//data2 |= (data2 << 8) | (data2 << 16) |(data2 << 24);
	//	data2 = 0x29 | (data2 << 8) | (data2 << 16) |(data2 << 24);
	//	HWChannelConfig[channel][0] = data1;										//后一信道的B40寄存器值
	//	HWChannelConfig[channel][1] = data2;										//后一信道的E68寄存器值
	//	channel++;
	//}

	//for(channel = 0; channel < 14; i++)
	//{
	//	data = HWEEPROMRead(hw_control, EEPROM_CH2_1_CAL_GAIN_OFFSET/*EEPROM_CHANNELCONFIG_OFFSET*/ + i);			//每次读两个信道的能量值

	//	QAM64Gain = (UCHAR)(data & 0xFF);											//前一信道的能量值
	//	QAM16Gain = QAM64Gain + 4;
	//	QPSKGain = QAM16Gain + 4;
	//	BPSKGain = QPSKGain + 4;
	//	CCKGain = BPSKGain + 4;
	//	data1 = (QAM64Gain << 24) | 0x685A52;
	//	data2 = (QAM16Gain << 24) | (QPSKGain << 16) | (BPSKGain << 8) | CCKGain;
	//	HWChannelConfig[channel][0] = data1;										//前一信道的B40寄存器值
	//	HWChannelConfig[channel][1] = data2;										//前一信道的E68寄存器值
	//	channel++;

	//	QAM64Gain = (UCHAR)((data >> 8) & 0xFF);									//后一信道的能量值
	//	QAM16Gain = QAM64Gain + 4;
	//	QPSKGain = QAM16Gain + 4;
	//	BPSKGain = QPSKGain + 4;
	//	CCKGain = BPSKGain + 4;
	//	data1 = (QAM64Gain << 24) | 0x685A52;
	//	data2 = (QAM16Gain << 24) | (QPSKGain << 16) | (BPSKGain << 8) | CCKGain;
	//	HWChannelConfig[channel][0] = data1;										//后一信道的B40寄存器值
	//	HWChannelConfig[channel][1] = data2;										//后一信道的E68寄存器值
	//	channel++;
	//}
}

VOID HWJoinEnable(PMAC_CONTROL hw_control,UINT CurrentBSSType)
{
    HANDLE hUSB = hw_control->hUSB;
    UINT RFType = hw_control->rftype;
    ULONG word = 0;

    if(RFType == RFIC_SCIS102)
    {
        word = 0x1447;
    }
    else if(RFType == RFIC_SCIS1022)
    {
        word = 0xC47;
    }
    else if(RFType == RFIC_SCIS1032)
    {
        word = 0xC47;
    }

    if(CurrentBSSType == BSS_TYPE/*dot11_BSS_type_infrastructure*/)
    {
        word |= SYSCFG_JOINEN | SYSCFG_BSSTYPE;
    }
    else if(CurrentBSSType == IBSS_TYPE/*dot11_BSS_type_independent*/)
    {
        word |= SYSCFG_JOINEN;
        word &= ~SYSCFG_BSSTYPE;
    }

    HwPlatformIOWrite(hUSB,SYSTEMCONFIG,word);
}
#endif

VOID HWSetTXDelay(PMAC_CONTROL hw_control,UINT  bWep)
{
	//UNREFERENCED_PARAMETER(hw_control);
	//UNREFERENCED_PARAMETER(bWep);
   // if(bWep)
   //     HwPlatformIOWrite(hw_control->hUSB,TXDELAY,0x62143428);
  //  else
  //      HwPlatformIOWrite(hw_control->hUSB,TXDELAY,0x01143428);

}
#if 0
VOID BeaconReceiveEnable(PMAC_CONTROL hw_control)
{
	HANDLE hUSB = hw_control->hUSB;
	ULONG word = 0;

	word = TRXCONFIG_VAL|0x20;
	HwPlatformIOWrite(hUSB,TRXCONFIG,word);
}

VOID BeaconReceiveDisable(PMAC_CONTROL hw_control)
{
	HANDLE hUSB = hw_control->hUSB;
	ULONG word = 0;

	word = TRXCONFIG_VAL&(~0x20);
	HwPlatformIOWrite(hUSB,TRXCONFIG,word);
}
#endif

struct _EEPROM_STATE_STRUC HWEEPROMReadState(PMAC_CONTROL hw_control)
{
    unsigned char val;
    struct _EEPROM_STATE_STRUC state;

    val = (unsigned char)HWEEPROMRead(hw_control, EEPROM_STATE_OFFSET);

    state.PRE_CAL_TXIQ_DONE = val & 0x01;
    state.RX_POWER_CAL_DONE = (val & 0x02)>>1;
    state.TX_POWER_CAL_DONE = (val & 0x04)>>2;
    state.TARGET_POWER_DONE = (val & 0x08)>>3;
    
    return state; 
}

unsigned short HWEEPROMReadTXPower(PMAC_CONTROL hw_control,unsigned short ver)
{
    struct _EEPROM_STATE_STRUC state;
    unsigned short TX_POWER_INI;

    state = HWEEPROMReadState(hw_control);

    if(ver == 0x01)
    {
        if(state.TARGET_POWER_DONE == 0x01)
            TX_POWER_INI = HWEEPROMRead(hw_control, EEPROM_TXPOWER_OFFSET);
        else
            TX_POWER_INI = 15;
    }else
        TX_POWER_INI = 15;

    return TX_POWER_INI;

}

unsigned char HWEEPROMReadNoise(PMAC_CONTROL hw_control,unsigned short ver)
{
    unsigned char channel_noise;

    if(ver == 0x01)
        channel_noise = (unsigned char)HWEEPROMRead(hw_control, EEPROM_CHANNEL_NOISE_OFFSET)& 0xFF;
    else
        channel_noise = 15;

    return channel_noise;
}

struct _TXIQIMBALANCE HWEEPROMReadTXIQIMBALANCE(PMAC_CONTROL hw_control,unsigned short ver)
{
    struct _EEPROM_STATE_STRUC state;
    struct _TXIQIMBALANCE TXIQImbalance;

    state = HWEEPROMReadState(hw_control);

    if(ver == 0x01)
    {
        if(state.PRE_CAL_TXIQ_DONE == 0x01)
        {
            TXIQImbalance.COM_24G_P_TX = ((unsigned char)HWEEPROMRead(hw_control, EEPROM_TX_IQ_IMBALANCE_24G_OFFSET)& 0xFF00)>>8;
            TXIQImbalance.COM_24G_G_TX = (unsigned char)HWEEPROMRead(hw_control, EEPROM_TX_IQ_IMBALANCE_24G_OFFSET)& 0xFF;
        }else
        {
            TXIQImbalance.COM_24G_P_TX = 0x09;
            TXIQImbalance.COM_24G_G_TX = 0x80;
        }
    }else
    {
        TXIQImbalance.COM_24G_P_TX = 0x09;
        TXIQImbalance.COM_24G_G_TX = 0x80;
    }
        
    return TXIQImbalance;
}

struct _RXDCIMBALANCE HWEEPROMReadRXDCIMBALANCE(PMAC_CONTROL hw_control,unsigned short ver)
{
    struct _RXDCIMBALANCE RxDCImbalance;

    if(ver == 0x01){
        RxDCImbalance.COM_24G_RX_I_DC = ((short)HWEEPROMRead(hw_control, EEPROM_COM_24G_RX_I_DC_OFFSET))& 0x3FF;
        RxDCImbalance.COM_24G_RX_Q_DC = ((short)HWEEPROMRead(hw_control, EEPROM_COM_24G_RX_Q_DC_OFFSET))& 0x3FF;
    }else
    {    
        RxDCImbalance.COM_24G_RX_I_DC = 0;
        RxDCImbalance.COM_24G_RX_Q_DC = 0;
    }

    return RxDCImbalance;
}

struct _CH_QAM64_GAIN HWEEPROMReadCH_QAM64_GAIN(PMAC_CONTROL hw_control)
{
    struct _CH_QAM64_GAIN CHQAM64Gain;

    CHQAM64Gain.CH1_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH2_1_CAL_GAIN_OFFSET)& 0xFF;          
    CHQAM64Gain.CH2_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH2_1_CAL_GAIN_OFFSET)& 0xFF00)>>8;   
    CHQAM64Gain.CH3_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH4_3_CAL_GAIN_OFFSET)& 0xFF;          
    CHQAM64Gain.CH4_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH4_3_CAL_GAIN_OFFSET)& 0xFF00)>>8;   
    CHQAM64Gain.CH5_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH6_5_CAL_GAIN_OFFSET)& 0xFF;          
    CHQAM64Gain.CH6_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH6_5_CAL_GAIN_OFFSET)& 0xFF00)>>8;   
    CHQAM64Gain.CH7_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH8_7_CAL_GAIN_OFFSET)& 0xFF;          
    CHQAM64Gain.CH8_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH8_7_CAL_GAIN_OFFSET)& 0xFF00)>>8;   
    CHQAM64Gain.CH9_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH10_9_CAL_GAIN_OFFSET)& 0xFF;         
    CHQAM64Gain.CH10_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH10_9_CAL_GAIN_OFFSET)& 0xFF00)>>8; 
    CHQAM64Gain.CH11_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH12_11_CAL_GAIN_OFFSET)& 0xFF;       
    CHQAM64Gain.CH12_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH12_11_CAL_GAIN_OFFSET)& 0xFF00)>>8;
    CHQAM64Gain.CH13_QAM64_GAIN = (unsigned char)HWEEPROMRead(hw_control,EEPROM_CH14_13_CAL_GAIN_OFFSET)& 0xFF;       
    CHQAM64Gain.CH14_QAM64_GAIN = ((unsigned char)HWEEPROMRead(hw_control,EEPROM_CH14_13_CAL_GAIN_OFFSET)& 0xFF00)>>8;

    return CHQAM64Gain;
}

struct _CH_DIFF_POWER HWEEPROMReadCH_DIFF(PMAC_CONTROL hw_control,unsigned short ver)
{
    struct _EEPROM_STATE_STRUC state;
    struct _CH_DIFF_POWER CHDiffPower;

    state = HWEEPROMReadState(hw_control);

    if(ver == 0x01)
    {
        if(state.TX_POWER_CAL_DONE == 0x01)
        {
            CHDiffPower.CH1_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_1_DIFF_OFFSET)& 0xFF;  
            CHDiffPower.CH2_DIFF_POWER = ((int8_t)HWEEPROMRead(hw_control, EEPROM_CH_2_3_DIFF_OFFSET)& 0xFF00)>>8; 
            CHDiffPower.CH3_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_2_3_DIFF_OFFSET)& 0xFF;  
            CHDiffPower.CH4_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_4_5_DIFF_OFFSET)& 0xFF;
            CHDiffPower.CH5_DIFF_POWER = ((int8_t)HWEEPROMRead(hw_control, EEPROM_CH_4_5_DIFF_OFFSET)& 0xFF00)>>8; 
            CHDiffPower.CH6_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_6_8_DIFF_OFFSET)& 0xFF;  
            CHDiffPower.CH7_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_7_DIFF_OFFSET)& 0xFF;   
            CHDiffPower.CH8_DIFF_POWER = ((int8_t)HWEEPROMRead(hw_control, EEPROM_CH_6_8_DIFF_OFFSET)& 0xFF00)>>8;   
            CHDiffPower.CH9_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_9_10_DIFF_OFFSET)& 0xFF;  
            CHDiffPower.CH10_DIFF_POWER = ((int8_t)HWEEPROMRead(hw_control, EEPROM_CH_9_10_DIFF_OFFSET)& 0xFF00)>>8;   
            CHDiffPower.CH11_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_11_12_DIFF_OFFSET)& 0xFF; 
            CHDiffPower.CH12_DIFF_POWER = ((int8_t)HWEEPROMRead(hw_control, EEPROM_CH_11_12_DIFF_OFFSET)& 0xFF00)>>8; 
            CHDiffPower.CH13_DIFF_POWER = (int8_t)HWEEPROMRead(hw_control, EEPROM_CH_13_14_DIFF_OFFSET)& 0xFF;
            CHDiffPower.CH14_DIFF_POWER = ((int8_t)HWEEPROMRead(hw_control, EEPROM_CH_13_14_DIFF_OFFSET)& 0xFF00)>>8;  
        }
        else
        {
            CHDiffPower.CH1_DIFF_POWER = 0; 
            CHDiffPower.CH2_DIFF_POWER = 0; 
            CHDiffPower.CH3_DIFF_POWER = 0; 
            CHDiffPower.CH4_DIFF_POWER = 0; 
            CHDiffPower.CH5_DIFF_POWER = 0; 
            CHDiffPower.CH6_DIFF_POWER = 0; 
            CHDiffPower.CH7_DIFF_POWER = 0; 
            CHDiffPower.CH8_DIFF_POWER = 0; 
            CHDiffPower.CH9_DIFF_POWER = 0; 
            CHDiffPower.CH10_DIFF_POWER = 0;
            CHDiffPower.CH11_DIFF_POWER = 0;
            CHDiffPower.CH12_DIFF_POWER = 0;
            CHDiffPower.CH13_DIFF_POWER = 0;
            CHDiffPower.CH14_DIFF_POWER = 0;
        }
    }
    else
    {
        CHDiffPower.CH1_DIFF_POWER = 0; 
        CHDiffPower.CH2_DIFF_POWER = 0; 
        CHDiffPower.CH3_DIFF_POWER = 0; 
        CHDiffPower.CH4_DIFF_POWER = 0; 
        CHDiffPower.CH5_DIFF_POWER = 0; 
        CHDiffPower.CH6_DIFF_POWER = 0; 
        CHDiffPower.CH7_DIFF_POWER = 0; 
        CHDiffPower.CH8_DIFF_POWER = 0; 
        CHDiffPower.CH9_DIFF_POWER = 0; 
        CHDiffPower.CH10_DIFF_POWER = 0;
        CHDiffPower.CH11_DIFF_POWER = 0;
        CHDiffPower.CH12_DIFF_POWER = 0;
        CHDiffPower.CH13_DIFF_POWER = 0;
        CHDiffPower.CH14_DIFF_POWER = 0;
    }
        
    return CHDiffPower;
}

