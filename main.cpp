#include <iostream>
#include <string>
#include <thread>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <handleapi.h>

#define CONTROL_BOARD "control-"
#define ENDLINE '^'

char ControlBoardPort[25];

uint8_t ArgumentQuanityControlBoard[] = {11, 8, 0, 0, 0, 0, 0, 0, 0, 0, 2};

std::string TerminalData;


void ReadPort(HANDLE hSerial, void(*ptrParser)(char *));
void WritePort(HANDLE hSerial, std::string message);
bool ConfigPort(HANDLE hSerial);
void WriteTxt(void);
void ParserControlBoard(char *buffer);
void searchComPort(void);
void OpenPorts(void);

bool threadTerminate;

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

    // Otwarcie portu COM
    HANDLE hControlBoard;

    void removeSubstring(std::string& str, const std::string& substr) {
    size_t pos = str.find(substr);
    if (pos != std::string::npos) {
        str.erase(pos, substr.length());
    }
}

int main() {
    searchComPort();
   // sprintf(ControlBoardPort, "\\\\.\\COM7");

     hControlBoard = CreateFile(ControlBoardPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hControlBoard == INVALID_HANDLE_VALUE) {
        std::cout << "Nie można otworzyć portu COM." << std::endl;
        return 1;
    }

    ConfigPort(hControlBoard);

    // Uruchomienie wątku do odczytu z portu COM
   // std::thread readThread(ReadPort, hControlBoard);

   std::thread readThreadControlBoard([](HANDLE handle, void(*func)(char*)) {
    ReadPort(handle, func);
}, hControlBoard, ParserControlBoard);


    // Uruchomienie wątku do zapisu do portu COM
    //std::thread writeThread(WritePort, hControlBoard);

    // Uruchomienie watku do zapisu zmiennych do pliku
    std::thread writeTxtThread(WriteTxt);

    while(true)
    {
        getline(std::cin, TerminalData);
        if(!TerminalData.compare("exit"))
        {
            std::cout << "exit" << std::endl;
            threadTerminate = TRUE;
            break;
        }
        TerminalData += ENDLINE;
        if (TerminalData.find(CONTROL_BOARD) != std::string::npos)
        {
            removeSubstring(TerminalData, CONTROL_BOARD);
            std::cout << TerminalData << std::endl;
            WritePort(hControlBoard, TerminalData);
            TerminalData.clear();
        }
        else if (TerminalData.find("screen") != std::string::npos)
        {
            removeSubstring(TerminalData, "screen");
            std::cout << TerminalData << std::endl;
        }

        std::cin.clear();


    }

    // Oczekiwanie na wciśnięcie klawisza Enter
   // std::cin.ignore();

    // Zakończenie wątków
    readThreadControlBoard.join();
    //writeThread.join();
    writeTxtThread.join();

    // Zamknięcie portu COM
    CloseHandle(hControlBoard);

    return 0;
}

void ReadPort(HANDLE hSerial, void(*ptrParser)(char*)) {
    std::string receivedMessage;

    while (true) {
        char buffer[512];
        DWORD bytesRead;

        if (ReadFile(hSerial, buffer, sizeof(buffer), &bytesRead, NULL)) {
            if (bytesRead > 0) {

                for (DWORD i = 0; i < bytesRead; i++) {
                    char currentChar = buffer[i];

                    if (currentChar == ENDLINE) {
                        // Znaleziono koniec wiadomości, przetwarzanie odczytanej wiadomości
                        //std::cout << "Odczytano: " << receivedMessage << std::endl;
                        char Msg[receivedMessage.size() + 1];
                        strcpy(Msg, receivedMessage.c_str());

                                ptrParser(Msg);


                        receivedMessage.clear();
                    } else {
                        // Dodanie znaku do odczytanej wiadomości
                        receivedMessage += currentChar;
                    }
                }
            }
        } else {
            std::cout << "Błąd podczas odczytu z portu COM." << std::endl;
            OpenPorts();
           // break;
        }
        if(threadTerminate) break;
    }
}

void WritePort(HANDLE hSerial, std::string message) {
    //std::string message = "2/3^";
   // while (true) {
        DWORD bytesWritten;
        if (!WriteFile(hSerial, message.c_str(), message.length(), &bytesWritten, NULL)) {
            std::cout << "Błąd podczas zapisu do portu COM." << std::endl;
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
        std::cout << "Błąd podczas pobierania ustawień portu COM." << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cout << "Błąd podczas konfigurowania ustawień portu COM." << std::endl;
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

        std::ofstream OutFile;
        std::ostringstream ControlBoardStream;
                  ControlBoardStream << "----ControlBoard " << ControlBoardReceivedVar.uId << "----" << std::endl
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
                    << "Error code: " << ControlBoardReceivedVar.ErrorCode;


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

void ParserControlBoard(char *buffer)
{
    char * ParsePointer = strtok(buffer, "/");
    uint8_t CommandID = atoi(ParsePointer);
    char tmpCommandArgument[20][30]; //20 command arg 30 char

    for(uint8_t i = 0; i < ArgumentQuanityControlBoard[CommandID]; i++)
    {
        char * ParsePointer = strtok(NULL, "/");
        sprintf(&tmpCommandArgument[i][0], "%s", ParsePointer);
    }
    switch(CommandID)
    {
    case 0:
        strcpy(ControlBoardReceivedVar.Input, &tmpCommandArgument[0][0]);
        strcpy(ControlBoardReceivedVar.Output, &tmpCommandArgument[1][0]);
        strcpy(ControlBoardReceivedVar.Pwm1, &tmpCommandArgument[2][0]);
        strcpy(ControlBoardReceivedVar.Pwm2, &tmpCommandArgument[3][0]);
        strcpy(ControlBoardReceivedVar.Pwm3, &tmpCommandArgument[4][0]);
        strcpy(ControlBoardReceivedVar.Pwm4, &tmpCommandArgument[5][0]);
        strcpy(ControlBoardReceivedVar.Temp, &tmpCommandArgument[6][0]);
        strcpy(ControlBoardReceivedVar.McuTemp, &tmpCommandArgument[7][0]);
        strcpy(ControlBoardReceivedVar.Volt12, &tmpCommandArgument[8][0]);
        strcpy(ControlBoardReceivedVar.Volt5, &tmpCommandArgument[9][0]);
        strcpy(ControlBoardReceivedVar.Curr, &tmpCommandArgument[10][0]);

        break;
    case 1:
        strcpy(ControlBoardReceivedVar.LightMode, &tmpCommandArgument[0][0]);
        strcpy(ControlBoardReceivedVar.LightPwm, &tmpCommandArgument[1][0]);
        strcpy(ControlBoardReceivedVar.LightDimm, &tmpCommandArgument[2][0]);
        strcpy(ControlBoardReceivedVar.LogoMode, &tmpCommandArgument[3][0]);
        strcpy(ControlBoardReceivedVar.LogoPwm, &tmpCommandArgument[4][0]);
        strcpy(ControlBoardReceivedVar.LogoDimm, &tmpCommandArgument[5][0]);
        strcpy(ControlBoardReceivedVar.ErrorCode, &tmpCommandArgument[6][0]);
        strcpy(ControlBoardReceivedVar.DoorOpen, &tmpCommandArgument[7][0]);
        break;
    case 10:
        strcpy(ControlBoardReceivedVar.uId, &tmpCommandArgument[0][0]);
        strcpy(ControlBoardReceivedVar.Version, &tmpCommandArgument[1][0]);
        break;
    }
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
            if (line.find("FalconEye ControlBoard") != std::string::npos)
            {
                std::string comPort = getComPort(line);
                if (!comPort.empty())
                {
                    strcpy(ControlBoardPort, comPort.c_str());
                    ModifyPortFormat(ControlBoardPort);
                    std::cout << "FalconEye ControlBoard znaleziony na porcie: " << comPort << std::endl;
                    // Kontynuuj przetwarzanie pozostałych informacji
                }
            }
            else if (line.find("GSM_Thermometer") != std::string::npos)
            {
                std::string comPort = getComPort(line);
                if (!comPort.empty())
                {
                    std::cout << "GSM_Thermometer znaleziony na porcie: " << comPort << std::endl;
                    // Kontynuuj przetwarzanie pozostałych informacji
                }
            }
        }
    }
}

void OpenPorts(void)
{
    CloseHandle(hControlBoard);
    Sleep(10000);
        searchComPort();

    HANDLE hControlBoard = CreateFile(ControlBoardPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hControlBoard == INVALID_HANDLE_VALUE) {
        std::cout << "Nie można otworzyć portu COM." << std::endl;
        return;
    }

    ConfigPort(hControlBoard);
}
