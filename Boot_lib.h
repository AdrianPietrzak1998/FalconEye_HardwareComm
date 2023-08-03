#ifndef BOOT_LIB_H_INCLUDED
#define BOOT_LIB_H_INCLUDED

#include <iostream>
#include <string>
#include <handleapi.h>

uint32_t calculate_crc32(uint32_t* data, size_t length);
uint8_t SendCommandToBootloader(uint8_t NumberOfCommand, uint32_t Argument, HANDLE hCom);
uint8_t SendPacket(uint32_t AddressOffset, uint8_t *Packet, uint32_t Size, HANDLE hCom);
void Update(char *ComPortName, std::string FileName);



#endif // BOOT_LIB_H_INCLUDED
