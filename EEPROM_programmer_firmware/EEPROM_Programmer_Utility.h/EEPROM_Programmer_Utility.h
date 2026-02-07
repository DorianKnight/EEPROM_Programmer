/*
  EEPROM_Programmer_Utility.h is a library for managing communication between
  the programmer software application and the EEPROM mounted to my custom hardware

  // Author: Dorian Knight
  // Created: August 10th 2025
  // Updated: August 17th 2025

  Contains utility function definitions to translate data and manage PCB hardware signals
*/

#ifndef EEPROM_Programmer_Utility_h
#define EEPROM_Programmer_Utility_h

#include "Arduino.h"

// Pin descriptions
const int CE = 2;
const int WE = 3;
const int OE = 4;
const int AddRx = 5;
const int DataTx = 6;
const int DataRx = 7;
const int Clk = 8;
const int Load = 9;

// Signal hold delay timings
const int clk_period          = 50; // milliseconds
const int EEPROM_write_delay  = 50; // milliseconds
const int EEPROM_load_delay   = 50; // milliseconds

class EEPROM_Utility
{
  public:
    EEPROM_Utility(int baud_rate);
    void begin();
    void data_packager(byte *bytewise_data, bool *data_container, unsigned short stream_length);
    void construct_address_bitstream(byte *raw_bytes, bool *bit_stream);
    void increment_address(bool *address_bit_stream);
    void decrement_address(bool *address_bit_stream);
    bool write(bool *data, bool *address);
    void read(bool *data, bool* address);
    int get_baud_rate();
    byte compress_bit_stream(bool *data_bit_stream);
  private:
    void pulse();
    void EEPROMWrite();
    void EEPROMLoad();
    bool verify(bool *data_written, bool *address_to_check);
    int  _baud_rate;

};

#endif