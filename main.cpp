#include <iostream>
#include <string>
#include <thread>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <handleapi.h>
#include <mutex>
#include <atomic>
#include <Boot_lib.h>
#include <vector>
#include <chrono>
#include <filesystem>

#define CONTROL_BOARD "control-"
#define TOP_BOARD "top-"
#define ENDLINE '^'

enum DevciceName{
DEV_CONTROL_BOARD = 0,
DEV_TOP_BOARD,
DEV_CONTROL_BOARD_BOOT,
DEV_TOP_BOARD_BOOT,
DEV_QUANITY
}DeviceName;

uint8_t ConectedDevices[DEV_QUANITY];
uint8_t PortIsOpen[DEV_QUANITY];

char ControlBoardPort[25];
char TopBoardPort[25];
char ControlBoardPortBoot[25];
char TopBoardPortBoot[25];

const uint8_t ArgumentQuanityControlBoard[] = {11, 8, 0, 0, 0, 0, 0, 0, 0, 0, 2};
const uint8_t ArgumentQuanityTopBoard[] = {8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 2};

bool ExitVar = false;

std::string TerminalData;


void ReadPort(HANDLE hSerial, void(*ptrParser)(char *));
void WritePort(HANDLE hSerial, std::string message);
bool ConfigPort(HANDLE hSerial);
void WriteTxt(void);
void ParserControlBoard(char *buffer);
void ParserTopBoard(char *buffer);
void searchComPort(void);
void OpenPorts(void);
void CloseReads(void);
void TerminalDataParser(std::string TerminalData);
void ReadCommand(void);

//std::mutex mtx;
volatile bool threadTerminate;
volatile bool readThreadTerminate;
volatile bool shared;


struct ControlBoardReceivedVar{
    char Input[20];
    char Output [20];
    char Pwm1[5];
    char Pwm2[5];
    char Pwm3[5];
    char Pwm4[5];
    char Temp[10];
    char McuTemp[10];
    char Volt12[10];
    char Volt5[10];
    char Curr[10];

    char LightMode[10];
    char LightPwm[10];
    char LightDimm[10];
    char LogoMode[10];
    char LogoPwm[10];
    char LogoDimm[10];
    char ErrorCode[10];
    char DoorOpen[5];

    char uId[40];
    char Version[10];
}ControlBoardReceivedVar;

struct TopBoardReceivedVar{
    char hLed[10];
    char sLed[10];
    char vLed[10];
    char modeLed[10];
    char fxDelayLed[10];
    char blinkDelayLed[10];
    char reverseDirLed[10];
    char gammaLed[10];

    char Temperature[10];
    char FanPWM[10];
    char FanSpeed[10];
    char CaseOpen[10];
    char ErrorCode[10];

    char uId[40];
    char Version[10];
}TopBoardReceivedVar;

    // Otwarcie portu COM
    HANDLE hControlBoard;
    HANDLE hTopBoard;

    std::thread readThreadControlBoard;
    std::thread readThreadTopBoard;

    std::thread restartCom;

    void removeSubstring(std::string& str, const std::string& substr) {
    size_t pos = str.find(substr);
    if (pos != std::string::npos) {
        str.erase(pos, substr.length());
    }
}

int main() {
    searchComPort();
   // sprintf(ControlBoardPort, "\\\\.\\COM7");

     if(ConectedDevices[DEV_CONTROL_BOARD])hControlBoard = CreateFile(ControlBoardPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     if(ConectedDevices[DEV_TOP_BOARD])hTopBoard = CreateFile(TopBoardPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hControlBoard == INVALID_HANDLE_VALUE && ConectedDevices[DEV_CONTROL_BOARD]) {
        std::cout << "Nie można otworzyć portu COM ControlBoard." << std::endl;
        //return 1;
    }
    else PortIsOpen[DEV_CONTROL_BOARD] = 1;
       if (hTopBoard == INVALID_HANDLE_VALUE && ConectedDevices[DEV_TOP_BOARD]) {
        std::cout << "Nie można otworzyć portu COM TopBoard." << std::endl;
        //return 1;
    }
    else PortIsOpen[DEV_TOP_BOARD] = 1;

    if(ConectedDevices[DEV_CONTROL_BOARD])ConfigPort(hControlBoard);
    if(ConectedDevices[DEV_TOP_BOARD])ConfigPort(hTopBoard);



     if(ConectedDevices[DEV_CONTROL_BOARD])
     {
         readThreadControlBoard = std::thread(ReadPort, hControlBoard, ParserControlBoard);
     }
     if(ConectedDevices[DEV_TOP_BOARD])
     {
         readThreadTopBoard = std::thread(ReadPort, hTopBoard, ParserTopBoard);
     }



    // Uruchomienie watku do zapisu zmiennych do pliku
    std::thread writeTxtThread(WriteTxt);
    std::thread ReadCommandThread(ReadCommand);


    while(ExitVar == false)
    {
        if(shared)
        {
            std::cout<<"share"<<std::endl;
            shared = false;
        }

        getline(std::cin, TerminalData);

        TerminalDataParser(TerminalData);

        if(restartCom.joinable())restartCom.join();


        std::cin.clear();


    }

    // Oczekiwanie na wciśnięcie klawisza Enter
   // std::cin.ignore();

    // Zakończenie wątków
    if(readThreadControlBoard.joinable()) readThreadControlBoard.join();
    if(readThreadTopBoard.joinable()) readThreadTopBoard.join();
    if(restartCom.joinable())restartCom.join();
    //writeThread.join();
    writeTxtThread.join();
    ReadCommandThread.join();

    // Zamknięcie portu COM
    CloseHandle(hControlBoard);
    CloseHandle(hTopBoard);

    return 0;
}

void TerminalDataParser(std::string TerminalData)
{

        if(!TerminalData.compare("exit"))
        {
            std::cout << "exit" << std::endl;
            threadTerminate = TRUE;
            ExitVar = true;
            return;
        }

        TerminalData += ENDLINE;
        if (TerminalData.find(CONTROL_BOARD) != std::string::npos)
        {
            removeSubstring(TerminalData, CONTROL_BOARD);
            if(TerminalData.find("update") != std::string::npos)
            {
                if(ConectedDevices[DEV_CONTROL_BOARD])WritePort(hControlBoard, "08/1234^");
                CloseReads();
                Sleep(2000);
                searchComPort();
                if(ConectedDevices[DEV_CONTROL_BOARD_BOOT])Update(hControlBoard, ControlBoardPortBoot, "Update/FalconEye ControlBoard_update.bin");
                else std::cout << "Bootmode fault" << std::endl;
                Sleep(2000);
                OpenPorts();
            }
            else
            {
                std::cout << TerminalData << std::endl;
                WritePort(hControlBoard, TerminalData);
            }
            TerminalData.clear();
        }
        else if (TerminalData.find(TOP_BOARD) != std::string::npos)
        {
            removeSubstring(TerminalData, TOP_BOARD);
            if(TerminalData.find("update") != std::string::npos)
            {
                if(ConectedDevices[DEV_TOP_BOARD])WritePort(hTopBoard, "07/1234^");
                CloseReads();
                Sleep(2000);
                searchComPort();
                if(ConectedDevices[DEV_TOP_BOARD_BOOT])Update(hTopBoard, TopBoardPortBoot, "Update/FalconEye TopBoard_update.bin");
                else std::cout << "Bootmode fault" << std::endl;
                Sleep(2000);
                OpenPorts();
            }
            else
            {
                std::cout << TerminalData << std::endl;
                WritePort(hTopBoard, TerminalData);
            }
            TerminalData.clear();
        }
        else if (TerminalData.compare("refresh"))
        {
            OpenPorts();
        }
        return;
}

void ReadCommand(void)
{
    std::filesystem::file_time_type Time;
    while(true)
    {
        if(std::filesystem::last_write_time("Input.txt") != Time)
        {
            std::ifstream file("Input.txt");
            if (file.peek() != std::ifstream::traits_type::eof())
            {
                std::string Line;
                if(!file.is_open())
                {
                    std::cout << "Error open file" << std::endl;
                    return;
                }
                while(getline(file, Line))
                {
                    TerminalDataParser(Line);
                }
                file.close();
                std::ofstream clrFile("Input.txt");
                if(!clrFile.is_open())
                {
                    std::cout << "Error clear file" << std::endl;
                    return;
                }
                clrFile.close();
            }
            Time = std::filesystem::last_write_time("Input.txt");

        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if(threadTerminate) break;
    }
}

void ReadPort(HANDLE hSerial, void(*ptrParser)(char*)) {
    std::string receivedMessage;

    while (!threadTerminate && !readThreadTerminate) {
        char currentChar;
        DWORD bytesRead;

        if (ReadFile(hSerial, &currentChar, sizeof(currentChar), &bytesRead, NULL)) {
            if (bytesRead > 0) {
                if (currentChar == ENDLINE) {
                    // Znaleziono koniec wiadomości, przetwarzanie odczytanej wiadomości
                    char Msg[1024];
                    strcpy(Msg, receivedMessage.c_str());
                    ptrParser(Msg);
                    receivedMessage.clear();
                } else {
                    // Dodanie znaku do odczytanej wiadomości
                    receivedMessage += currentChar;
                }
            }
        } else {
            std::cout << "COM port read error" << std::endl;
            if (!restartCom.joinable()) {
                restartCom = std::thread(OpenPorts);
            }
            break;
        }
    }
}




void WritePort(HANDLE hSerial, std::string message) {
    //std::string message = "2/3^";
   // while (true) {
        DWORD bytesWritten;
        if (!WriteFile(hSerial, message.c_str(), message.length(), &bytesWritten, NULL)) {
            std::cout << "COM port write error" << std::endl;
            //break;
        } else {
            //std::cout << "Zapisano " << bytesWritten << " bajtów do portu COM." << std::endl;
        }

       // if(threadTerminate) break;
       // std::this_thread::sleep_for(std::chrono::seconds(1));
    //}
}

bool ConfigPort(HANDLE hSerial)
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

void WriteTxt(void)
{
    while(true)
    {
        //printf("000/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s/%s\n", ControlBoardReceivedVar.Input, ControlBoardReceivedVar.Output, ControlBoardReceivedVar.Pwm1, ControlBoardReceivedVar.Pwm2, ControlBoardReceivedVar.Pwm3, ControlBoardReceivedVar.Pwm4, ControlBoardReceivedVar.Temp, ControlBoardReceivedVar.McuTemp, ControlBoardReceivedVar.Volt12, ControlBoardReceivedVar.Volt5, ControlBoardReceivedVar.Curr);
        //printf("001/%s/%s/%s/%s/%s/%s\n", ControlBoardReceivedVar.LightMode, ControlBoardReceivedVar.LightPwm, ControlBoardReceivedVar.LightDimm, ControlBoardReceivedVar.LogoMode, ControlBoardReceivedVar.LogoPwm, ControlBoardReceivedVar.LogoDimm);
        //printf("010/%s\n", ControlBoardReceivedVar.uId);

        std::string ControlState;
        if(ConectedDevices[DEV_CONTROL_BOARD]) ControlState = "CONNECT";
        else if(ConectedDevices[DEV_CONTROL_BOARD_BOOT]) ControlState = "BOOT";
        else ControlState = "DISCONNECT";

        std::string TopState;
        if(ConectedDevices[DEV_TOP_BOARD]) TopState = "CONNECT";
        else if(ConectedDevices[DEV_TOP_BOARD_BOOT]) TopState = "BOOT";
        else TopState = "DISCONNECT";

        std::ofstream OutFile;
        std::ostringstream ControlBoardStream;
                  ControlBoardStream << "----Device connected----" << std::endl
                    << "Control: " << ControlState <<std::endl
                    << "Top: " << TopState << std::endl
                    << "Printer: " << std::endl
                    << "----ControlBoard " << ControlBoardReceivedVar.uId << "----" << std::endl
                    << "Version" << ControlBoardReceivedVar.Version << std::endl
                    << "Input: " << ControlBoardReceivedVar.Input << std::endl
                    << "Output: " << ControlBoardReceivedVar.Output << std::endl
                    << "Pwm1: " << ControlBoardReceivedVar.Pwm1 << std::endl
                    << "Pwm2: " << ControlBoardReceivedVar.Pwm2 << std::endl
                    << "Pwm3: " << ControlBoardReceivedVar.Pwm3 << std::endl
                    << "Pwm4: " << ControlBoardReceivedVar.Pwm4 << std::endl
                    << "Temperature: " << ControlBoardReceivedVar.Temp << std::endl
                    << "MCU Temperature: " << ControlBoardReceivedVar.McuTemp << std::endl
                    << "12V: " << ControlBoardReceivedVar.Volt12 << std::endl
                    << "5V: " << ControlBoardReceivedVar.Volt5 << std::endl
                    << "Current: " << ControlBoardReceivedVar.Curr << std::endl
                    << "Light mode:" << ControlBoardReceivedVar.LightMode << std::endl
                    << "Light pwm: " << ControlBoardReceivedVar.LightPwm << std::endl
                    << "Light dimmer time: " << ControlBoardReceivedVar.LightDimm << std::endl
                    << "Logo mode: " << ControlBoardReceivedVar.LogoMode << std::endl
                    << "Logo pwm: " << ControlBoardReceivedVar.LogoPwm << std::endl
                    << "Logo dimmer time: " << ControlBoardReceivedVar.LogoDimm << std::endl
                    << "Door is open: " << ControlBoardReceivedVar.DoorOpen << std::endl
                    << "Error code: " << ControlBoardReceivedVar.ErrorCode <<std::endl
                    << "----TopBoard " << TopBoardReceivedVar.uId << "----" << std::endl
                    << "Version" << TopBoardReceivedVar.Version << std::endl
                    << "H led value: " << TopBoardReceivedVar.hLed << std::endl
                    << "S led value: " << TopBoardReceivedVar.sLed << std::endl
                    << "V led value: " << TopBoardReceivedVar.vLed << std::endl
                    << "Mode Led: " << TopBoardReceivedVar.modeLed << std::endl
                    << "Fx delay led: " << TopBoardReceivedVar.fxDelayLed << std::endl
                    << "Blink delay led: " << TopBoardReceivedVar.blinkDelayLed << std::endl
                    << "Reverse fx enabled: " << TopBoardReceivedVar.reverseDirLed << std::endl
                    << "Gamma corection enabled: " << TopBoardReceivedVar.gammaLed << std::endl
                    << "Temperature: " << TopBoardReceivedVar.Temperature << std::endl
                    << "Fan PWM: " << TopBoardReceivedVar.FanPWM << std::endl
                    << "Fan speed: " << TopBoardReceivedVar.FanSpeed << std::endl
                    << "Case open: " << TopBoardReceivedVar.CaseOpen << std::endl
                    << "Error code: " << TopBoardReceivedVar.ErrorCode;


        OutFile.open("Output.txt");
        if(OutFile.is_open())
        {
            OutFile << ControlBoardStream.str();
            OutFile.close();

        }

        ControlBoardStream.clear();

        if(threadTerminate) break;


        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void ParserControlBoard(char *buffer) {
    char *ParsePointer = buffer;
    uint8_t CommandID = atoi(ParsePointer);
    char tmpCommandArgument[20][40]; // 20 command arg 30 char

    std::vector<std::string> parsedArgs; // Przechowuje sparsowane argumenty

    for (uint8_t i = 0; i < ArgumentQuanityControlBoard[CommandID]; i++) {
        ParsePointer = strchr(ParsePointer, '/');
        if (!ParsePointer) {
            break;
        }
        ParsePointer++; // Przesunięcie wskaźnika za znak '/'
        char *nextSeparator = strchr(ParsePointer, '/');
        if (!nextSeparator) {
            nextSeparator = strchr(ParsePointer, '\0');
        }
        size_t argLength = nextSeparator - ParsePointer;
        std::string arg(ParsePointer, argLength);
        parsedArgs.emplace_back(arg);
        ParsePointer = nextSeparator;
    }

    switch (CommandID) {
    case 0:
        if (parsedArgs.size() >= 11) {
            strcpy(ControlBoardReceivedVar.Input, parsedArgs[0].c_str());
            strcpy(ControlBoardReceivedVar.Output, parsedArgs[1].c_str());
            strcpy(ControlBoardReceivedVar.Pwm1, parsedArgs[2].c_str());
            strcpy(ControlBoardReceivedVar.Pwm2, parsedArgs[3].c_str());
            strcpy(ControlBoardReceivedVar.Pwm3, parsedArgs[4].c_str());
            strcpy(ControlBoardReceivedVar.Pwm4, parsedArgs[5].c_str());
            strcpy(ControlBoardReceivedVar.Temp, parsedArgs[6].c_str());
            strcpy(ControlBoardReceivedVar.McuTemp, parsedArgs[7].c_str());
            strcpy(ControlBoardReceivedVar.Volt12, parsedArgs[8].c_str());
            strcpy(ControlBoardReceivedVar.Volt5, parsedArgs[9].c_str());
            strcpy(ControlBoardReceivedVar.Curr, parsedArgs[10].c_str());
        }
        break;
    case 1:
        if (parsedArgs.size() >= 8) {
            strcpy(ControlBoardReceivedVar.LightMode, parsedArgs[0].c_str());
            strcpy(ControlBoardReceivedVar.LightPwm, parsedArgs[1].c_str());
            strcpy(ControlBoardReceivedVar.LightDimm, parsedArgs[2].c_str());
            strcpy(ControlBoardReceivedVar.LogoMode, parsedArgs[3].c_str());
            strcpy(ControlBoardReceivedVar.LogoPwm, parsedArgs[4].c_str());
            strcpy(ControlBoardReceivedVar.LogoDimm, parsedArgs[5].c_str());
            strcpy(ControlBoardReceivedVar.ErrorCode, parsedArgs[6].c_str());
            strcpy(ControlBoardReceivedVar.DoorOpen, parsedArgs[7].c_str());
        }
        break;
    case 10:
        if (parsedArgs.size() >= 2) {
            strcpy(ControlBoardReceivedVar.uId, parsedArgs[0].c_str());
            strcpy(ControlBoardReceivedVar.Version, parsedArgs[1].c_str());
        }
        break;
    }
}
void ParserTopBoard(char *buffer) {
    char *ParsePointer = buffer;
    uint8_t CommandID = atoi(ParsePointer);
    char tmpCommandArgument[20][30]; // 20 command arg 30 char

    std::vector<std::string> parsedArgs; // Przechowuje sparsowane argumenty

    for (uint8_t i = 0; i < ArgumentQuanityTopBoard[CommandID]; i++) {
        ParsePointer = strchr(ParsePointer, '/');
        if (!ParsePointer) {
            break;
        }
        ParsePointer++; // Przesunięcie wskaźnika za znak '/'
        char *nextSeparator = strchr(ParsePointer, '/');
        if (!nextSeparator) {
            nextSeparator = strchr(ParsePointer, '\0');
        }
        size_t argLength = nextSeparator - ParsePointer;
        std::string arg(ParsePointer, argLength);
        parsedArgs.emplace_back(arg);
        ParsePointer = nextSeparator;
    }

    switch (CommandID) {
    case 0:
        if (parsedArgs.size() >= 8) {
            strcpy(TopBoardReceivedVar.hLed, parsedArgs[0].c_str());
            strcpy(TopBoardReceivedVar.sLed, parsedArgs[1].c_str());
            strcpy(TopBoardReceivedVar.vLed, parsedArgs[2].c_str());
            strcpy(TopBoardReceivedVar.modeLed, parsedArgs[3].c_str());
            strcpy(TopBoardReceivedVar.fxDelayLed, parsedArgs[4].c_str());
            strcpy(TopBoardReceivedVar.blinkDelayLed, parsedArgs[5].c_str());
            strcpy(TopBoardReceivedVar.reverseDirLed, parsedArgs[6].c_str());
            strcpy(TopBoardReceivedVar.gammaLed, parsedArgs[7].c_str());
        }
        break;
    case 1:
        if (parsedArgs.size() >= 5) {
            strcpy(TopBoardReceivedVar.Temperature, parsedArgs[0].c_str());
            strcpy(TopBoardReceivedVar.FanPWM, parsedArgs[1].c_str());
            strcpy(TopBoardReceivedVar.FanSpeed, parsedArgs[2].c_str());
            strcpy(TopBoardReceivedVar.CaseOpen, parsedArgs[3].c_str());
            strcpy(TopBoardReceivedVar.ErrorCode, parsedArgs[4].c_str());
        }
        break;
    case 10:
        if (parsedArgs.size() >= 2) {
            strcpy(TopBoardReceivedVar.uId, parsedArgs[0].c_str());
            strcpy(TopBoardReceivedVar.Version, parsedArgs[1].c_str());
        }
        break;
    }
    parsedArgs.clear();
}



std::string getComPort(const std::string& line)
{
    size_t comPos = line.find("COM");
    if (comPos != std::string::npos)
    {
        size_t startPos = line.find_first_of("0123456789", comPos);
        if (startPos != std::string::npos)
        {
            size_t endPos = line.find_first_not_of("0123456789", startPos);
            if (endPos != std::string::npos)
            {
                return line.substr(comPos, endPos - comPos);
            }
            else
            {
                return line.substr(comPos);
            }
        }
    }
    return "";
}

void ModifyPortFormat(char* port)
{
    if (port == nullptr)
        return;

    const char prefix[] = "\\\\.\\";
    const size_t prefixLength = strlen(prefix);
    const size_t portLength = strlen(port);

    // Sprawdź, czy tablica ma wystarczająco dużo miejsca dla nowego formatu
    if (portLength + prefixLength + 1 > MAX_PATH)
        return;

    // Przesuń znaki w tablicy o prefixLength miejsc w prawo
    for (size_t i = portLength; i > 0; --i)
    {
        port[i + prefixLength - 1] = port[i - 1];
    }

    // Skopiuj prefix do tablicy
    memcpy(port, prefix, prefixLength);
}

void searchComPort()
{
    system("USBDeview.exe /stab \"USBDev.txt\"");

    std::ifstream comFile;
    comFile.open("USBDev.txt");
    Sleep(500);
    if (comFile.is_open())
    {
        std::string line;
        while (std::getline(comFile, line))
        {
            if (line.find("FalconEye ControlBoard_Boot") != std::string::npos)
            {
                ConectedDevices[DEV_CONTROL_BOARD_BOOT] = 1;
                std::string comPort = getComPort(line);
                if (!comPort.empty())
                {
                    strcpy(ControlBoardPortBoot, comPort.c_str());
                    ModifyPortFormat(ControlBoardPortBoot);
                    std::cout << "FalconEye ControlBoard_Boot znaleziony na porcie: " << comPort << std::endl;
                }
            }
            else if (line.find("FalconEye TopBoard_Boot") != std::string::npos)
            {
                ConectedDevices[DEV_TOP_BOARD_BOOT] = 1;
                std::string comPort = getComPort(line);
                if (!comPort.empty())
                {
                    strcpy(TopBoardPortBoot, comPort.c_str());
                    ModifyPortFormat(TopBoardPortBoot);
                    std::cout << "FalconEye TopBoard_Boot znaleziony na porcie: " << comPort << std::endl;
                }
            }
            else if (line.find("FalconEye ControlBoard") != std::string::npos)
            {
                ConectedDevices[DEV_CONTROL_BOARD] = 1;
                std::string comPort = getComPort(line);
                if (!comPort.empty())
                {
                    strcpy(ControlBoardPort, comPort.c_str());
                    ModifyPortFormat(ControlBoardPort);
                    std::cout << "FalconEye ControlBoard znaleziony na porcie: " << comPort << std::endl;
                }
            }
            else if (line.find("FalconEye TopBoard") != std::string::npos)
            {
                ConectedDevices[DEV_TOP_BOARD] = 1;
                std::string comPort = getComPort(line);
                if (!comPort.empty())
                {
                    strcpy(TopBoardPort, comPort.c_str());
                    ModifyPortFormat(TopBoardPort);
                    std::cout << "FalconEye TopBoard znaleziony na porcie: " << comPort << std::endl;
                }
            }
        }
    }
}

void OpenPorts(void)
{
    //Terminate thread
    readThreadTerminate = true;

    if(readThreadControlBoard.joinable()) readThreadControlBoard.join();
    if(readThreadTopBoard.joinable()) readThreadTopBoard.join();

    if (PortIsOpen[DEV_CONTROL_BOARD])
    {
        CloseHandle(hControlBoard);
        PortIsOpen[DEV_CONTROL_BOARD] = 0;
    }
    if (PortIsOpen[DEV_TOP_BOARD])
    {
        CloseHandle(hTopBoard);
        PortIsOpen[DEV_TOP_BOARD] = 0;
    }


    for(int i=0 ; i<DEV_QUANITY; i++)
    {
        ConectedDevices[i] = 0;
    }


    searchComPort();

    if(ConectedDevices[DEV_CONTROL_BOARD]) hControlBoard = CreateFile(ControlBoardPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hControlBoard == INVALID_HANDLE_VALUE && ConectedDevices[DEV_CONTROL_BOARD]) {
        std::cout << "Nie można otworzyć portu COM." << std::endl;
        ConectedDevices[DEV_CONTROL_BOARD] = 0;
        return;
    }
    else PortIsOpen[DEV_CONTROL_BOARD] = 1;

    if(ConectedDevices[DEV_TOP_BOARD]) hTopBoard = CreateFile(TopBoardPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hTopBoard == INVALID_HANDLE_VALUE && ConectedDevices[DEV_TOP_BOARD]) {
        std::cout << "Nie można otworzyć portu COM." << std::endl;
        ConectedDevices[DEV_TOP_BOARD] = 0;
        return;
    }
    else PortIsOpen[DEV_TOP_BOARD] = 1;

    if(ConectedDevices[DEV_CONTROL_BOARD]) ConfigPort(hControlBoard);
    if(ConectedDevices[DEV_TOP_BOARD]) ConfigPort(hTopBoard);

    //Run Thread

    readThreadTerminate = false;



    if(ConectedDevices[DEV_CONTROL_BOARD])
    {
        readThreadControlBoard = std::thread(ReadPort, hControlBoard, ParserControlBoard);
    }
    if(ConectedDevices[DEV_TOP_BOARD])
    {
        readThreadTopBoard = std::thread(ReadPort, hTopBoard, ParserTopBoard);
    }
}

void CloseReads(void)
{
        //Terminate thread
    readThreadTerminate = true;



    if(readThreadControlBoard.joinable()) readThreadControlBoard.join();
    if(readThreadTopBoard.joinable()) readThreadTopBoard.join();

    if (PortIsOpen[DEV_CONTROL_BOARD])
    {
        CloseHandle(hControlBoard);
        PortIsOpen[DEV_CONTROL_BOARD] = 0;
    }
    if (PortIsOpen[DEV_TOP_BOARD])
    {
        CloseHandle(hTopBoard);
        PortIsOpen[DEV_TOP_BOARD] = 0;
    }

    for(int i=0 ; i<DEV_QUANITY; i++)
    {
        ConectedDevices[i] = 0;
    }

    readThreadTerminate = false;

}
