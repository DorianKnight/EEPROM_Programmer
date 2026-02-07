// Author: Dorian Knight
// Created: August 6th 2025
// Updated: August 8th 2025
// Description: Command interpreter for EEPROM Programmer firmware

#include "EEPROM_Programmer_Utility.h"

bool error_bit = 0;
const byte WRITE_COMMAND_LEN = 0x0E; // hex(0x0E) = dec(14)
const byte READ_COMMAND_LEN  = 0x04; // hex(0x04) = dec(4)
const byte SERIAL_BUFFER_MAX = 0x40; // hex(0x40) = dec(64)
const byte SHIFT_COMMAND_LEN = 0x06;
const unsigned short METADATA_LEN = 12;
const unsigned short MAX_EEPROM_SPACE = 8192;

// Binary array to pipe into shift register
bool data_package[64][8];
bool address_bit_stream[13];

// Byte array to store information from computer
byte command_info[WRITE_COMMAND_LEN];
byte file_data[SERIAL_BUFFER_MAX];
byte stop_byte_collector[1];

EEPROM_Utility Memory_Manager(9600);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(Memory_Manager.get_baud_rate());
  Memory_Manager.begin();
  pinMode(LED_BUILTIN, OUTPUT); // Error LED
}

void write_manager() {
  /** Function description
    * Parses file metadata and handles bulk file data transfer from the computer into the EEPROM
    * The metadata contains the start address of the file, end address of the file and the name of the file (max 8 characters)
    * After writing the meta data into the "file allocation table" section of the EEPROM, the write_manager will signal the computer to send over the bulk data 64 bytes at a time
    * The after reading in the bulk data, the write_manager will transfer that data into the EEPROM in accordance to the file's metadata
    */

  /************ Metadata processing ************/

  // Read in full write command
  Serial.readBytes(command_info, WRITE_COMMAND_LEN);
  Serial.readBytes(stop_byte_collector,1); // Clear 0x0a from serial buffer

  // Package address and data into appropriate binary streams
  Memory_Manager.construct_address_bitstream(command_info, address_bit_stream);  // First two bytes of command_info is the metadata start address
  Memory_Manager.data_packager(command_info + 2, (bool *)data_package, METADATA_LEN);  // The remaining 12 bytes of data from command_info is the metadata to be written

  // Write metadata to EEPROM
  for(int i=0; i<METADATA_LEN; i++) {
    if (Memory_Manager.write(data_package[i], address_bit_stream) == 0) {
      error_funct();
    }
    Memory_Manager.increment_address(address_bit_stream);
  }

  /************ File data processing ************/
  // Assemble metadata
  unsigned short file_start;
  unsigned short file_end;

  memcpy(&file_start, command_info+2, 2);
  memcpy(&file_end, command_info+4, 2);
  unsigned short file_len = file_end - file_start + 1; // +1 because the end is inclusive

  // Keeps track of how much data has been written
  unsigned short bytes_written = 0;
  unsigned short bytes_remaining = file_len;
  unsigned short bytes_to_write = 0;

  // Create starting address bit stream
  Memory_Manager.construct_address_bitstream((byte *)&file_start, address_bit_stream);

  // Signal computer to commence data transfer
  Serial.write(0x7e);

  // Read in data from serial buffer and write to EEPROM in 64 byte chunks
  while (bytes_written < file_len) {
    // Send acknowledge bit back to computer

    // Wait for buffer to fill with data
    while (Serial.available() < SERIAL_BUFFER_MAX && Serial.available() < bytes_remaining);

    if (bytes_remaining < SERIAL_BUFFER_MAX) {
      bytes_to_write = bytes_remaining;
    }

    else {
      bytes_to_write = SERIAL_BUFFER_MAX;
    }

    Serial.readBytes(file_data, bytes_to_write);
    Serial.readBytes(stop_byte_collector, 1); // Collect 0x0a stop byte at the end
    Serial.write(0x7e); // Send acknowledge bit back to computer such that it can fill the buffer while the Arduino writes data into the EEPROM

    // Package and write data in buffer into EEPROM
    Memory_Manager.data_packager(file_data, (bool *)data_package, bytes_to_write);
    for (int i=0; i<bytes_to_write; i++) {
      if (Memory_Manager.write(data_package[i], address_bit_stream) == 0) {
        error_funct();
      }
      Memory_Manager.increment_address(address_bit_stream);
    }

    bytes_written += bytes_to_write;
    bytes_remaining -= bytes_to_write;
  }
}


void read_manager() {
  /** Function description
    * The read manager is called when a read command is presented to the Arduino firmware
    * This read command contains two important data points: Address start, address end, each of which are two bytes
    * The read manager should read from start to finish, and send the data over Serial in chunks of 64 bytes at a time
    */

  Serial.readBytes(command_info, READ_COMMAND_LEN +1); // +1 to remove the 0x0a from the serial buffer

  unsigned short file_start;
  unsigned short file_end;
  unsigned short file_len;
  unsigned short bytes_remaining;
  byte bytes_to_read;

  memcpy(&file_start, command_info, 2);
  memcpy(&file_end, command_info+2, 2);

  file_len = file_end - file_start + 1;  // +1 because the file end is inclusive
  bytes_remaining = file_len;

  // Test
  Serial.println(file_start);
  Serial.println(file_end);

  Memory_Manager.construct_address_bitstream((byte *)&file_start, address_bit_stream);

  while(bytes_remaining > 0) {

    if (bytes_remaining < SERIAL_BUFFER_MAX) {
      bytes_to_read = bytes_remaining;
    }

    else {
      bytes_to_read = SERIAL_BUFFER_MAX;
    }

    // Read the required amount of bytes

    for (int i=0; i<bytes_to_read; i++) {
      Memory_Manager.read(data_package[i], address_bit_stream);  // Read individual bits from EEPROM shift register
      file_data[i] = Memory_Manager.compress_bit_stream(data_package[i]);  // Convert bit stream into a singular byte of data and store in buffer;
      Memory_Manager.increment_address(address_bit_stream);  // Move to next address

      // Test
      Serial.println(file_data[i]);
    }

    // Write bytes in memory to computer over serial
    Serial.write(file_data, bytes_to_read);

    bytes_remaining -= bytes_to_read;
  }
}


void data_shift_command() {
  /** Function description
    * The data shift command is composed of three arguments, start, stop, and the number to shift by (signed)
    * because the number to shift by is a signed number we can shift forwards or backwards
    */


  // Read in command
  Serial.readBytes(command_info, SHIFT_COMMAND_LEN);
  Serial.readBytes(stop_byte_collector, 1); // Read the stop byte (0x0a) to remove it from the serial buffer

  unsigned short shift_start;
  unsigned short shift_end;
  unsigned short current_address;
  signed short shift_amplitude;
  signed short shift_destination;

  memcpy(&shift_start,     command_info,   2);
  memcpy(&shift_end,       command_info+2, 2);
  memcpy(&shift_amplitude, command_info+4, 2);

  shift_destination = (signed short)current_address + shift_amplitude;

  bool destination_address[8];
  if (shift_amplitude > 0) {
    // Shift data from lower address to higher address
    // Start from higher addresses and work down to the lower addresses
    current_address = shift_end;

    // Construct address bit streams
    Memory_Manager.construct_address_bitstream((byte *)&shift_end, address_bit_stream);
    Memory_Manager.construct_address_bitstream((byte *)&shift_destination, destination_address);
    for (current_address; current_address >= shift_start; current_address--) {
      // Read in data at current address
      Memory_Manager.read(data_package[0], address_bit_stream);

      // Write data to destination address
      Memory_Manager.write(data_package[0], destination_address);

      // Decrement address and repeat process
      Memory_Manager.decrement_address(address_bit_stream);
      Memory_Manager.decrement_address(destination_address);
    }
  }

  else {
    // Shift data from higher address to lower address
    // Start with lower addresses and work up to the higher addresses
    current_address = shift_start;

    // Construct address bit streams
    Memory_Manager.construct_address_bitstream((byte *)&shift_start, address_bit_stream);
    Memory_Manager.construct_address_bitstream((byte *)&shift_destination, destination_address);
    for (current_address; current_address <= shift_end; current_address++) {
      // Read in data at current address
      Memory_Manager.read(data_package[0], address_bit_stream);

      // Write data to destination address
      Memory_Manager.write(data_package[0], destination_address);

      // Increment address and repeat process
      Memory_Manager.increment_address(address_bit_stream);
      Memory_Manager.increment_address(destination_address);
    }
  }
}

void eeprom_format_command() {
  /** Function description
    * Called when the entire EEPROM chip needs to be erased
    * File allocation size reset to zero
    */

  file_data[0] = 0x5E;
  byte empty_memory_data = 0xFF;
  byte start_address = 0x00;

  Memory_Manager.data_packager(file_data, (bool *)data_package, 1);
  Memory_Manager.data_packager(&empty_memory_data, (bool *)data_package+1, 1);
  Memory_Manager.construct_address_bitstream(&start_address, address_bit_stream);

  for (int i=0; i<MAX_EEPROM_SPACE; i++) {
    if (i < 2) {
      Memory_Manager.write(data_package[0], address_bit_stream);
    }

    else {
      Memory_Manager.write(data_package[1], address_bit_stream);
    }

    Memory_Manager.increment_address(address_bit_stream);
  }
}

void loop() {
  // Serial.println("In main function");
  // put your main code here, to run repeatedly:
  byte command;
  if (Serial.available() > 0) {
    // Interpret command, parse data and call utility function
    command = Serial.read();
    Serial.println(command);
    switch (command) {
      case 64:  // Write command
        write_manager();
        break;
      case 65:  // Read command
        read_manager();
        break;
      case 2:  // Shift data command
        data_shift_command();
        break;
      case 3:  // Format EEPROM command
        eeprom_format_command();
        break;
      default:
        error_funct();
        break;
    }
  }
}

void error_funct() {
  // Break out of infinite loop
  Serial.println("Unexpected error has occured");
  while(true) // Infinite loop to trap program execution @TODO: find a better solution
  {
    digitalWrite(LED_BUILTIN, HIGH);  // Signal Error has occured
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

