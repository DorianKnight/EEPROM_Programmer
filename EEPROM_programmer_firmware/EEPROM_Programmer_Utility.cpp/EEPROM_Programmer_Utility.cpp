/*
  EEPROM_Programmer_Utility.cpp is a library for managing communication between
  the programmer software application and the EEPROM mounted to my custom hardware

  // Author: Dorian Knight
  // Created: August 11th 2025
  // Updated: August 18th 2025

  Contains utility function implementations to translate data and manage PCB hardware signals
*/

#include "Arduino.h"
#include "EEPROM_Programmer_Utility.h"

EEPROM_Utility::EEPROM_Utility(int baud_rate){
  _baud_rate = baud_rate;
}

void EEPROM_Utility::begin()
{
  // Called during setup

  // Define MCU port pins
  pinMode(OE,     OUTPUT); // Output Enable
  pinMode(WE,     OUTPUT); // Write Enable
  pinMode(CE,     OUTPUT); // Chip Enable
  pinMode(AddRx,  OUTPUT); // Address received from PC
  pinMode(DataRx, OUTPUT); // Data received from PC
  pinMode(DataTx, INPUT);  // Data received from EEPROM
  pinMode(Clk,    OUTPUT); // Clock signal
  pinMode(Load,   OUTPUT); // Load signal

  // Pull the control signal lines high as the control signals are active low
  digitalWrite(CE, HIGH);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
}

void EEPROM_Utility::data_packager(byte *bytewise_data, bool *data_container, unsigned short stream_length) {
  /** Function description:
    * Take data bytes as input and construct an array of 8 elements where each element represents a bit of the input data byte
    * The end result is a two dimensional array where the [i] is the binary array for each input byte and the [j] is a singular bit within the desired byte
    * data_container[8][4] would get me the 5th bit in the 9th byte provided to me as input
    * Note: The data is stored in little endian format
    */
  byte mask;
  byte result;
  for (int i=0; i<stream_length; i++) {
    // Take byte from input data and convert into a binary stream
    mask = 0b00000001;
    for (int j=0; j<8; j++) {
      // Use mask to construct bit stream
      result = bytewise_data[i] & mask;
      if (result > 0) {
        *(data_container + i*8 + j) = 1;
      }

      else {
        *(data_container + i*8 + j) = 0;
      }

      mask <<= 1;
    }
  }
}

void EEPROM_Utility::construct_address_bitstream(byte *raw_bytes, bool *bit_stream) {
  /** Function description
    * Use bitmasking to construct binary array
    * Binary array is little endian
    */

  unsigned short raw_data;
  unsigned short result;
  unsigned short mask = 0b00000001;

  memcpy(&raw_data, raw_bytes, 2); // Converts binary array into unsigned short making the data easier to manipulate

  for (int i=0; i<13; i++) {
    result = raw_data & mask;

    if (result > 0) {
      // ith bit is 1
      bit_stream[i] = 1;
    }
    else {
      // ith bit is a zero
      bit_stream[i] = 0;
    }

    mask <<= 1; // Left shift the mask
  }
}

void EEPROM_Utility::increment_address (bool *address_bit_stream) {
  /** Function description
    * Given the boolean array representing an address in binary, the function will iterate through the array
    * and increment the binary representation by 1 such that the end result array will represent a binary number
    * one higher than the original
    */

  int carry = 0;
  for (int i=0; i<13; i++) {
    if (address_bit_stream[i] == 0 && carry == 0) {
      // Increment and break out of loop
      address_bit_stream[i] = 1;
      break;
    }

    else if (address_bit_stream[i] == 1 && carry == 0) {
      // Reset and set carry
      address_bit_stream[i] = 0;
      carry = 1;
    }

    else if (address_bit_stream[i] == 0 && carry == 1) {
      // Increment and break out of loop
      address_bit_stream[i] = 1;
      break;
    }

    else {
      // Address is 1 and carry is 1
      // Reset and set carry
      address_bit_stream[i] = 0;
      carry = 1;
    }
  }
}

void EEPROM_Utility::decrement_address (bool *address_bit_stream) {
  /** Function description
    * Given the boolean array representing an address in binary, the function will iterate through the array
    * and decrement the binary representation by 1 such that the end result array will represent a binary number
    * one lower than the original
    */

  // Find the least significant 1
  int i=0;
  for (i; i<13; i++) {
    if (address_bit_stream[i] == 1) {
      // Found least significant 1
      // Subtract 1
      address_bit_stream[i] = 0;
      i--;
      break;
    }
  }

  for (i; i>=0; i--) {
    // To properly subtract, all bits behind that 1 must be flipped
    address_bit_stream[i] ^= 1;
  }
}

bool EEPROM_Utility::write(bool *data, bool *address) {
  // Given the address and data binary arrays, output serially into the shift register
  // After loading into the shift register, send EEPROM the write signal
  bool success;
  byte iteration_count = 0;

  do
  {
    for (int i=0; i<13; i++) {
      // For the first eight iterations, program address and data
      // After the eigth iteration, only program in the address

      // Set the address and data lines
      if (i<8) {
        // Program the data line
        digitalWrite(DataRx, data[i]);
      }
      digitalWrite(AddRx, address[i]);

      // Clock the data in
      pulse();
    }

    // Send write commands to EEPROM
    EEPROMWrite();

    // Verify transcription
    success = verify(data, address);
    iteration_count++;
  } while (success == 0 && iteration_count < 3);

  return success;
}

void EEPROM_Utility::read(bool *data, bool *address) {
  // Given the address in binary, the read function will use the shift register to read the byte written at the specified address,
  // Fill the given data pointer with bits stored at the specified address

  // Input the address into the shift register
  for (int i=0; i<13; i++) {
    digitalWrite(AddRx, address[i]);
    pulse();
  }

  // Load the EEPROM data from the specified address into the shift register
  EEPROMLoad();

  // Shift the data out and read one bit at a time
  for (int i=0; i<8; i++) {
    data[i] = (bool)digitalRead(DataTx);
    pulse(); // Advance the shift register
  }
}

byte EEPROM_Utility::compress_bit_stream(bool* data_bit_stream) {
  /** Function description
   *  Converts the 8 bit stream of data into an decimal integer between 0 and 255
   */
  int data_byte = 0;
  int power_multiplier = 1;
  for (int i=0; i<8; i++) {
    data_byte += (int)data_bit_stream[i]*power_multiplier;
    power_multiplier <<= 1;
  }
  return (byte)data_byte;
}

// ############### Private functions ############### //
void EEPROM_Utility::pulse() {
  // Governs the clock pulse logic to control the shift register
  digitalWrite(Clk, HIGH);
  delay(clk_period);
  digitalWrite(Clk, LOW);
}

void EEPROM_Utility::EEPROMWrite() {
  // Send write signal to EEPROM to write data into provided address

  /** Context:
    * At this point it is assumed that the address and data have been piped into the shift register and are currently
    * outputting their values directly into the EEPROM. At this point, the control signals to write should be sent
    * and the EEPROM will write the provided data into the provided address.
    */

  digitalWrite(CE, LOW);
  digitalWrite(WE, LOW);

  delay(EEPROM_write_delay);

  digitalWrite(WE, HIGH);
  digitalWrite(CE, HIGH);
}

void EEPROM_Utility::EEPROMLoad() {
  // Provide the required control signals to load data from the EEPROM into the shift register

  /** Context:
    * This function is called during the read operation.
    * At this point, an address has been inputted into the shift register to read from.
    * By sending the following control signals, the data at the specified address will be loaded into the data
    * flip flops.
    * After loading, the Arduino will read the values one bit at a time and reconstruct the data.
    */

  digitalWrite(Load, HIGH);  // Change shift register to input mode

  // Read signals
  digitalWrite(CE, LOW);
  digitalWrite(OE, LOW);

  delay(EEPROM_load_delay);

  // Clock pulse to load in the data into the shift register
  pulse();

  delay(EEPROM_load_delay); // Test Please Remove

  // Return control signals back to normal
  digitalWrite(Load, LOW);
  digitalWrite(OE, HIGH);
  digitalWrite(CE, HIGH);
}

int EEPROM_Utility::get_baud_rate() {
  return _baud_rate;
}

bool EEPROM_Utility::verify(bool *data_written, bool *address_to_check) {
  /** Function Description
   *  This function takes in the data bit stream written into the EEPROM and the address that the data was written to
   *  The verify function will read back the data in the address to check and compare the read back data against the data written bit by bit
   *  If the two bitstreams are different, the verify function will return false indicating that the write operation failed
   */

  bool correctly_transcribed = true;  // Prove false
  bool read_data[8];
  read(read_data, address_to_check);

  for(int i=0; i<8; i++) {
    if (read_data[i] != data_written[i]) {
      correctly_transcribed = false;
      break;
    }
  }

  return correctly_transcribed;
}