/*
 * SCI_Init.c
 *
 *  Created on: 2 A�u 2019
 *      Author: Tovura
 */

#include "F2806x_Device.h"
#include "F2806x_Sci.h"
#include "SCI_Init.h"
#include "vcu0_crc.h"
#include "stdlib.h"
#include "..\..\Tasks/MesAnalyze/include/T_MesAnalyze.h"
#include "..\..\Tasks/ModBus/include/ModBus.h"
#include "..\..\Tasks/SM/include/T_SM.h"
#include "DSP2DSP.h"
#include "eCAN_Init.h"
#include "DFU.h"
#include "stdlib.h"
#include "System_Init.h"
#include "WDT.h"


#define SCI_IDLE_LINE_DETECTION_TIME_MS (20)
#define SCI_GUI_TIMEOUT_MS  (500-40)
#define SCI_RX_TX_BUFLEN (256 + 12)

#pragma SET_CODE_SECTION("ramfuncs")
__interrupt void ISR_SCIB_RX(void);
__interrupt void ISR_SCIB_TX(void);
#pragma SET_CODE_SECTION()

#pragma CODE_SECTION(SCIB_Handle_Slave,"ramfuncs");
static void SCIB_Handle_Slave(void);

#pragma CODE_SECTION(SCIB_InitiateTX,"ramfuncs");
static void SCIB_InitiateTX(void);

void* TASK_SCI_CTRL = 0;

extern volatile Uint32 LocalTime;

struct SCI_RecData{        // SCI Data Format.

    Uint16  ModuleNo;
    Uint16  OpCode;
    Uint16  Length;
    Uint16  Crc;
    Uint16  Data[SCI_RX_TX_BUFLEN];
    Uint16  Counter;
    Uint32  Last_RecTime;
};

struct SCI_SentData{
    Uint16 Data[SCI_RX_TX_BUFLEN];     // Pointer to Dynamically Allocated Memory Containing, Data to Send.
    Uint16 Counter;
    Uint16 Length;    // Length Of the Data without Header or CRC.
    Uint16 Crc;
};

struct  SCI_Data{
    struct SCI_RecData  RecData;     // Received Data must be formatted as above structure.
    struct SCI_SentData SentData;    //SentData must be formed as a whole.
};
struct SCI_Data SCIB;

volatile bool MODBUS_EVLOGREQUEST_RECIEVED = false;
volatile bool MODBUS_EVLOGREQUEST_SERVED   = false;
volatile uint16_t MODBUS_EVLOGREQUEST_NUMOFEVENTS = 0;

void SYS_SCIInit(void)
{
    uint16_t cnt = 0;

    SCIB.RecData.OpCode  = 0;
    SCIB.RecData.Length  = 0;
    SCIB.RecData.Crc     = 0;
    SCIB.RecData.Counter = 0;
    SCIB.RecData.Last_RecTime= 0;

    for(cnt = 0; cnt<SCI_RX_TX_BUFLEN;cnt++)
    {
        SCIB.RecData.Data[cnt] = 0;
    }

    SCIB.SentData.Counter = 0;
    SCIB.SentData.Length  = 0;
    SCIB.SentData.Crc = 0;

    EALLOW;

    SysCtrlRegs.PCLKCR0.bit.SCIBENCLK   = 1;

    ScibRegs.SCICTL1.bit.SWRESET        = 0;  // Pull the Lines to High.

    ScibRegs.SCICCR.bit.SCICHAR         = 7;  // 8-Bit Data.
    ScibRegs.SCICCR.bit.ADDRIDLE_MODE   = 0;  // Idle-Line Protocol Mode.
    ScibRegs.SCICCR.bit.LOOPBKENA       = 0;  // Loop-Back Test Mode Disabled.

    ScibRegs.SCICCR.bit.PARITY          = 0;  // No Parity Bit.
    ScibRegs.SCICCR.bit.STOPBITS        = 0;  // 1 Stop Bit.


    ScibRegs.SCICTL1.bit.RXENA          = 1;  // Enable Receiver.
    ScibRegs.SCICTL1.bit.TXENA          = 1;  // Disable Transmitter.
    ScibRegs.SCICTL1.bit.RXERRINTENA    = 1;  // Enable RX Errors Interrupt.
    ScibRegs.SCICTL1.bit.SLEEP          = 1;  // Sleep Mode is Disabled --> 0, Enable -->1(Prevent Further Interrupts from Occuring.).

    /*
    ScibRegs.SCIHBAUD                   = 0x01;  // 19200 Baud - Rate.
    ScibRegs.SCILBAUD                   = 0x24;
    */

    ScibRegs.SCIHBAUD                   = 0x00;  // 115200 Baud - Rate.
    ScibRegs.SCILBAUD                   = 0x30;

    ScibRegs.SCICTL2.bit.RXBKINTENA     = 1;  // Enable RX(Buffer-Break)/Disable TX Interrupts.
    ScibRegs.SCICTL2.bit.TXINTENA       = 1;
    ScibRegs.SCIPRI.bit.FREE            = 0;  // Free-Soft: Immediate Stop at Emulation Suspend.
    ScibRegs.SCIPRI.bit.SOFT            = 0;

    ScibRegs.SCICTL1.bit.SWRESET        = 1;  // Release Lines.

    PieVectTable.SCIRXINTB = ISR_SCIB_RX;
    PieVectTable.SCITXINTB = ISR_SCIB_TX;

    EDIS;
}

__interrupt void ISR_SCIB_RX(void)
{
    /**
     * SCI Idle-Line Mode Configurations are made.
     *  1.) Check RXWAKE flag to Decide whether to perform Address Check or Not, If so Check the Address and set sleep bit accordingly.
     *  This will reduce the CPU ISR Serving Overhead.
     *  2.) If RXWAKE is not Set, means Data has come. Take Data, Form corresponding SCI Reception Data Structure.
     *  3.) Upon Completion,after last byte is received, call SCI Slave Handler which starts the response to the query.
     *
     */

    if(ScibRegs.SCIRXST.bit.RXERROR)
    {
        // Either Overrun, Framing Error, Parity Error or Break Condition has occured. Handle Properly.
        if(ScibRegs.SCIRXST.bit.BRKDT)
        {
            GpioDataRegs.GPASET.bit.GPIO8 = 1;
        }
        ScibRegs.SCICTL1.bit.SWRESET= 0;
        ScibRegs.SCICTL1.bit.SWRESET =1;

        ScibRegs.SCICTL1.bit.SLEEP = 1; // Prevent Interrupts from Occuring at Data Reception.
    }
    else
    {
        if(ScibRegs.SCIRXST.bit.RXWAKE)
        {
            //  Received is an Address Data.

            SCIB.RecData.ModuleNo = ScibRegs.SCIRXBUF.bit.RXDT;
            SCIB.RecData.Data[0] = SCIB.RecData.ModuleNo;
            SCIB.RecData.Counter = 0;

            if(SCIB.RecData.ModuleNo == (RAM_NoInit.Bits.UPS_Address.Bits.Addr + UPS_MODULE_ADDR_INVOFFSET) || (SCIB.RecData.ModuleNo == UPS_MODULE_ADDR_INVOFFSET))
            {
                ScibRegs.SCICTL1.bit.SLEEP = 0; // Let further Interrupts to Occur, (WKUP-->Enable Interrupt.)
            }
            else
            {
                ScibRegs.SCICTL1.bit.SLEEP = 1; // Prevent Interrupts from Occuring at Data Reception.
            }
            SCIB.RecData.Last_RecTime = LocalTime;
        }
        else
        {
            // Data Bytes Reception.

            if(SCIB.RecData.Counter % 2)
            {
                SCIB.RecData.Data[SCIB.RecData.Counter/2] += ((ScibRegs.SCIRXBUF.bit.RXDT & 0x00FF)<<8);
            }
            else
            {
                SCIB.RecData.Data[SCIB.RecData.Counter/2]  = ScibRegs.SCIRXBUF.bit.RXDT;
            }

            SCIB.RecData.Last_RecTime = LocalTime;
        }

        SCIB.RecData.Counter++;
        if(SCIB.RecData.Counter >= (2*SCI_RX_TX_BUFLEN))
        {
            SCIB.RecData.Counter = 0;
        }
    }
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}

__interrupt void ISR_SCIB_TX(void)
{
    SCIB.SentData.Counter++;
    if(SCIB.SentData.Counter <= SCIB.SentData.Length)
    {
        if(SCIB.SentData.Counter % 2 == 0)
        {
            ScibRegs.SCITXBUF = ((*(SCIB.SentData.Data +SCIB.SentData.Counter / 2))&0x00FF);
        }
        else
        {
            ScibRegs.SCITXBUF = (((*(SCIB.SentData.Data +SCIB.SentData.Counter / 2))>>8)&0x00FF);
        }
    }
    else
    {
        if(MODBUS_EVLOGREQUEST_RECIEVED)
        {
            MODBUS_EVLOGREQUEST_RECIEVED = false;
            MODBUS_EVLOGREQUEST_SERVED = true;
        }

        GpioDataRegs.GPASET.bit.GPIO8 = 1;
    }
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}

uint32_t SCI_LastRecTime_forTX = 0;

void TASK_SCI_Manager(void)
{
    uint16_t tmp[2],Calc_Crc;

    // Idle Detected, Process Recieved Data, Send Response.

    SCIB.RecData.Length = SCIB.RecData.Counter;
    SCIB.RecData.OpCode = (SCIB.RecData.Data[0] & 0xFF00)>>8;

    if(SCIB.RecData.Length % 2 == 0)
    {
        tmp[0] =  SCIB.RecData.Data[SCIB.RecData.Length / 2 - 1] & 0x00FF;
        tmp[1] = (SCIB.RecData.Data[SCIB.RecData.Length / 2 - 1] & 0xFF00) >> 8;
    }
    else
    {
        tmp[0] = (SCIB.RecData.Data[SCIB.RecData.Length /2 -1] & 0xFF00) >> 8;
        tmp[1] = (SCIB.RecData.Data[SCIB.RecData.Length /2]) & 0x00FF;

    }
    SCIB.RecData.Crc = ((tmp[0] << 8)&0xFF00) + (tmp[1]&0x00FF);

    if(SCIB.RecData.Length < 3)
    {
        SCIB.RecData.Last_RecTime = 0;
        return;
    }

   Calc_Crc =  getCRC16P1_vcu(0,SCIB.RecData.Data,CRC_parity_even,SCIB.RecData.Length-2);

   if(SCIB.RecData.Crc != Calc_Crc)
   {
       //CRC Error.
   }
   else
   {
       SCIB_Handle_Slave();
   }
}
bool TASK_SCI_CondCheck(void)
{
    static uint32_t LastCheckTime = 0;

    if(LocalTime - LastCheckTime > 9)
    {
        LastCheckTime = LocalTime;

        if(SCIB.RecData.Last_RecTime != 0 && (LocalTime - SCIB.RecData.Last_RecTime > SCI_IDLE_LINE_DETECTION_TIME_MS))
        {
            SCI_LastRecTime_forTX = SCIB.RecData.Last_RecTime;

            if(LocalTime - SCIB.RecData.Last_RecTime < SCI_GUI_TIMEOUT_MS)
            {
                SCIB.RecData.Last_RecTime = 0;
                return true;
            }

            SCIB.RecData.Last_RecTime = 0;
        }
    }
    return false;
}

static void SCIB_Handle_Slave(void)
{
    /**
     * This is where;
     *  1.) The Received Data is Processed according to OpCode.
     *  2.) Length of the 'SentData.Length' Decided.
     *  3.) Memory Allocation is done.
     *  4.) 'SentData.Data' is filled with its content.
     */

    uint16_t cnt        = 0;
   volatile uint16_t Address    = 0;
   volatile uint16_t ReadLen    = 0;
   volatile uint16_t WriteLen   = 0;
    uint32_t tmp;
    volatile uint16_t addrNew;

    //Address Region for ModBus Write Operations.
    static uint32_t ModBUS_RW_Region_Addr0  = (uint32_t)&ModBus_Table.Structure.User_CMD - (uint32_t)&ModBus_Table.All[0];
    static uint32_t ModBus_RW_Region_Addr1  = (uint32_t)&ModBus_Table.Structure.EvenstLog[0] - (uint32_t)&ModBus_Table.All[0];

    switch(SCIB.RecData.OpCode)
    {
    case 0x03:

        /*
         * ReadLen is the #Floating Point Registers to Read !
         * SCIB:SentData:Length is 4*ReadLen + HeaderLength(6),Since We are Sending byte by byte.
         */

        Address = ((SCIB.RecData.Data[1]>> 8)&0x00FF) + ((SCIB.RecData.Data[1]<<8)&0xFF00); // Address Offset for Starting of Read Operation.
        ReadLen = ((SCIB.RecData.Data[2]>> 8)&0x00FF) + ((SCIB.RecData.Data[2]<<8)&0xFF00); // Number of Registers to Read.

        SCIB.SentData.Length = 4*ReadLen + 6;

        if(((2*(Address + ReadLen)) <=  sizeof(ModBus_Table_t)) &&  (SCIB.SentData.Length <= (512+6)))
        {
            *(SCIB.SentData.Data)       = (RAM_NoInit.Bits.UPS_Address.Bits.Addr + UPS_MODULE_ADDR_INVOFFSET);
            *(SCIB.SentData.Data)       += (0x03<<8);
            *(SCIB.SentData.Data +1 )   = ((SCIB.SentData.Length-6)<<8)&0xFF00;


            for(cnt = 0; cnt<2*ReadLen;cnt++)
            {
                *(SCIB.SentData.Data + cnt + 2) = *(((uint16_t*)&ModBus_Table.All[0]) + 2*Address + cnt);
            }

            SCIB.SentData.Crc = getCRC16P1_vcu(0,SCIB.SentData.Data,CRC_parity_even,SCIB.SentData.Length - 2);
            *(SCIB.SentData.Data+cnt+2) = ((SCIB.SentData.Crc <<8)&0xFF00)+((SCIB.SentData.Crc>>8)&0x00FF);

            if((Address+ReadLen) > MODBUSTABLE_EVENTSLOG_FPADDRESSOFFSET)
            {
                MODBUS_EVLOGREQUEST_RECIEVED    = true;
                MODBUS_EVLOGREQUEST_NUMOFEVENTS = ((Address+ReadLen)-MODBUSTABLE_EVENTSLOG_FPADDRESSOFFSET)/4;
            }
            else
            {
                MODBUS_EVLOGREQUEST_RECIEVED    = false;
            }

            if(SCIB.RecData.ModuleNo == (RAM_NoInit.Bits.UPS_Address.Bits.Addr + UPS_MODULE_ADDR_INVOFFSET))
            {
                SCIB_InitiateTX();
            }
        }
        break;
    case 0x10:
        Address  = ((SCIB.RecData.Data[1]>> 8)&0x00FF) + ((SCIB.RecData.Data[1]<<8)&0xFF00);
        WriteLen = ((SCIB.RecData.Data[2]>> 8)&0x00FF) + ((SCIB.RecData.Data[2]<<8)&0xFF00);

        SCIB.SentData.Length        = SCIB.RecData.Length;

        if((2*Address) >= ModBUS_RW_Region_Addr0  && (2*(Address+WriteLen)) <= ModBus_RW_Region_Addr1  ) // Check Requested Write Region
        {
            for(cnt = 0; cnt<WriteLen;cnt++)
            {

                tmp = ((((uint32_t)SCIB.RecData.Data[3+2*cnt]) & 0x00FF) << 24 ) + ((((uint32_t)SCIB.RecData.Data[3+2*cnt]) & 0xFF00) << 8 )
                    + ((((uint32_t)SCIB.RecData.Data[4+2*cnt]) & 0x00FF) << 8  ) + ((((uint32_t)SCIB.RecData.Data[4+2*cnt]) & 0xFF00) >> 8);

                *(uint32_t*)&ModBus_Table.All[Address+cnt] = tmp;
            }

            for(cnt = 0; cnt<SCIB.RecData.Length;cnt++)
            {
                *(SCIB.SentData.Data+cnt) = SCIB.RecData.Data[cnt];
            }

            ModBus_HandleWrites(Address,WriteLen);

            if(SCIB.RecData.ModuleNo == (RAM_NoInit.Bits.UPS_Address.Bits.Addr + UPS_MODULE_ADDR_INVOFFSET))
            {
                SCIB_InitiateTX();
            }
        }
        break;
    case 0xFF:

        Address = ((SCIB.RecData.Data[1]>> 8)&0x00FF) + ((SCIB.RecData.Data[1]<<8)&0xFF00); //This is OpCode identifier for 0xFF Special OpCodes, Not an Address.

        switch(Address)
        {
        case 1: // DFU

            _Handle_DFU_Comm((DFU_Pack_t*)&SCIB.RecData.Data[2]);

            SCIB.SentData.Length = 14;

            *(SCIB.SentData.Data)       = 0xFF00;
            *(SCIB.SentData.Data)      += (RAM_NoInit.Bits.UPS_Address.Bits.Addr + UPS_MODULE_ADDR_INVOFFSET);
            *(SCIB.SentData.Data +1 )   = 0x0100;

            memcpy(SCIB.SentData.Data +2,&RAM_NoInit.Bits.DFU_Info,sizeof(DFU_PackInfo_t));

            SCIB.SentData.Crc = getCRC16P1_vcu(0,SCIB.SentData.Data,CRC_parity_even,SCIB.SentData.Length - 2);
            *(SCIB.SentData.Data+6) = ((SCIB.SentData.Crc <<8)&0xFF00)+((SCIB.SentData.Crc>>8)&0x00FF);

            if(SCIB.RecData.ModuleNo == (RAM_NoInit.Bits.UPS_Address.Bits.Addr + UPS_MODULE_ADDR_INVOFFSET))
            {
                SCIB_InitiateTX();
            }
            break;

        case 2: // Addressing
            if((SM_Interface.State == Init || SM_Interface.State == Fault) && SM_Interface.User_Commands.Force_UPS_Shutdown)
            {
                // Update Address Field in EEPROM and RAM, Restart.
                addrNew = ((SCIB.RecData.Data[2]>> 8)&0x00FF) + ((SCIB.RecData.Data[2]<<8)&0xFF00);
                if(addrNew <= (DEFAULT_UPS_ADDR + UPS_MODULE_ADDR_INVOFFSET) && addrNew > UPS_MODULE_ADDR_INVOFFSET)
                {
                    RAM_NoInit.Bits.UPS_Address.Bits.Addr = addrNew - UPS_MODULE_ADDR_INVOFFSET;
                    RAM_NoInit.Bits.UPS_Address.Bits.KEY  = ADDRESS_VALIDATION_KEY;
                    RAM_NoInit.Bits.UPS_Address.Bits.CRC16= getCRC16P1_vcu(0,RAM_NoInit.Bits.UPS_Address.All,CRC_parity_even,4);

                    EEPROM_Interface.Write_Page(RAM_NoInit.Bits.UPS_Address.All,MEM_EEPROM_PAGE_ADDRESS(8),3);

                    RAM_NoInit.Bits.ResetSource.Bits.Addressing = 1;
                    WDT_Interface.SWReset();
                }
            }

        default:
            break;
        }

        break;
    default:
        BKPT;
        return;
    }
}

static void SCIB_InitiateTX(void)
{
    // This function initiates TX,it must be used after Data buffer is formed and Length is Decided.

    if((LocalTime - SCI_LastRecTime_forTX) < SCI_GUI_TIMEOUT_MS)
    {
        SCIB.SentData.Counter = 0;

        GpioDataRegs.GPACLEAR.bit.GPIO8 = 1;
        while(!ScibRegs.SCICTL2.bit.TXRDY);
        ScibRegs.SCITXBUF = (*SCIB.SentData.Data) & 0x00FF;
    }
    else
    {
        MODBUS_EVLOGREQUEST_RECIEVED = false;
        MODBUS_EVLOGREQUEST_NUMOFEVENTS = 0;
    }
}

