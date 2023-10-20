#include <winsock2.h>
#include <windows.h>
#include "myconsole.cpp"
#include "protocol.cpp"

#pragma comment(lib, "ws2_32.lib")
// #define DEBUG

const int BUF_SIZE = 4096;
const char INET_ADDR[] = "127.0.0.1";
const int PORT = 12345;
wchar_t nickname[32];
static uint32_t activeChannel = 0;

typedef struct {
    SOCKET clientSock;
    wchar_t nickname[32];
    uint32_t userID;
} ThreadParams;

DWORD WINAPI ReceiveMessages(LPVOID lpParam);
void SendMessageToServer(ThreadParams params);

ThreadParams PackThreadParams(SOCKET clientSock, wchar_t nickname[32], uint32_t userID){
    ThreadParams params;
    params.clientSock = clientSock;
    wcscpy(params.nickname, nickname);
    params.userID = userID;
    return params;
}

int main() {
    HANDLE hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

    win_printf(hConsoleOut, L"OrzChat client is starting...\n");

    // init winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != NO_ERROR) {
        win_printf(hConsoleOut, L"WSAStartup failed with error code: %d\n", result);
        return 1;
    }

    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSock == INVALID_SOCKET) {
        win_printf(hConsoleOut, L"socket failed with error code: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in servAddr;
    ZeroMemory(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(INET_ADDR);
    servAddr.sin_port = htons(PORT);

    if (connect(clientSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
        win_printf(hConsoleOut, L"connect failed with error code: %ld\n", WSAGetLastError());
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    // Clear the console at the start
    system("CLS");

    // Get nickname from user and get user ID from server
    COORD coord;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsoleOut, &csbi);
    coord.X = 0;
    coord.Y = 0;
    SetConsoleCursorPosition(hConsoleOut, coord);

    // Read nickname from user
    win_printf(hConsoleOut, L"Enter your nickname: ");
    win_scanf(hConsoleIn, L"%31ls", &nickname);

    // Send nickname to server
    uint32_t totalSize;
    char* buffer = PackLogin(nickname, totalSize);
    send(clientSock, buffer, totalSize, 0);

    // Waiting for server's response
    uint32_t userId;
    char recvBuffer[BUF_SIZE];
    int recvLen = recv(clientSock, recvBuffer, BUF_SIZE, 0);
    recvBuffer[recvLen] = '\0';

    win_printf(hConsoleOut, L"Waiting for server's response\n");
    // Unpack the response
    MessageHeader* header = reinterpret_cast<MessageHeader*>(recvBuffer);
    if (header->type == MessageType::LOGIN_SUCCESS) {
        LoginSuccessPayload* payload = reinterpret_cast<LoginSuccessPayload*>(recvBuffer + sizeof(MessageHeader));
        userId = payload->user_id;
        win_printf(hConsoleOut, L"Your ID is %d\n", payload->user_id);

#ifdef DEBUG
        win_printf(hConsoleOut, L"Received: ");
        for (int i = 0; i < recvLen; i++) {
            win_printf(hConsoleOut, L"%02x ", static_cast<unsigned char>(recvBuffer[i]));
        }
        win_printf(hConsoleOut, L"\n");
#endif

        // print out the channel list
        win_printf(hConsoleOut, L"Channel list:\n");
        uint32_t* channelIds = reinterpret_cast<uint32_t*>(recvBuffer + sizeof(MessageHeader) + sizeof(LoginSuccessPayload));
        for (int i = 0; i < payload->channel_amount; i++) {
            win_printf(hConsoleOut, L"  - Channel %d\n", channelIds[i]);
        }

    } else if (header->type == MessageType::ERR) {
        ErrorPayload* payload = reinterpret_cast<ErrorPayload*>(recvBuffer + sizeof(MessageHeader));
        win_printf(hConsoleOut, L"Error code: %d\n", payload->err_code);
        // win_printf(hConsoleOut, L"Error message: %S\n", payload->err_msg);
        closesocket(clientSock);
        WSACleanup();
        return 1;
    } else {
        win_printf(hConsoleOut, L"Message type not supported\n");
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    // Clear the console to main chat screen
    // system("CLS");

    // Start a thread to receive messages from the server
    ThreadParams params = PackThreadParams(clientSock, nickname, userId);
    DWORD dwThreadId;
    HANDLE hThread = CreateThread(NULL, 0, ReceiveMessages, (LPVOID)&params, 0, &dwThreadId);
    if (hThread == NULL) {
        fprintf(stderr, "Error creating thread: %d\n", GetLastError());
        // Handle error
        return 1;
    }

    // Send messages to the server
    SendMessageToServer(params);

    // Kill the thread
    TerminateThread(hThread, 0);
    CloseHandle(hThread);

    closesocket(clientSock);
    WSACleanup();

    return 0;
}

DWORD WINAPI ReceiveMessages(LPVOID lpParam) {
    ThreadParams* params = (ThreadParams*)lpParam;
    SOCKET clientSock = params->clientSock;
    wchar_t nickname[32];
    wcscpy(nickname, params->nickname);
    uint32_t userId = params->userID;

    char buffer[BUF_SIZE];
    int recvLen;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD coordBottom;
    coordBottom.X = 0;
    coordBottom.Y = csbi.srWindow.Bottom;

    while ((recvLen = recv(clientSock, buffer, BUF_SIZE - 1, 0)) > 0) {
        buffer[recvLen] = '\0';
        
        // Clean the last line
        SetConsoleCursorPosition(hConsole, coordBottom);
        win_printf(hConsole, L"%*s", csbi.dwSize.X, L"");
        SetConsoleCursorPosition(hConsole, coordBottom);

#ifdef DEBUG
        // Print the received message
        win_printf(hConsole, L"Received: ");
        for (int i = 0; i < recvLen; i++) {
            win_printf(hConsole, L"%02x ", static_cast<unsigned char>(buffer[i]));
        }
#endif

        // unpack the message
        MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
        if (header->type == MessageType::NEW_MSG) {
            NewMsgPayload* payload = reinterpret_cast<NewMsgPayload*>(buffer + sizeof(MessageHeader));
            // get the message
            wchar_t* message = reinterpret_cast<wchar_t*>(buffer + sizeof(MessageHeader) + sizeof(NewMsgPayload));
            win_printf(hConsole, L"%ls (%d) @ Channel %u > ", payload->nickname, payload->user_id, payload->channel_id);
            win_printf(hConsole, L"%ls", message);
            win_printf(hConsole, L"\n");
        } else if (header->type == MessageType::ERR) {
            ErrorPayload* payload = reinterpret_cast<ErrorPayload*>(buffer + sizeof(MessageHeader));
            win_printf(hConsole, L"Error code: %d\n", payload->err_code);
            // win_printf(hConsole, L"Error message: %S\n", payload->err_msg);
        } else if (header->type == MessageType::JOIN_CHANNEL_SUCCESS) {
            JoinChannelSuccessPayload* payload = reinterpret_cast<JoinChannelSuccessPayload*>(buffer + sizeof(MessageHeader));
            win_printf(hConsole, L" * Joined channel %u, type /switch %u to switch ur channel.\n", payload->channel_id, payload->channel_id);
        } else if (header->type == MessageType::LEAVE_CHANNEL_SUCCESS) {
            LeaveChannelSuccessPayload* payload = reinterpret_cast<LeaveChannelSuccessPayload*>(buffer + sizeof(MessageHeader));
            win_printf(hConsole, L" * Left channel %u, type /switch <channelID> to switch ur channel.\n", payload->channel_id);
        } else {
            win_printf(hConsole, L"Message type not supported\n");
        }
        win_printf(hConsole, L"%ls (%d) @ Channel %u > ", nickname, userId, activeChannel);
    }

    return 0;
}

void SendMessageToServer(ThreadParams params) {
    HANDLE hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsoleOut, &csbi);
    COORD coordBottom;
    coordBottom.X = 0;
    coordBottom.Y = csbi.srWindow.Bottom;
    COORD coordTop;
    coordTop.X = 0;
    coordTop.Y = 0;

    while (true) {
        // Move cursor to the input position
        SetConsoleCursorPosition(hConsoleOut, coordBottom);
        wchar_t message[1024] = {0};
        win_printf(hConsoleOut, L"%ls (%d) @ Channel %u > ", params.nickname, params.userID, activeChannel);
        win_scanf(hConsoleIn, L"%1023[^\n]", &message);
        
        // win_printf(hConsoleOut, L"%d\n", wcslen(message));
        // eat empty message
        if (wcslen(message) == 1) {
            continue;
        }

        // warn if message is too long
        if (wcslen(message) > 1023) {
            win_printf(hConsoleOut, L"Message too long, truncated to 1023 characters\n");
            message[1023] = L'\0';
        }

        // command starts with /
        if (message[0] == L'/') {
            if (wcsncmp(message, L"/quit", 5) == 0) {
                // send quit message to server
                uint32_t totalSize;
                char* buffer = PackDisconnect(params.userID, totalSize);
                send(params.clientSock, buffer, totalSize, 0);
                delete[] buffer;
                win_printf(hConsoleOut, L"Bye!\n");
                system("CLS");
                break;
            } else if (wcsncmp(message, L"/join ", 6) == 0) {
                int64_t channelID = wcstoul(message + 6, nullptr, 10);
                if (channelID > 0) {
                    uint32_t totalSize;
                    char* buffer = PackJoinChannel(params.userID, (uint32_t)channelID, totalSize);
                    send(params.clientSock, buffer, totalSize, 0);
                    delete[] buffer;
                } else {
                    win_printf(hConsoleOut, L"Invalid channel ID\n");
                }
            } else if (wcsncmp(message, L"/leave ", 7) == 0) {
                int64_t channelID = wcstoul(message + 7, nullptr, 10);
                if (channelID > 0) {
                    uint32_t totalSize;
                    char* buffer = PackLeaveChannel(params.userID, (uint32_t)channelID, totalSize);
                    send(params.clientSock, buffer, totalSize, 0);
                    delete[] buffer;
                } else {
                    win_printf(hConsoleOut, L"Invalid channel ID\n");
                }
            } else if (wcsncmp(message, L"/switch ", 8) == 0) {
                int64_t channelID = wcstoul(message + 8, nullptr, 10);
                if (channelID >= 0) {
                    activeChannel = (uint32_t)channelID;
                    win_printf(hConsoleOut, L" * Switched to channel %u\n", activeChannel);
                } else {
                    win_printf(hConsoleOut, L"Invalid channel ID\n");
                }
            } else if (wcsncmp(message, L"/help", 5) == 0 || wcsncmp(message, L"/?", 2) == 0) {
                win_printf(hConsoleOut, L" * Commands:\n");
                win_printf(hConsoleOut, L"   /join <channel_id>: join a channel\n");
                win_printf(hConsoleOut, L"   /leave <channel_id>: leave a channel\n");
                win_printf(hConsoleOut, L"   /switch <channel_id>: switch to another channel\n");
                win_printf(hConsoleOut, L"   /quit: quit the program\n");
                win_printf(hConsoleOut, L"   /help or /?: show this help message\n");
                continue;
            } else {
                win_printf(hConsoleOut, L"Unknown command: %ls\n", message);
                continue;
            }
        } else {
            // normal message
            uint32_t totalSize;
            char* buffer = PackSendMsg(params.userID, activeChannel, nickname, message, totalSize);

#ifdef DEBUG
            // preview the buffer
            win_printf(hConsoleOut, L"Send: ");
            for (int i = 0; i < totalSize; i++) {
                win_printf(hConsoleOut, L"%02x ", static_cast<unsigned char>(buffer[i]));
            }
            win_printf(hConsoleOut, L"\n");
#endif

            int result = send(params.clientSock, buffer, totalSize, 0);
            if (result == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAECONNRESET) {
                    win_printf(hConsoleOut, L"Server is down\n");
                } else {
                    win_printf(hConsoleOut, L"send failed with error code: %d\n", WSAGetLastError());
                }
                break;
            }
        }
    }
}
