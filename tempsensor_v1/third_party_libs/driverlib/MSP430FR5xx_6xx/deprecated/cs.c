/* --COPYRIGHT--,BSD
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//*****************************************************************************
//
// cs.c - Driver for the cs Module.
//
//*****************************************************************************

//*****************************************************************************
//
//! \addtogroup cs_api cs
//! @{
//
//*****************************************************************************

#include "inc/hw_regaccess.h"
#include "inc/hw_memmap.h"

#ifdef DRIVERLIB_LEGACY_MODE

#if defined(__MSP430_HAS_CS__) || defined(__MSP430_HAS_SFR__)
#include "cs.h"

#include <assert.h>

//*****************************************************************************
//
// The following value is used by CS_getACLK, CS_getSMCLK, CS_getMCLK to
// determine the operating frequency based on the available DCO frequencies.
//
//*****************************************************************************
#define CS_DCO_FREQ_1                                                   1000000
#define CS_DCO_FREQ_2                                                   2670000
#define CS_DCO_FREQ_3                                                   3330000
#define CS_DCO_FREQ_4                                                   4000000
#define CS_DCO_FREQ_5                                                   5330000
#define CS_DCO_FREQ_6                                                   6670000
#define CS_DCO_FREQ_7                                                   8000000
#define CS_DCO_FREQ_8                                                  16000000
#define CS_DCO_FREQ_9                                                  20000000
#define CS_DCO_FREQ_10                                                 24000000

//*****************************************************************************
//
// Internal very low power VLOCLK, low frequency oscillator with 10kHz typical
// frequency, internal low-power oscillator MODCLK with 5 MHz typical
// frequency and LFMODCLK is MODCLK divided by 128.
//
//*****************************************************************************
#define CS_VLOCLK_FREQUENCY                                               10000
#define CS_MODCLK_FREQUENCY                                             5000000
#define CS_LFMODCLK_FREQUENCY                                             39062

//*****************************************************************************
//
// The following value is used by CS_XT1Start, CS_bypassXT1,
// CS_XT1StartWithTimeout, CS_bypassXT1WithTimeout to properly set the XTS
// bit. This frequnecy threshold is specified in the User's Guide.
//
//*****************************************************************************
#define LFXT_FREQUENCY_THRESHOLD                                          50000

//*****************************************************************************
//
// LFXT crystal frequency. Should be set with
// CS_externalClockSourceInit if LFXT is used and user intends to invoke
// CS_getSMCLK, CS_getMCLK, CS_getACLK and
// CS_LFXTStart, CS_LFXTByPass, CS_LFXTStartWithTimeout,
// CS_LFXTByPassWithTimeout.
//
//*****************************************************************************
uint32_t CS_LFXTClockFrequency = 0;

//*****************************************************************************
//
// The HFXT crystal frequency. Should be set with
// CS_externalClockSourceInit if HFXT is used and user intends to invoke
// CS_getSMCLK, CS_getMCLK, CS_getACLK,
// CS_LFXTStart, CS_LFXTByPass, CS_LFXTStartWithTimeout,
// CS_LFXTByPassWithTimeout.
//
//*****************************************************************************
uint32_t CS_HFXTClockFrequency = 0;

static uint32_t privateCSASourceClockFromDCO ( uint16_t baseAddress,
        uint8_t clockdivider)
{
    uint32_t CLKFrequency = 0;

    if (HWREG16(baseAddress + OFS_CSCTL1)& DCORSEL) {
            switch(HWREG16(baseAddress + OFS_CSCTL1)& DCOFSEL_7){
                case DCOFSEL_0:
                    CLKFrequency=CS_DCO_FREQ_1/clockdivider;
                    break;
                case DCOFSEL_1:
                    CLKFrequency=CS_DCO_FREQ_5/clockdivider;
                    break;
                case DCOFSEL_2:
                    CLKFrequency=CS_DCO_FREQ_6/clockdivider;
                    break;
                case DCOFSEL_3:
                    CLKFrequency=CS_DCO_FREQ_7/clockdivider;
                    break;
                case DCOFSEL_4:
                    CLKFrequency=CS_DCO_FREQ_8/clockdivider;
                    break;
                case DCOFSEL_5:
                    CLKFrequency=CS_DCO_FREQ_9/clockdivider;
                    break;
                case DCOFSEL_6:
                case DCOFSEL_7:
                    CLKFrequency=CS_DCO_FREQ_10/clockdivider;
                    break;
                default:
                    CLKFrequency=0;
                    break;
            }
        }else{
            switch(HWREG16(baseAddress + OFS_CSCTL1)& DCOFSEL_7){
                case DCOFSEL_0:
                    CLKFrequency=CS_DCO_FREQ_1/clockdivider;
                    break;
                case DCOFSEL_1:
                    CLKFrequency=CS_DCO_FREQ_2/clockdivider;
                    break;
                case DCOFSEL_2:
                    CLKFrequency=CS_DCO_FREQ_3/clockdivider;
                    break;
                case DCOFSEL_3:
                    CLKFrequency=CS_DCO_FREQ_4/clockdivider;
                    break;
                case DCOFSEL_4:
                    CLKFrequency=CS_DCO_FREQ_5/clockdivider;
                    break;
                case DCOFSEL_5:
                    CLKFrequency=CS_DCO_FREQ_6/clockdivider;
                    break;
                case DCOFSEL_6:
                case DCOFSEL_7:
                    CLKFrequency=CS_DCO_FREQ_7/clockdivider;
                    break;
                default:
                    CLKFrequency=0;
                    break;
            }

        }

    return (CLKFrequency);
}

static uint32_t privateCSAComputeCLKFrequency ( uint16_t baseAddress,
    uint16_t CLKSource,
    uint16_t CLKSourceDivider
    )
{
    uint32_t CLKFrequency = 0;
    uint8_t CLKSourceFrequencyDivider = 1;
    uint8_t i = 0;

    // Determine Frequency divider
    for ( i = 0; i < CLKSourceDivider; i++){
        CLKSourceFrequencyDivider *= 2;
    }

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    // Determine clock source based on CLKSource
    switch (CLKSource){

        // If LFXT is selected as clock source
        case SELM__LFXTCLK:
            CLKFrequency = (CS_LFXTClockFrequency /
                            CLKSourceFrequencyDivider);

            //Check if LFXTOFFG is not set. If fault flag is set
            //VLO is used as source clock
            if (HWREG8(baseAddress + OFS_CSCTL5) & LFXTOFFG){
                HWREG8(baseAddress + OFS_CSCTL5) &= ~(LFXTOFFG);
                //Clear OFIFG fault flag
                HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;

                if (HWREG8(baseAddress + OFS_CSCTL5) & LFXTOFFG){
                            CLKFrequency = CS_LFMODCLK_FREQUENCY;
                }
            }
            break;

        case SELM__VLOCLK:
            CLKFrequency =
                (CS_VLOCLK_FREQUENCY / CLKSourceFrequencyDivider);
            break;

        case SELM__LFMODOSC:
            CLKFrequency =
                (CS_LFMODCLK_FREQUENCY / CLKSourceFrequencyDivider);

            break;

        case SELM__DCOCLK:
            CLKFrequency =
            privateCSASourceClockFromDCO(baseAddress, CLKSourceFrequencyDivider);

            break;

        case SELM__MODOSC:
            CLKFrequency =
                (CS_MODCLK_FREQUENCY / CLKSourceFrequencyDivider);

            break;

        case SELM__HFXTCLK:
            CLKFrequency =
                (CS_HFXTClockFrequency / CLKSourceFrequencyDivider);

            if (HWREG8(baseAddress + OFS_CSCTL5) & HFXTOFFG){

              HWREG8(baseAddress + OFS_CSCTL5) &=  ~HFXTOFFG;
              //Clear OFIFG fault flag
              HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;
            }

            if (HWREG8(baseAddress + OFS_CSCTL5) & HFXTOFFG){
                CLKFrequency = CS_MODCLK_FREQUENCY;
            }
            break;
    }

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;

    return (CLKFrequency) ;
}

void CS_setExternalClockSource (uint16_t baseAddress,
    uint32_t LFXTCLK_frequency,
    uint32_t HFXTCLK_frequency
    )
{
    CS_LFXTClockFrequency = LFXTCLK_frequency;
    CS_HFXTClockFrequency = HFXTCLK_frequency;
}

void CS_clockSignalInit ( uint16_t baseAddress,
    uint8_t selectedClockSignal,
    uint16_t clockSource,
    uint16_t clockSourceDivider
    )
{

    //Verify User has selected a valid Frequency divider
    assert(
            (CS_CLOCK_DIVIDER_1 == clockSourceDivider) ||
            (CS_CLOCK_DIVIDER_2 == clockSourceDivider) ||
            (CS_CLOCK_DIVIDER_4 == clockSourceDivider) ||
            (CS_CLOCK_DIVIDER_8 == clockSourceDivider) ||
            (CS_CLOCK_DIVIDER_16 == clockSourceDivider) ||
            (CS_CLOCK_DIVIDER_32 == clockSourceDivider)
            );

        // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    switch (selectedClockSignal){
        case CS_ACLK:
                assert(
                    (CS_LFXTCLK_SELECT == clockSource)  ||
                    (CS_VLOCLK_SELECT == clockSource)   ||
                    (CS_LFMODOSC_SELECT == clockSource)
                    );

            clockSourceDivider = clockSourceDivider << 8;
            clockSource = clockSource << 8;

            HWREG16(baseAddress + OFS_CSCTL2) &= ~(SELA_7);
            HWREG16(baseAddress + OFS_CSCTL2) |= (clockSource);
            HWREG16(baseAddress + OFS_CSCTL3) &= ~(DIVA0 + DIVA1 + DIVA2);
            HWREG16(baseAddress + OFS_CSCTL3) |= clockSourceDivider;
            break;
        case CS_SMCLK:
            assert(
                (CS_LFXTCLK_SELECT == clockSource) ||
                (CS_VLOCLK_SELECT == clockSource) ||
                (CS_DCOCLK_SELECT == clockSource) ||
                (CS_HFXTCLK_SELECT == clockSource) ||
                (CS_LFMODOSC_SELECT== clockSource)||
                (CS_MODOSC_SELECT == clockSource)
                );

            clockSource = clockSource << 4;
            clockSourceDivider = clockSourceDivider << 4;

            HWREG16(baseAddress + OFS_CSCTL2) &= ~(SELS_7);
            HWREG16(baseAddress + OFS_CSCTL2) |= clockSource;
            HWREG16(baseAddress + OFS_CSCTL3) &= ~(DIVS0 + DIVS1 + DIVS2);
            HWREG16(baseAddress + OFS_CSCTL3) |= clockSourceDivider;
            break;
        case CS_MCLK:
            assert(
            (CS_LFXTCLK_SELECT == clockSource) ||
            (CS_VLOCLK_SELECT == clockSource) ||
            (CS_DCOCLK_SELECT == clockSource) ||
            (CS_HFXTCLK_SELECT == clockSource) ||
            (CS_LFMODOSC_SELECT== clockSource)||
            (CS_MODOSC_SELECT == clockSource)
            );

            HWREG16(baseAddress + OFS_CSCTL2) &= ~(SELM_7);
            HWREG16(baseAddress + OFS_CSCTL2) |= clockSource;
            HWREG16(baseAddress + OFS_CSCTL3) &= ~(DIVM0 + DIVM1 + DIVM2);
            HWREG16(baseAddress + OFS_CSCTL3) |= clockSourceDivider;
            break;
    }

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

void CS_LFXTStart ( uint16_t baseAddress,
    uint16_t lfxtdrive
    )
{
    assert(CS_LFXTClockFrequency !=0);

        assert((lfxtdrive == CS_LFXT_DRIVE0 ) ||
            (lfxtdrive == CS_LFXT_DRIVE1 ) ||
            (lfxtdrive == CS_LFXT_DRIVE2 ) ||
            (lfxtdrive == CS_LFXT_DRIVE3 ));

        // Unlock CS control register
        HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

        //If the drive setting is not already set to maximum
        //Set it to max for LFXT startup
        if ((HWREG16(baseAddress + OFS_CSCTL4) & LFXTDRIVE_3) != LFXTDRIVE_3){
            //Highest drive setting for LFXTstartup
            HWREG16(baseAddress + OFS_CSCTL4_L) |= LFXTDRIVE1_L + LFXTDRIVE0_L;
        }

        HWREG16(baseAddress + OFS_CSCTL4) &= ~LFXTBYPASS;

        //Wait for Crystal to stabilize
        while (HWREG8(baseAddress + OFS_CSCTL5) & LFXTOFFG)
        {
            //Clear OSC flaut Flags fault flags
            HWREG8(baseAddress + OFS_CSCTL5) &= ~(LFXTOFFG);

            //Clear OFIFG fault flag
            HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;
        }

        //set requested Drive mode
        HWREG16(baseAddress + OFS_CSCTL4) = ( HWREG16(baseAddress + OFS_CSCTL4) &
                                             ~(LFXTDRIVE_3)
                                             ) |
                                           (lfxtdrive);

        //Switch ON LFXT oscillator
        HWREG16(baseAddress + OFS_CSCTL4) &= ~LFXTOFF;

        // Lock CS control register
        HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;

}

void CS_bypassLFXT ( uint16_t baseAddress)
{
    //Verify user has set frequency of LFXT with SetExternalClockSource
    assert(CS_LFXTClockFrequency!=0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    assert(CS_LFXTClockFrequency<LFXT_FREQUENCY_THRESHOLD);

    // Set LFXT in LF mode Switch off LFXT oscillator and enable BYPASS mode
    HWREG16(baseAddress + OFS_CSCTL4) |= (LFXTBYPASS + LFXTOFF);

    //Wait until LFXT stabilizes
    while (HWREG8(baseAddress + OFS_CSCTL5) & LFXTOFFG)
    {
        //Clear OSC flaut Flags fault flags
        HWREG8(baseAddress + OFS_CSCTL5) &= ~(LFXTOFFG);

        // Clear the global fault flag. In case the LFXT caused the global fault
        // flag to get set this will clear the global error condition. If any
        // error condition persists, global flag will get again.
        HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;
    }

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

bool CS_LFXTStartWithTimeout (uint16_t baseAddress,
        uint16_t lfxtdrive,
        uint32_t timeout
    )
{
    assert(CS_LFXTClockFrequency !=0);

    assert((lfxtdrive == CS_LFXT_DRIVE0 ) ||
        (lfxtdrive == CS_LFXT_DRIVE1 ) ||
        (lfxtdrive == CS_LFXT_DRIVE2 ) ||
        (lfxtdrive == CS_LFXT_DRIVE3 ));

    assert(timeout > 0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    //If the drive setting is not already set to maximum
    //Set it to max for LFXT startup
    if ((HWREG16(baseAddress + OFS_CSCTL4) & LFXTDRIVE_3) != LFXTDRIVE_3){
        //Highest drive setting for LFXTstartup
        HWREG16(baseAddress + OFS_CSCTL4_L) |= LFXTDRIVE1_L + LFXTDRIVE0_L;
    }

    HWREG16(baseAddress + OFS_CSCTL4) &= ~LFXTBYPASS;

    while ((HWREG8(baseAddress + OFS_CSCTL5) & LFXTOFFG) && --timeout)
    {
        //Clear OSC fault Flags fault flags
        HWREG8(baseAddress + OFS_CSCTL5) &= ~(LFXTOFFG);

        // Clear the global fault flag. In case the LFXT caused the global fault
        // flag to get set this will clear the global error condition. If any
        // error condition persists, global flag will get again.
        HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;

    }

    if(timeout){

        //set requested Drive mode
        HWREG16(baseAddress + OFS_CSCTL4) = ( HWREG16(baseAddress + OFS_CSCTL4) &
                                                 ~(LFXTDRIVE_3)
                                                 ) |
                                               (lfxtdrive);
        //Switch ON LFXT oscillator
        HWREG16(baseAddress + OFS_CSCTL4) &= ~LFXTOFF;
        // Lock CS control register
        HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
        return (STATUS_SUCCESS);
    } else   {
        // Lock CS control register
        HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
        return (STATUS_FAIL);
    }
}

bool CS_bypassLFXTWithTimeout (uint16_t baseAddress,
    uint32_t timeout
    )
{
    assert(CS_LFXTClockFrequency !=0);

    assert(CS_LFXTClockFrequency<LFXT_FREQUENCY_THRESHOLD);

    assert(timeout > 0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    // Set LFXT in LF mode Switch off LFXT oscillator and enable BYPASS mode
    HWREG16(baseAddress + OFS_CSCTL4) |= (LFXTBYPASS + LFXTOFF);

    while ((HWREG8(baseAddress + OFS_CSCTL5) & LFXTOFFG) && --timeout)
    {
        //Clear OSC fault Flags fault flags
        HWREG8(baseAddress + OFS_CSCTL5) &= ~(LFXTOFFG);

        // Clear the global fault flag. In case the LFXT caused the global fault
        // flag to get set this will clear the global error condition. If any
        // error condition persists, global flag will get again.
        HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;

    }

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;

    if (timeout){
        return (STATUS_SUCCESS);
    } else {
        return (STATUS_FAIL);
    }
}

void CS_LFXTOff (uint16_t baseAddress)
{
    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    //Switch off LFXT oscillator
    HWREG16(baseAddress + OFS_CSCTL4) |= LFXTOFF;

        // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

void CS_HFXTStart (  uint16_t baseAddress,
    uint16_t hfxtdrive
    )
{
    assert(CS_HFXTClockFrequency !=0);

    assert((hfxtdrive == CS_HFXTDRIVE_4MHZ_8MHZ  ) ||
           (hfxtdrive == CS_HFXTDRIVE_8MHZ_16MHZ ) ||
           (hfxtdrive == CS_HFXTDRIVE_16MHZ_24MHZ )||
           (hfxtdrive == CS_HFXTDRIVE_24MHZ_32MHZ ));

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    //Disable HFXTBYPASS mode and Switch on HFXT oscillator
    HWREG16(baseAddress + OFS_CSCTL4) &= ~HFXTBYPASS;

    //If HFFrequency is 16MHz or above
    if (CS_HFXTClockFrequency>16000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_3;
    }
    //If HFFrequency is between 8MHz and 16MHz
    else if (CS_HFXTClockFrequency>8000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_2;
    }
    //If HFFrequency is between 0MHz and 4MHz
    else if (CS_HFXTClockFrequency<4000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_0;
    }
    //If HFFrequency is between 4MHz and 8MHz
    else{
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_1;
    }

    while (HWREG8(baseAddress + OFS_CSCTL5) & HFXTOFFG){
     //Clear OSC flaut Flags
     HWREG8(baseAddress + OFS_CSCTL5) &= ~(HFXTOFFG);

     //Clear OFIFG fault flag
     HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;
    }

    HWREG16(baseAddress + OFS_CSCTL4) = ( HWREG16(baseAddress + OFS_CSCTL4) &
                                         ~(CS_HFXTDRIVE_24MHZ_32MHZ)
                                         ) |
                                       (hfxtdrive);

    HWREG16(baseAddress + OFS_CSCTL4) &= ~HFXTOFF;

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;

}

void CS_bypassHFXT (  uint16_t baseAddress )
{
    //Verify user has initialized value of HFXTClock
    assert(CS_HFXTClockFrequency !=0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    //Switch off HFXT oscillator and set it to BYPASS mode
    HWREG16(baseAddress + OFS_CSCTL4) |= ( HFXTBYPASS + HFXTOFF );

    //Set correct HFFREQ bit for FR58xx/FR59xx devices

    //If HFFrequency is 16MHz or above
    if (CS_HFXTClockFrequency>16000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_3;
    }
    //If HFFrequency is between 8MHz and 16MHz
    else if (CS_HFXTClockFrequency>8000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_2;
    }
    //If HFFrequency is between 0MHz and 4MHz
    else if (CS_HFXTClockFrequency<4000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_0;
    }
    //If HFFrequency is between 4MHz and 8MHz
    else{
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_1;
    }

    while (HWREG8(baseAddress + OFS_CSCTL5) & HFXTOFFG){
     //Clear OSC fault Flags
     HWREG8(baseAddress + OFS_CSCTL5) &= ~(HFXTOFFG);

     //Clear OFIFG fault flag
     HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;
    }

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

bool CS_HFXTStartWithTimeout ( uint16_t baseAddress,
    uint16_t hfxtdrive,
    uint32_t timeout
    )
{
    //Verify user has initialized value of HFXTClock
    assert(CS_HFXTClockFrequency !=0);

    assert(timeout > 0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    // Disable HFXTBYPASS mode
    HWREG16(baseAddress + OFS_CSCTL4) &= ~HFXTBYPASS;

    //Set correct HFFREQ bit for FR58xx/FR59xx devices based
    //on HFXTClockFrequency

    //If HFFrequency is 16MHz or above
    if (CS_HFXTClockFrequency>16000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_3;
    }
    //If HFFrequency is between 8MHz and 16MHz
    else if (CS_HFXTClockFrequency>8000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_2;
    }
    //If HFFrequency is between 0MHz and 4MHz
    else if (CS_HFXTClockFrequency<4000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_0;
    }
    //If HFFrequency is between 4MHz and 8MHz
    else{
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_1;
    }

    while ((HWREG8(baseAddress + OFS_CSCTL5) & HFXTOFFG) && --timeout)
    {
        //Clear OSC fault Flags fault flags
        HWREG8(baseAddress + OFS_CSCTL5) &= ~(HFXTOFFG);

        // Clear the global fault flag. In case the LFXT caused the global fault
        // flag to get set this will clear the global error condition. If any
        // error condition persists, global flag will get again.
        HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;

    }

    if (timeout){
        //Set drive strength for HFXT
        HWREG16(baseAddress + OFS_CSCTL4) = ( HWREG16(baseAddress + OFS_CSCTL4) &
                                             ~(CS_HFXTDRIVE_24MHZ_32MHZ)
                                             ) |
                                           (hfxtdrive);

        //Switch on HFXT oscillator
        HWREG16(baseAddress + OFS_CSCTL4) &= ~HFXTOFF;
        // Lock CS control register
        HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
        return (STATUS_SUCCESS);
    } else   {
        // Lock CS control register
        HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
        return (STATUS_FAIL);
    }
}

bool CS_bypassHFXTWithTimeout ( uint16_t baseAddress,
    uint32_t timeout
    )
{
    //Verify user has initialized value of HFXTClock
    assert(CS_HFXTClockFrequency !=0);

    assert(timeout > 0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    //If HFFrequency is 16MHz or above
    if (CS_HFXTClockFrequency>16000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_3;
    }
    //If HFFrequency is between 8MHz and 16MHz
    else if (CS_HFXTClockFrequency>8000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_2;
    }
    //If HFFrequency is between 0MHz and 4MHz
    else if (CS_HFXTClockFrequency<4000000) {
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_0;
    }
    //If HFFrequency is between 4MHz and 8MHz
    else{
        HWREG16(baseAddress + OFS_CSCTL4)=HFFREQ_1;
    }

    //Switch off HFXT oscillator and enable BYPASS mode
    HWREG16(baseAddress + OFS_CSCTL4) |= (HFXTBYPASS + HFXTOFF );

    while ((HWREG8(baseAddress + OFS_CSCTL5) & HFXTOFFG) && --timeout)
    {
        //Clear OSC fault Flags fault flags
        HWREG8(baseAddress + OFS_CSCTL5) &= ~(HFXTOFFG);

        // Clear the global fault flag. In case the LFXT caused the global fault
        // flag to get set this will clear the global error condition. If any
        // error condition persists, global flag will get again.
        HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;

    }

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;

    if (timeout){
        return (STATUS_SUCCESS);
    } else   {
        return (STATUS_FAIL);
    }
}

void CS_HFXTOff (uint16_t baseAddress)
{
    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    //Switch off HFXT oscillator
    HWREG16(baseAddress + OFS_CSCTL4) |= HFXTOFF;

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

void CS_enableClockRequest (uint16_t baseAddress,
    uint8_t selectClock
    )
{
    assert(
            (CS_ACLK  == selectClock )||
            (CS_SMCLK == selectClock )||
            (CS_MCLK  == selectClock )||
            (CS_MODOSC== selectClock ));

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    HWREG8(baseAddress + OFS_CSCTL6) |= selectClock;

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

void CS_disableClockRequest (uint16_t baseAddress,
    uint8_t selectClock
    )
{
    assert(
            (CS_ACLK  == selectClock )||
            (CS_SMCLK == selectClock )||
            (CS_MCLK  == selectClock )||
            (CS_MODOSC== selectClock ));

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    HWREG8(baseAddress + OFS_CSCTL6) &= ~selectClock;

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

uint8_t CS_faultFlagStatus (uint16_t baseAddress,
    uint8_t mask
    )
{
    assert(
                (CS_HFXTOFFG  == mask )||
                (CS_LFXTOFFG == mask )
                );
    return (HWREG8(baseAddress + OFS_CSCTL5) & mask);
}

void CS_clearFaultFlag (uint16_t baseAddress,
    uint8_t mask
    )
{
    assert(
            (CS_HFXTOFFG  == mask )||
            (CS_LFXTOFFG == mask )
            );

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    HWREG8(baseAddress + OFS_CSCTL5) &= ~mask;

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

uint32_t CS_getACLK (uint16_t baseAddress)
{

    //Find ACLK source
    uint16_t ACLKSource = (HWREG16(baseAddress + OFS_CSCTL2) & SELA_7);
    ACLKSource = ACLKSource >> 8;

    //Find ACLK frequency divider
    uint16_t ACLKSourceDivider =  HWREG16(baseAddress + OFS_CSCTL3) & SELA_7;
    ACLKSourceDivider = ACLKSourceDivider >>8;

    return (privateCSAComputeCLKFrequency(baseAddress,
                ACLKSource,
                ACLKSourceDivider));

}

uint32_t CS_getSMCLK (uint16_t baseAddress)
{
        //Find SMCLK source
        uint16_t SMCLKSource = HWREG8(baseAddress + OFS_CSCTL2) & SELS_7 ;

        SMCLKSource = SMCLKSource >> 4;

        //Find SMCLK frequency divider
        uint16_t SMCLKSourceDivider = HWREG16(baseAddress + OFS_CSCTL3) & SELS_7;
        SMCLKSourceDivider = SMCLKSourceDivider >> 4;

        return (privateCSAComputeCLKFrequency(baseAddress,
                    SMCLKSource,
                    SMCLKSourceDivider )
                );
}

uint32_t CS_getMCLK (uint16_t baseAddress)
{
        //Find MCLK source
        uint16_t MCLKSource = (HWREG16(baseAddress + OFS_CSCTL2) & SELM_7);
        //Find MCLK frequency divider
        uint16_t MCLKSourceDivider =  HWREG16(baseAddress + OFS_CSCTL3) & SELM_7;

        return (privateCSAComputeCLKFrequency(baseAddress,
                    MCLKSource,
                    MCLKSourceDivider )
                );
}

void CS_VLOoff(uint16_t baseAddress)
{
    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    HWREG16(baseAddress + OFS_CSCTL4) |= VLOOFF;

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

uint16_t CS_clearAllOscFlagsWithTimeout(uint16_t baseAddress,
                                               uint32_t timeout)
{
    assert(timeout > 0);

    // Unlock CS control register
    HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

    do {
      // Clear all osc fault flags
      HWREG8(baseAddress + OFS_CSCTL5) &= ~(LFXTOFFG + HFXTOFFG );

      // Clear the global osc fault flag.
      HWREG8(SFR_BASE + OFS_SFRIFG1) &= ~OFIFG;

      // Check LFXT fault flags
    } while ((HWREG8(SFR_BASE + OFS_SFRIFG1) & OFIFG) && --timeout);

    // Lock CS control register
    HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;

    return (HWREG8(baseAddress + OFS_CSCTL5) & (LFXTOFFG + HFXTOFFG));
}

void CS_setDCOFreq(uint16_t baseAddress,
        uint16_t dcorsel,
        uint16_t dcofsel)
{
        assert(
                    (dcofsel==CS_DCOFSEL_0)||
                    (dcofsel==CS_DCOFSEL_1)||
                    (dcofsel==CS_DCOFSEL_2)||
                    (dcofsel==CS_DCOFSEL_3)||
                    (dcofsel==CS_DCOFSEL_4)||
                    (dcofsel==CS_DCOFSEL_5)||
                    (dcofsel==CS_DCOFSEL_6)
                    );

        //Verify user has selected a valid DCO Frequency Range option
        assert(
                (dcorsel==CS_DCORSEL_0)||
                (dcorsel==CS_DCORSEL_1));

        //Unlock CS control register
        HWREG16(baseAddress + OFS_CSCTL0) = CSKEY;

        // Set user's frequency selection for DCO
        HWREG16(baseAddress + OFS_CSCTL1) = (dcorsel + dcofsel);

        // Lock CS control register
        HWREG8(baseAddress + OFS_CSCTL0_H) = 0x00;
}

#endif
#endif
//*****************************************************************************
//
//! Close the doxygen group for cs_api
//! @}
//
//*****************************************************************************
