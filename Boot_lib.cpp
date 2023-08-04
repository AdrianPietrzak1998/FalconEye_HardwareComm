#include <iostream>
#include <string>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <handleapi.h>
#include <chrono>
#include <iomanip>
#include <Boot_lib.h>


#define POLY 0x04C11DB7
#define PACKET_SIZE_CONST 1024

static bool ConfigPort(HANDLE hSerial)
{
        // Konfiguracja ustawień portu COM
     DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cout << "COM port download settings error" << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cout << "COM port config error" << std::endl;
        CloseHandle(hSerial);
        return 1;
    }
}





void Update(char *ComPortName, std::string FileName)
{
    HANDLE hBootloader;

    enum Step{
	ECHO = 0x00,
	FULL_SIZE = 0x01,
	FULL_CRC = 0x02,
	EREASE = 0x03,
	PACKET_SIZE = 0x04,
	PACKET_ADDRESS = 0x05,
	PACKET_CRC = 0x06,
	RECEIVE_PACKET = 0x07,
	FULL_CRC_CALCULATE = 0x08,
	END = 0x09
}BootStep;

union data {
    uint32_t data32[1024 * 1024 / 4];
    uint8_t data8[1024 * 1024];
}data;




       hBootloader = CreateFile(ComPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hBootloader == INVALID_HANDLE_VALUE)
    {
        std::cout<<"blad portu com" << std::endl;
    }
    else
    {
        std::cout<<"Otwarto port com" << std::endl;
    }

    ConfigPort(hBootloader);

    std::ifstream binaryFile(FileName, std::ios::binary);
    if(!binaryFile)
    {
        std::cout << "Open file error" << std::endl;
        return;
    }

    binaryFile.read(reinterpret_cast<char*>(data.data8), sizeof(data.data8));

    uint32_t binary_size = binaryFile.gcount() - 4;
    binaryFile.close();
    uint16_t packet_quanity = (binary_size%PACKET_SIZE_CONST)? binary_size / PACKET_SIZE_CONST + 1 : binary_size / PACKET_SIZE_CONST;
    //uint32_t binary_crc = calculate_crc32(data.data32, binary_size/4);
    uint32_t binary_crc = data.data32[binary_size/4];

    std::cout <<  "Size: "<< std::fixed << std::setprecision(2) << static_cast<double>(binary_size)/1024 << "kB" << std::endl;
    std::cout << "Pakietow: " << packet_quanity << std::endl;
    std::cout << "CRC: 0x" << std::hex << binary_crc << std::dec << std::endl << std::endl << std::endl;

    SendCommandToBootloader(ECHO, 0, hBootloader);
    SendCommandToBootloader(FULL_SIZE, binary_size, hBootloader);
    SendCommandToBootloader(FULL_CRC, binary_crc, hBootloader);
    std::cout<<"Erasing"<<std::endl;
    SendCommandToBootloader(EREASE, 0, hBootloader);


    for(uint32_t ByteToSend = binary_size; ByteToSend > 0; )
    {
        uint16_t ActualPacketSize;
        uint32_t ActualAddress;
        uint32_t ActualPacketCRC;

        if(ByteToSend >=PACKET_SIZE_CONST)
        {
            ActualPacketSize = PACKET_SIZE_CONST;
        }
        else
        {
            ActualPacketSize = ByteToSend;
        }

        ActualPacketCRC = calculate_crc32(data.data32+ActualAddress/4, ActualPacketSize/4);

        //
        //handling transmit packet
        //
        SendCommandToBootloader(PACKET_SIZE, ActualPacketSize, hBootloader);
        SendCommandToBootloader(PACKET_ADDRESS ,ActualAddress, hBootloader);
        SendCommandToBootloader(PACKET_CRC, ActualPacketCRC, hBootloader);
        SendCommandToBootloader(RECEIVE_PACKET, 0xff4e, hBootloader);
        SendPacket(ActualAddress, data.data8, ActualPacketSize, hBootloader);


        ///
        ///PacketDone
        ///
        ByteToSend -= ActualPacketSize;
        std::cout << std::dec << "Wyslano: " << ActualPacketSize << " B | Pod adres: 0x" << std::hex << ActualAddress << " | Zostalo: " << std::dec << ByteToSend << " B | CRC: 0x" << std::hex << ActualPacketCRC << std::dec <<std::endl;
        ActualAddress += ActualPacketSize;


    }

    SendCommandToBootloader(FULL_CRC_CALCULATE, 0xffce, hBootloader);
    if(SendCommandToBootloader(END, 0xffee, hBootloader) != 0x00)
       {
        std::cout<<"Ended fail"<<std::endl;
       }
    else
        {
           std::cout<<"Ended ";
        }
    CloseHandle(hBootloader);
    std::cout << "DONE!" << std::endl;
}







uint8_t SendCommandToBootloader(uint8_t NumberOfCommand, uint32_t Argument, HANDLE hCom)
{
    union Command{
	uint8_t Buff[5];
	struct{
		uint32_t Argument;
		uint8_t Command_number;
	};
}Command;
    //Sleep(200);
    Command.Command_number = NumberOfCommand;
    Command.Argument = Argument;
    DWORD bytesWritten;

    if (!WriteFile(hCom, Command.Buff, 5, &bytesWritten, NULL))
    {
        std::cout << "COM port write error" << std::endl;
        return 0xFF;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 10000; // Timeout
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(hCom, &timeouts);

    uint8_t response;
    DWORD bytesRead;

    if (ReadFile(hCom, &response, sizeof(response), &bytesRead, NULL))
    {
        if (bytesRead)
        {
            return response;
        }
        else
        {
            // Brak odpowiedzi w czasie
            std::cout << "Timeout occurred" << std::endl;
            return 0xFF;
        }
    }
    else
    {
        // Błąd odczytu z portu szeregowego
        std::cout << "COM port read error" << std::endl;
        return 0xFF;
    }
}











uint8_t SendPacket(uint32_t AddressOffset, uint8_t *Packet, uint32_t Size, HANDLE hCom)
{
    //Sleep(200);
   DWORD bytesWritten;

    if (!WriteFile(hCom, Packet + AddressOffset , Size, &bytesWritten, NULL))
    {
        std::cout << "COM port write error" << std::endl;
        return 0xFF;
    }


        COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 10000; // Timeout
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(hCom, &timeouts);

    uint8_t response;
    DWORD bytesRead;

    if (ReadFile(hCom, &response, sizeof(response), &bytesRead, NULL))
    {
        if (bytesRead)
        {
            return response;
        }
        else
        {
            // Brak odpowiedzi w czasie
            std::cout << "Timeout occurred" << std::endl;
            return 0xFF;
        }
    }
    else
    {
        // Błąd odczytu z portu szeregowego
        std::cout << "COM port read error" << std::endl;
        return 0xFF;
    }
}


uint32_t calculate_crc32(uint32_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (int j = 0; j < 32; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ POLY;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}


