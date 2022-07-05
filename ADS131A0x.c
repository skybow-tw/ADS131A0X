
#include <stdint.h>
#include <stdio.h>

#include "ADS131A0x.h"

// include this wrapper(a linux SPI userspace driver) to create a API for user

// Constants sould be defined with MACRO, where it doesn't declare a variable nor occupy any memory spaces,
// it just replace the keyword with conatant by the Pre-processor

#define LENGTH_DEVICEWORD 3                               // 3 Bytes=24bits
#define SIZE_DATAFRAME_A04 6                              // FIXED=1, the number of device word per data frame is 6(for ADS131A04)
#define SIZE_DATAFRAME_A02 4                              // FIXED=1, the number of device word per data frame is 6(for ADS131A02)
#define SIZE_SYSCMD LENGTH_DEVICEWORD *SIZE_DATAFRAME_A04 // 3 Bytes * 6 = 18 Bytes
#define VREF_EXT 2.5
#define fullscale_24bit 8388608.0

//---------Register Address---------:
// Read Only ID Registers
#define RegAdr_ID_MSB 0x00
#define RegAdr_ID_LSB 0x01
// Status Registers
#define RegAdr_STAT_1 0x02
#define RegAdr_STAT_P 0x03
#define RegAdr_STAT_N 0x04
#define RegAdr_STAT_S 0x05
#define RegAdr_ERROR_CNT 0x06
#define RegAdr_STAT_M2 0x07
// User Configuration Registers
#define RegAdr_A_SYS_CFG 0x0B
#define RegAdr_D_SYS_CFG 0x0C
#define RegAdr_CLK1 0x0D
#define RegAdr_CLK2 0x0E
#define RegAdr_ADC_ENA 0x0F
#define RegAdr_ADC1 0x11
#define RegAdr_ADC2 0x12
#define RegAdr_ADC3 0x13
#define RegAdr_ADC4 0x14
//---------System Commands---------:
uint8_t CMD_NULL[3] = {0x00, 0x00, 0x00};
uint8_t CMD_RESET[3] = {0x00, 0x11, 0x00};
uint8_t CMD_STANDBY[3] = {0x00, 0x22, 0x00};
uint8_t CMD_WAKEUP[3] = {0x00, 0x33, 0x00};
uint8_t CMD_LOCK[3] = {0x05, 0x55, 0x00};
uint8_t CMD_UNLOCK[3] = {0x06, 0x55, 0x00};
// Register Write&Read Commands:
uint8_t CMD_RREG[3] = {0x20, 0x00, 0x00};      // 001a aaaa 0000 0000 (a=address)
uint8_t CMD_RREGS[3] = {0x20, 0xFF, 0x00};     // 001a aaaa nnnn nnnn (Read "nnnn nnnn+1" registers  start at address "a aaaa")
uint8_t CMD_WREG_Mask[3] = {0x40, 0x00, 0x00}; // 010a aaaa dddd dddd (Write data "dddd dddd" at address "a aaaa")
uint8_t CMD_WREGS[3] = {0x60, 0x00, 0x00};     // 011a aaaa nnnn nnnn (Write"nnnn nnnn+1" registers  begin at address "a aaaa";
// additional device word="dddd dddd eeee eeee", dddd dddd for register at address "a", and eeee eeee for address "a+1")
uint8_t CMD_RREG_STAT_S[3] = {0x25, 0x00, 0x00}; // 001a aaaa 0000 0000 (a=address)

//-----Parameters-----
/*Declare 2 buffer storing SPI System Commands("aSysCmd") and Command Status Responses("aResponse") ;
The size of these 2 char array is set to SIZE_SYSCMD, which is defined by 2 programmable system parameters (Device word length=24bit and FIXED frame mode),
therefore SIZE_SYSCMD = 3(Bytes/DeviceWord) * 6(DeviceWord/DataFrame) = 18 (Bytes/DataFrame)*/
uint8_t aSysCmd[SIZE_SYSCMD] = {0};
uint8_t aResponse[SIZE_SYSCMD] = {0};
uint8_t aNULLCmd[SIZE_SYSCMD] = {0};
long ADC1_raw = 0, ADC2_raw = 0, ADC3_raw = 0, ADC4_raw = 0;

// for Debug Serial out Message
char arySerialMsg[100] = {0}; // Traditional C char array for serial communication about ADS131A04 SysCmd

//=======================================================Functions==================================================
/*NOTE: sprintf use the format like: %[flags][width][.precision][length]specifier;
    (1)flags:
    "0"= pedding zero at left side
    "#"= Use "0X" prefix(if the specifier is X)
    (2)width:
    6=set the minumum width to be print(including the width of "0X" prefix)
    (4)length: 3 selection
    h=>the argument is interpreted as short int or unsigned short int,
    l=>the argument is interpreted as long int or unsigned long int ,
    L=>the argument is interpreted as long duble
    (5)specifier:
    X=hexadecimal
*/

/*SPI Speed = 3.814kHz ~ 125MHz
(250MHZ/CLKDIV, where CLKDIV= 2,4,6,...65534 )
*/
void ADS131A0x_setSPI(enum ChipSelectPin CS_Pin, uint64_t Speed)
{
    // file descriptor for SPI
    int spiFd = -1;

    // set CS=0,speed=2MHz,SPI MODE=1
    spiFd = SPI_Setup(CS_Pin, Speed, 1);

    printf("RPi4B SPI Init with CS_pin:%d / speed:%dHZ ", CS_Pin, Speed);
    printf(spiFd > 0 ? "success" : "failed");
    printf(" at FD=%d\n", spiFd);
}
//-------------------------------------------------------
/*
 *NOTE: WREG command=010a aaaa dddd dddd
 * (Write data "dddd dddd" at address "a aaaa")
 */
void ADS131A0x_WREG(uint8_t reg_adr, uint8_t wrt_data)
{
    /*Write-Read: Write WREG command, and Write-Read the NULL command's response*/
    // STEP1:
    memset(aSysCmd, 0, SIZE_SYSCMD); // Initialize aSysCmd array
    // Perform OR operation between WREG-MASK and 5-bit register address( 0100 0000 | 000a aaaa),
    // which would create a 8-bit command that set the to-be-written Regiser Address
    aSysCmd[0] = CMD_WREG_Mask[0] | reg_adr;
    aSysCmd[1] = wrt_data; // set Regiser Data to Write
    SPI_ReadWrite(0, aSysCmd, aResponse, SIZE_SYSCMD);

    // STEP2:
    // Initialize aResponse array
    memset(aResponse, 0, SIZE_SYSCMD);

    // Send NULL command
    SPI_ReadWrite(0, aNULLCmd, aResponse, SIZE_SYSCMD);

    /*
    In aResponse data array, the first 3 Bytes, elements [0],[1],[2] is "Status Response"; and
    [3],[4],[5]=ADC1; [6],[7],[8]=ADC2; [9],[10],[11]=ADC3;[12],[13],[14]=ADC4; [15],[16],[17]=CRC(optional)
    */

    // ADC1_raw=(uint32_t(aResponse[3])<<16)|(aResponse[4]<<8)|aResponse[5];

    // 2 complement conversion (divide by 256 equals to ">>8" bitwise operation, which would automatically do sign-extension
    ADC2_raw = (((uint32_t)aResponse[6]) << 24 | ((uint32_t)aResponse[7]) << 16 | ((uint32_t)aResponse[8]) << 8) / 256;

    sprintf(arySerialMsg, "WREG_Resp=%#04X,ADC2=%0#10lX \n\n", ((aResponse[0] << 8) | aResponse[1]), ADC2_raw);

    // printf(arySerialMsg);
}
//-------------------------------------------------------
int ADS131A0x_RREG(uint8_t reg_adr)
{
    memset(aSysCmd, 0, SIZE_SYSCMD); // Initialize aSysCmd array
    int32_t rawdata = 0;

    SPI_ReadWrite(0, aSysCmd, aResponse, SIZE_SYSCMD);

    rawdata = (aResponse[0] << 8) | aResponse[1];
    return rawdata;
}
//-------------------------------------------------------
void ADS131A0x_SYSCMD(char SysCmdSel)
{

    memset(aSysCmd, 0, SIZE_SYSCMD); // Initialize aSysCmd array

    printf("Sending SysCmd:");
    switch (SysCmdSel)
    {
    /*Copy the 3-Bytes system command CMD_XXXXXX and overwrite the first 3 Bytes of the "aSysCmd" array "*/
    case 'N':
        printf("NULL\n");
        memcpy(aSysCmd, CMD_NULL, 3);
        break;

    case 'R':
        printf("RESET\n");
        memcpy(aSysCmd, CMD_RESET, 3);
        break;

    case 'Y':
        printf("STANDBY...\n");
        memcpy(aSysCmd, CMD_STANDBY, 3);
        break;

    case 'W':
        printf("WAKEUP!\n");
        memcpy(aSysCmd, CMD_WAKEUP, 3);
        break;

    case 'L':
        printf("LOCK!\n");
        memcpy(aSysCmd, CMD_LOCK, 3);
        break;

    case 'U':
        printf("UNLOCK!\n");
        memcpy(aSysCmd, CMD_UNLOCK, 3);
        break;

    case 'r': // read Register STAT_S and clear the error bit
        printf("Read STAT_S(SPI)\n");
        memcpy(aSysCmd, CMD_RREG_STAT_S, 3);
        break;

    default:
        printf("Unknown SYSCMD!\n");
        break;
    }

    // from a wrapper of Linux userspace driver
    // Write the System command, but ignore the response
    SPI_ReadWrite(0, aSysCmd, aResponse, SIZE_SYSCMD);

    /*Write-Read: Write Null command, and Read the response*/

    // Initialize aResponse array
    memset(aResponse, 0, SIZE_SYSCMD);

    // Send NULL command
    SPI_ReadWrite(0, aNULLCmd, aResponse, SIZE_SYSCMD);

    // ADC1_raw=(uint32_t(aResponse[3])<<16)|(aResponse[4]<<8)|aResponse[5];
    ADC1_raw = 17; // test only

    if (SysCmdSel == 'N')
    {
        // Response is always 16 bit; ADC raw data is set to 24bits
        sprintf(arySerialMsg, "NULL_Resp=%#04X,ADC1=%0#10lX \n\n", ((aResponse[0] << 8) | aResponse[1]), ADC1_raw);
        printf(arySerialMsg);
    }
    else
    {

        sprintf(arySerialMsg, "Status_Resp=%0#6lX \n\n", (((uint32_t)aResponse[0]) << 8 | aResponse[1]));
        printf(arySerialMsg);
    }
}
//-------------------------------------------------------
void ADS131A0x_InitialADC()
{
    // ADS131A0x_SYSCMD('N');//NULL
    ADS131A0x_SYSCMD('U'); // UNLOCK
    // ADS131A0x_SYSCMD('N');
    ADS131A0x_SYSCMD('r'); // read register STAT_S and clear the error bit
    ADS131A0x_SYSCMD('N');

    // ADC Initialization
    printf("set CLK_DIV=2\n");
    ADS131A0x_WREG(RegAdr_CLK1, 0x02); // set CLK_DIV=2
    printf("set ICLK_DIV=4 and OSR=4096=> Data Rate=500Hz\n");
    ADS131A0x_WREG(RegAdr_CLK2, 0x40); // set ICLK_DIV=4 and OSR=4096=> Data Rate=500Hz

    // set Internal reference voltage (The 3rd bit , "INT_REFEN =1 => 0110 1000b= 0x68)
    // ADS131A0x_WREG(RegAdr_A_SYS_CFG,0x68);

    // Negative charge pump enable, others remain Default( High Reso. /RFEP=2.442V/ use EXT VREF,which is from EVM onboard 2.5V)
    printf("set Negative charge pump enable\n");
    ADS131A0x_WREG(RegAdr_A_SYS_CFG, 0xE0);
}
//-------------------------------------------------------
void ADS131A0x_Start()
{
    printf("Enable ADC channels\n");
    ADS131A0x_WREG(RegAdr_ADC_ENA, 0x0F); // enable ADC channels (0000 1111b=0x0f)
    ADS131A0x_SYSCMD('W');                // WAKEUP
}
//-------------------------------------------------------
void ADS131A0x_Stop()
{
    ADS131A0x_SYSCMD('Y');                // STANDBY
    ADS131A0x_WREG(RegAdr_ADC_ENA, 0x00); // disable ADC channels
}
//-------------------------------------------------------
void ADS131A0x_GetADCData(uint8_t Mode, float *DataBuffer)
{

    switch (Mode)
    {
    case 0:
        // Simulation
        aResponse[3] = 0xFE;
        aResponse[4] = 0x79;
        aResponse[5] = 0x60;

        aResponse[6] = 0xFF;
        aResponse[7] = 0xFC;
        aResponse[8] = 0x18;

        break;

    case 1:

        // In aResponse data array, the first 3 Bytes [0],[1],[2]=Status Response;
        //[3],[4],[5]=ADC1; [6],[7],[8]=ADC2; [9],[10],[11]=ADC3;[12],[13],[14]=ADC4; [15],[16],[17]=CRC(optional)

        // Initialize aResponse array
        memset(aResponse, 0, SIZE_SYSCMD);
        // Initialize aSysCmd array
        memset(aSysCmd, 0, SIZE_SYSCMD);

        // Send NULL command
        SPI_ReadWrite(0, aNULLCmd, aResponse, SIZE_SYSCMD);

        ADC1_raw = ConvertInt_24to32((aResponse + 3));
        // aData_ADC[0] = ADC1_raw / fullscale_24bit * VREF_EXT;
        DataBuffer[0] = ADC1_raw / fullscale_24bit * VREF_EXT;

        ADC2_raw = ConvertInt_24to32((aResponse + 6));
        // aData_ADC[1] = ADC2_raw / fullscale_24bit * VREF_EXT;
        DataBuffer[1] = ADC2_raw / fullscale_24bit * VREF_EXT;

        ADC3_raw = ConvertInt_24to32((aResponse + 9));
        // aData_ADC[2] = ADC3_raw / fullscale_24bit * VREF_EXT;
        DataBuffer[2] = ADC3_raw / fullscale_24bit * VREF_EXT;

        ADC4_raw = ConvertInt_24to32((aResponse + 12));
        // aData_ADC[3] = ADC4_raw / fullscale_24bit * VREF_EXT;
        DataBuffer[3] = ADC4_raw / fullscale_24bit * VREF_EXT;

        break;
    default:
        // Do nothing
        break;
    }
}

//-------------------------------------------------------
/*
 *Converts 24-bits integer to 32-bits integer,where the compiler would automatically perform the sign-extension(?)
 *It receive an uint8_t array pointer "byteArray", it implies the address of the first element,
 *therefore byteArray[0],byteArray[1],byteArray[2] are 3 consecutive memory address that forms a 24-bits integer of an ADC channel
 */
int32_t ConvertInt_24to32(uint8_t *byteArray)
{
    // uint8_t temp001;
    /*
    printf(byteArray[0],HEX);
    printf(',');
    printf(byteArray[1],HEX);
    printf(',');
    printf(byteArray[2],HEX);
    printf(',');
    */

    // If MSB==1, it's a negative number, then do the 2's complement conversion:  flip all the bits, plus 1, and multiply by "-1"
    if (((byteArray[0] >> 7) & 0x01) == 1)
    {

        // Method1: bitwise NOT operator
        byteArray[0] = ~byteArray[0];
        byteArray[1] = ~byteArray[1];
        byteArray[2] = ~byteArray[2];

        // Method2: XOR operator
        //  byteArray[0] ^=0xff;
        //  byteArray[1] ^=0xff;
        //  byteArray[2] ^=0xff;
        return -1 * ((((uint32_t)byteArray[0]) << 16 | ((uint32_t)byteArray[1]) << 8 | byteArray[2]) + 1);
    }
    else
    {
        return (((uint32_t)byteArray[0]) << 16 | ((uint32_t)byteArray[1]) << 8 | byteArray[2]);
    }
}
