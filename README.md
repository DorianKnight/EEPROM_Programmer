# EEPROM_Programmer
![image](https://github.com/user-attachments/assets/a5cba170-78f9-45bd-aac2-f92fe1cdf1a0)
## Project Overview
Custom Serializer Deserializer (SerDes) PCB designed to handle read/write operations to a parallel address EEPROM.
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/IMG_20260207_110320803.jpg)

## High Level Architecture
Data from the host computer is transferred through serial communication to the Arduino. The Arduino writes the serial data into the SerDes which outputs the data in parallel to the EEPROM for storage.

To read information from the EEPROM, the Arduino loads the parallel data output into the SerDes and reconstructs the byte packet bit by bit. A high level functional block diagram explaining the system can be seen below.
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/High%20level%20user%20flow(white_background).png)
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/Motherboard%20functional%20diagram(V2).png)

## SerDes
A custom shift register functions as a primitive SerDes. Going from serial to parallel, the custom hardware allows serial data to ripple into individual flip-flops where each flip-flop drives an individual output. These outputs then wire into the EEPROM’s address and data pins. The same hardware also functions to load in the parallel data output from the EEPROM and shift it out sequentially such that the Arduino can reconstruct the byte packet.
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/IMG_20260207_110930291.jpg)
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/Shift%20Register%20LayoutV3.png)

## Debugging
To facilitate debugging operations, a debug daughter board PCB was constructed that allows the technician to manually toggle all control signals made available to the Arduino MCU.
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/IMG_20260207_111005096.jpg)
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/Debug%20Board%20Layout.png)

## Motherboard Interface
The motherboard hosts a series of LEDs and seven-segment displays that serve to display the byte stored at a specific address. By changing the board to debug mode, the technician can select an address to read its binary and hexadecimal representation.
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/IMG_20260207_111747989.jpg)
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/Motherboard%20Layout.png)

## Custom Bin -> Hex 7-SD Driver 
Binary to hexadecimal conversion was accomplished through the design of custom digital combinational logic that uses a series of primitive logic gates to turn a four bit binary input into a seven pin output. The individual outputs turn on or off the individual segments on a seven segment display.
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/IMG_20260207_110945020.jpg)
![image](https://github.com/DorianKnight/EEPROM_Programmer/blob/main/readme_images/Seven%20Segment%20Decoder%20Layout.png)


