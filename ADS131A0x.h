
/*
<Purpose>: API for TI ADS131A04 (24-bit ADC) via SPI interface

<Hardware>:
1.TI ADS131A04EVM
2.RaspberryPi4B_4G
**********************************
SPI Settings:
(1)SCLK=2MHZ
(2)SPI MODE=1
(3)MSB-First
(4)Data length:23bit


<Connection>:
Description       ADS131A04EVM       RPi4B
---------------------------------------------
GND               AGND               GND
VDD               VDD                +5V
MOSI              MISO               MOSI
MISO              MOSI               MISO    
SCLK                                 
CS                                   Pin10


V0.1.0
2021/Dec/15th by Chi-Hao Lee
 */

#include <string.h>
#include <stdint.h>

/*
How to set up Sampling Rate?
1.f_CLKIN(external clock)=16.384MHz=> this is fixed, which is generated from crystal
2.f_ICLK(Internal System Clock)=f_CLKIN/CLK_DIV=16384kHz/2=8192kHz; CLK_DIV=2=>Register CLK1(address:0x0D)=0000 0010b=0x02
3.f_MOD(MODulator Clock)=f_ICLK/ICLK_DIV=8192kHz/4=2048kHz; ICLK_DIV=4 =>Register CLK2(address:0x0E) bit[7:5]=010
4.Data Rate=f_MOD/OSR=2048kHz/4096=0.5kHz=500Hz; OSR(OverSampling Ratio)=4096=>Register CLK2(address:0x0E) bit[3:0]=0000
=>Combine with f_MOD setting, Register CLK2 should be set to 0100 0000b=0x40
*/

/*Method2: Use WREG_Mask and bitwise OR operation
extern uint8_t CMD_WREG_temp[3]={0x00,0x00,0x00};//Initialize a temp WREG command; 010a aaaa dddd dddd (Write data "dddd dddd" at address "a aaaa")
CMD_WREG_temp[0]=CMD_WREG_Mask[0] | RegAdr_CLK1;//set Regiser Address to Write
CMD_WREG_temp[1]=0x02;//set Regiser Data to Write
*/

/*There are no specific command for reading ADC value, 
because the ADC always returns all channels'(4or2) datas at 2nd~5th(or 2nd~3rd) byte on MISO*/

enum ChipSelectPin
{
    CS_0,
    CS_1,
    CS_2
};
//-----Methods-----
void ADS131A0x_setSPI(enum ChipSelectPin CS_Pin, uint64_t Speed);
void ADS131A0x_WREG(uint8_t reg_adr, uint8_t wrt_data);
int ADS131A0x_RREG(uint8_t reg_adr);
void ADS131A0x_SYSCMD(char SysCmdSel);
void ADS131A0x_InitialADC();
void ADS131A0x_Start();
void ADS131A0x_Stop();
void ADS131A0x_GetADCData(uint8_t Mode, float *DataBuffer);
int32_t ConvertInt_24to32(uint8_t *byteArray); //2's complement 24bit to 32bit integer
