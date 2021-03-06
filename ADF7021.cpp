/*
 *   Copyright (C) 2016 by Jim McLaughlin KI6ZUM
 *   Copyright (C) 2016, 2017 by Andy Uribe CA6JAU
 * 
 *   Some of the code is based on work of Guus Van Dooren PE1PLM:
 *   https://github.com/ki6zum/gmsk-dstar/blob/master/firmware/dvmega/dvmega.ino
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include "Config.h"

#if defined(ENABLE_ADF7021)

#include "Globals.h"
#include "IO.h"
#include "ADF7021.h"
#include <math.h>

volatile uint32_t  AD7021_control_word;

uint32_t           ADF7021_RX_REG0;
uint32_t           ADF7021_TX_REG0;

volatile bool sle_request = false;

static  void Send_AD7021_control_shift()
{
  int AD7021_counter;

  for(AD7021_counter = 31; AD7021_counter >= 0; AD7021_counter--) {
    if(bitRead(AD7021_control_word, AD7021_counter) == HIGH)
      io.SDATA_pin(HIGH);
    else
      io.SDATA_pin(LOW);

    io.dlybit();
    io.SCLK_pin(HIGH);
    io.dlybit();
    io.SCLK_pin(LOW);
  }
}

static  void Send_AD7021_control_nosle()
{
  Send_AD7021_control_shift();
  sle_request = true;
}

void Send_AD7021_control()
{
  int AD7021_counter;

  for(AD7021_counter = 31; AD7021_counter >= 0; AD7021_counter--) {
    if(bitRead(AD7021_control_word, AD7021_counter) == HIGH)
      io.SDATA_pin(HIGH);
    else
      io.SDATA_pin(LOW);

    io.dlybit();
    io.SCLK_pin(HIGH);
    io.dlybit();
    io.SCLK_pin(LOW);
  }

  io.SLE_pin(HIGH);
  io.dlybit();
  io.SLE_pin(LOW);
  io.SDATA_pin(LOW);
}

#if defined(SEND_RSSI_DATA)
uint16_t CIO::readRSSI()
{
  uint32_t AD7021_RB;
  uint16_t RB_word = 0;
  int AD7021_counter;
  uint8_t RB_code, gain_code, gain_corr;

  // Register 7, readback enable, ADC RSSI mode
  AD7021_RB = 0x0147;
  
  // Send control register
  for(AD7021_counter = 8; AD7021_counter >= 0; AD7021_counter--) {
    if(bitRead(AD7021_RB, AD7021_counter) == HIGH)
      SDATA_pin(HIGH);
    else
      SDATA_pin(LOW);

    dlybit();
    SCLK_pin(HIGH);
    dlybit();
    SCLK_pin(LOW);
  }

  SDATA_pin(LOW);
  SLE_pin(HIGH);
  dlybit();
  
  // Read SREAD pin
  for(AD7021_counter = 17; AD7021_counter >= 0; AD7021_counter--) {
    SCLK_pin(HIGH);
    dlybit();

    if( (AD7021_counter != 17) && (AD7021_counter != 0) )
      RB_word |= ( (SREAD_pin() & 0x01) << (AD7021_counter-1) );

    SCLK_pin(LOW);
    dlybit();

  }

  SLE_pin(LOW);
  
  // Process RSSI code
  RB_code = RB_word & 0x7f;
  gain_code = (RB_word >> 7) & 0x0f;
  
  switch(gain_code) {
    case 0b1010:
      gain_corr = 0;
      break;
    case 0b0110:
      gain_corr = 24;
      break;
    case 0b0101:
      gain_corr = 38;
      break;
    case 0b0100:
      gain_corr = 58;
      break;
    case 0b0000:
      gain_corr = 86;
      break;
    default:
      gain_corr = 0;
      break;
  }

  return ( 130 - (RB_code + gain_corr)/2 );

}
#endif

void CIO::ifConf(MMDVM_STATE modemState, bool reset)
{
  float    divider;
  uint8_t  N_divider;
  uint16_t F_divider;
  uint32_t div2;

  uint32_t ADF7021_REG1  = 0;
  uint32_t ADF7021_REG2  = 0;
  uint32_t ADF7021_REG3  = 0;
  uint32_t ADF7021_REG4  = 0;
  uint32_t ADF7021_REG10 = 0;
  uint32_t ADF7021_REG13 = 0;
  uint32_t AFC_OFFSET = 0;

  m_modemState_prev = modemState;

  // Toggle CE pin for ADF7021 reset
  if(reset) {
    CE_pin(LOW);
    delay_rx();
    CE_pin(HIGH);
    delay_rx();
  }

  // Check frequency band
  if( (m_frequency_tx >= VHF1_MIN) && (m_frequency_tx < VHF1_MAX) ) {
    ADF7021_REG1 = ADF7021_REG1_VHF1;         // VHF1, external VCO
    div2 = 1U;
  }
  else if( (m_frequency_tx >= VHF2_MIN) && (m_frequency_tx < VHF2_MAX) ) {
    ADF7021_REG1 = ADF7021_REG1_VHF2;         // VHF1, external VCO
    div2 = 1U;
  }
  else if( (m_frequency_tx >= UHF1_MIN)&&(m_frequency_tx < UHF1_MAX) ) {
    ADF7021_REG1 = ADF7021_REG1_UHF1;         // UHF1, internal VCO
    div2 = 1U;
  }
  else if( (m_frequency_tx >= UHF2_MIN)&&(m_frequency_tx < UHF2_MAX) ) {
    ADF7021_REG1 = ADF7021_REG1_UHF2;         // UHF2, internal VCO
    div2 = 2U;
  }
  else {
    ADF7021_REG1 = ADF7021_REG1_UHF1;         // UHF1, internal VCO
    div2 = 1U;
  }

  switch (modemState) {
    case STATE_DSTAR:
      AFC_OFFSET = 0;
      break;
    case STATE_DMR:
      AFC_OFFSET = AFC_OFFSET_DMR;
      break;
    case STATE_YSF:
      AFC_OFFSET = AFC_OFFSET_YSF;
      break;
    case STATE_P25:
      AFC_OFFSET = AFC_OFFSET_P25;
      break;
    default:
      break;
  }

  if( div2 == 1U )
    divider = (m_frequency_rx - 100000 + AFC_OFFSET) / (ADF7021_PFD / 2U);
  else
    divider = (m_frequency_rx - 100000 + (2*AFC_OFFSET)) / ADF7021_PFD;

  N_divider = floor(divider);
  divider = (divider - N_divider) * 32768;
  F_divider = floor(divider + 0.5);

  ADF7021_RX_REG0  = (uint32_t) 0b0000;
  
#if defined(BIDIR_DATA_PIN)
  ADF7021_RX_REG0 |= (uint32_t) 0b01001   << 27;   // mux regulator/receive
#else
  ADF7021_RX_REG0 |= (uint32_t) 0b01011   << 27;   // mux regulator/uart-spi enabled/receive
#endif

  ADF7021_RX_REG0 |= (uint32_t) N_divider << 19;   // frequency;
  ADF7021_RX_REG0 |= (uint32_t) F_divider << 4;    // frequency;
  
  if( div2 == 1U )
    divider = m_frequency_tx / (ADF7021_PFD / 2U);
  else
    divider = m_frequency_tx / ADF7021_PFD;

  N_divider = floor(divider);
  divider = (divider - N_divider) * 32768;
  F_divider = floor(divider + 0.5);

  ADF7021_TX_REG0  = (uint32_t) 0b0000;            // register 0

#if defined(BIDIR_DATA_PIN)
  ADF7021_TX_REG0 |= (uint32_t) 0b01000   << 27;   // mux regulator/transmit
#else
  ADF7021_TX_REG0 |= (uint32_t) 0b01010   << 27;   // mux regulator/uart-spi enabled/transmit
#endif

  ADF7021_TX_REG0 |= (uint32_t) N_divider << 19;   // frequency;
  ADF7021_TX_REG0 |= (uint32_t) F_divider << 4;    // frequency;

  switch (modemState) {
    case STATE_DSTAR:
      // Dev: 1200 Hz, symb rate = 4800

      ADF7021_REG3 = ADF7021_REG3_DSTAR;
      ADF7021_REG10 = ADF7021_REG10_DSTAR;
    
      // K=32
      ADF7021_REG4  = (uint32_t) 0b0100                    << 0;   // register 4
      ADF7021_REG4 |= (uint32_t) 0b001                     << 4;   // mode, GMSK
      ADF7021_REG4 |= (uint32_t) 0b1                       << 7;
      ADF7021_REG4 |= (uint32_t) 0b10                      << 8;
      ADF7021_REG4 |= (uint32_t) ADF7021_DISC_BW_DSTAR     << 10;  // Disc BW
      ADF7021_REG4 |= (uint32_t) ADF7021_POST_BW_DSTAR     << 20;  // Post dem BW
      ADF7021_REG4 |= (uint32_t) 0b10                      << 30;  // IF filter

      ADF7021_REG13 = (uint32_t) 0b1101      << 0;   // register 13
      ADF7021_REG13 |= (uint32_t) ADF7021_SLICER_TH_DSTAR  << 4;   // slicer threshold

      ADF7021_REG2 = (uint32_t) 0b00                       << 28;  // clock normal
      ADF7021_REG2 |= (uint32_t) (ADF7021_DEV_DSTAR / div2)<< 19;  // deviation
      ADF7021_REG2 |= (uint32_t) 0b001                     << 4;   // modulation (GMSK)
      break;
      
    case STATE_DMR:
      // Dev: +1 symb 648 Hz, symb rate = 4800
    
      ADF7021_REG3 = ADF7021_REG3_DMR;
      ADF7021_REG10 = ADF7021_REG10_DMR;

      // K=32
      ADF7021_REG4  = (uint32_t) 0b0100                    << 0;   // register 4
      ADF7021_REG4 |= (uint32_t) 0b011                     << 4;   // mode, 4FSK
      ADF7021_REG4 |= (uint32_t) 0b0                       << 7;
      ADF7021_REG4 |= (uint32_t) 0b11                      << 8;
      ADF7021_REG4 |= (uint32_t) ADF7021_DISC_BW_DMR       << 10;  // Disc BW
      ADF7021_REG4 |= (uint32_t) ADF7021_POST_BW_DMR       << 20;  // Post dem BW
      ADF7021_REG4 |= (uint32_t) 0b10                      << 30;  // IF filter

      ADF7021_REG13 = (uint32_t) 0b1101                    << 0;   // register 13
      ADF7021_REG13 |= (uint32_t) ADF7021_SLICER_TH_DMR    << 4;   // slicer threshold

      ADF7021_REG2 = (uint32_t) 0b10                       << 28;  // invert data (and RC alpha = 0.5)
      ADF7021_REG2 |= (uint32_t) (ADF7021_DEV_DMR / div2)  << 19;  // deviation
      ADF7021_REG2 |= (uint32_t) 0b111                     << 4;   // modulation (RC 4FSK)
      break;
      
    case STATE_YSF:
      // Dev: +1 symb 900 Hz, symb rate = 4800

      ADF7021_REG3 = ADF7021_REG3_YSF;
      ADF7021_REG10 = ADF7021_REG10_YSF;

      // K=28
      ADF7021_REG4  = (uint32_t) 0b0100                    << 0;   // register 4
      ADF7021_REG4 |= (uint32_t) 0b011                     << 4;   // mode, 4FSK
      ADF7021_REG4 |= (uint32_t) 0b0                       << 7;
      ADF7021_REG4 |= (uint32_t) 0b11                      << 8;
      ADF7021_REG4 |= (uint32_t) (m_LoDevYSF ? ADF7021_DISC_BW_YSF_L : ADF7021_DISC_BW_YSF_H) << 10;  // Disc BW
      ADF7021_REG4 |= (uint32_t) ADF7021_POST_BW_YSF       << 20;  // Post dem BW
      ADF7021_REG4 |= (uint32_t) 0b10                      << 30;  // IF filter

      ADF7021_REG13 = (uint32_t) 0b1101                    << 0;   // register 13
      ADF7021_REG13 |= (uint32_t) (m_LoDevYSF ? ADF7021_SLICER_TH_YSF_L : ADF7021_SLICER_TH_YSF_H) << 4;   // slicer threshold

      ADF7021_REG2 = (uint32_t) 0b10                       << 28;  // invert data (and RC alpha = 0.5)
      ADF7021_REG2 |= (uint32_t) ((m_LoDevYSF ? ADF7021_DEV_YSF_L : ADF7021_DEV_YSF_H) / div2)  << 19;  // deviation
      ADF7021_REG2 |= (uint32_t) 0b111                     << 4;   // modulation (RC 4FSK)
      break;
      
    case STATE_P25:
      // Dev: +1 symb 600 Hz, symb rate = 4800

      ADF7021_REG3 = ADF7021_REG3_P25;
      ADF7021_REG10 = ADF7021_REG10_P25;

      // K=32
      ADF7021_REG4  = (uint32_t) 0b0100                    << 0;   // register 4
      ADF7021_REG4 |= (uint32_t) 0b011                     << 4;   // mode, 4FSK
      ADF7021_REG4 |= (uint32_t) 0b0                       << 7;
      ADF7021_REG4 |= (uint32_t) 0b11                      << 8;
      ADF7021_REG4 |= (uint32_t) ADF7021_DISC_BW_P25       << 10;  // Disc BW
      ADF7021_REG4 |= (uint32_t) ADF7021_POST_BW_P25       << 20;  // Post dem BW
      ADF7021_REG4 |= (uint32_t) 0b10                      << 30;  // IF filter

      ADF7021_REG13 = (uint32_t) 0b1101                    << 0;   // register 13
      ADF7021_REG13 |= (uint32_t) ADF7021_SLICER_TH_P25    << 4;   // slicer threshold

      ADF7021_REG2 = (uint32_t) 0b10                       << 28;  // invert data (and RC alpha = 0.5)
      ADF7021_REG2 |= (uint32_t) (ADF7021_DEV_P25 / div2)  << 19;  // deviation
      ADF7021_REG2 |= (uint32_t) 0b111                     << 4;   // modulation (RC 4FSK)
      break;
      
    default:
      break;
  }

  // VCO/OSCILLATOR (REG1)
  AD7021_control_word = ADF7021_REG1;
  Send_AD7021_control();

  // TX/RX CLOCK (3)
  AD7021_control_word = ADF7021_REG3;
  Send_AD7021_control();

  // DEMOD (4)
  AD7021_control_word = ADF7021_REG4;
  Send_AD7021_control();

  // IF FILTER (5)
  AD7021_control_word = ADF7021_REG5;
  Send_AD7021_control();
  
  // Delay for coarse IF filter calibration
  delay_rx();
  delay_rx();
  delay_rx();

  // Frequency RX (0)
  setRX();

  // MODULATION (2)
  ADF7021_REG2 |= (uint32_t) 0b0010;               // register 2
  ADF7021_REG2 |= (uint32_t) m_power       << 13;  // power level
  ADF7021_REG2 |= (uint32_t) 0b110001      << 7;   // PA  
  AD7021_control_word = ADF7021_REG2;
  Send_AD7021_control();
 
  // TEST MODE (disabled) (15)
  AD7021_control_word = 0x000E000F;
  Send_AD7021_control();

  // AGC (auto, defaults) (9)
  AD7021_control_word = 0x000231E9;
  Send_AD7021_control();

  // AFC (10)
  AD7021_control_word = ADF7021_REG10;
  Send_AD7021_control();

  // SYNC WORD DET (11)
  AD7021_control_word = 0x0000003B;
  Send_AD7021_control();

  // SWD/THRESHOLD (12)
  AD7021_control_word = 0x0000010C;
  Send_AD7021_control();

  // 3FSK/4FSK DEMOD (13)
  AD7021_control_word = ADF7021_REG13;
  Send_AD7021_control();

}

//======================================================================================================================
void CIO::setTX()
{ 
  // Send register 0 for TX operation
  AD7021_control_word = ADF7021_TX_REG0;         
  Send_AD7021_control_nosle();
  
#if defined(BIDIR_DATA_PIN)
  Data_dir_out(true);  // Data pin output mode
#endif
  
  // PTT pin on
  PTT_pin(HIGH); 
}

//======================================================================================================================
void CIO::setRX()
{ 
  // Send register 0 for RX operation
  AD7021_control_word = ADF7021_RX_REG0;
  Send_AD7021_control();
  
#if defined(BIDIR_DATA_PIN)
  Data_dir_out(false);  // Data pin input mode
#endif
  
  // PTT pin off
  PTT_pin(LOW);
}

void CIO::setLoDevYSF(bool on)
{
  m_LoDevYSF = on;
}

#endif
