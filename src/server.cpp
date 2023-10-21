#include <vector>
#include <map>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include "myconsole.cpp"
#include "protocol.cpp"

#pragma comment(lib, "ws2_32.lib")
// #define DEBUG

const int BUF_SIZE = 4096;
const char INET_ADDR[] = "127.0.0.1";
const int PORT = 12345;
static uint32_t userID = 0;

uint32_t GetUserID() {
    return userID++;
}

std::map<uint32_t, SOCKET> userSockets;
std::map<uint32_t, wchar_t[32]> nicknames;
std::map<uint32_t, std::vector<uint32_t>> channelMembers;

std::vector<uint32_t> channelIds = {1024};
SOCKET serverSock;
static BOOL running = TRUE;

DWORD WINAPI ClientHandler(LPVOID lpParam);

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    switch (CEvent)
    {
    case CTRL_C_EVENT:
        // Cleanup
        running = FALSE;
        for (auto& pair : userSockets) {
            closesocket(pair.second);
        }
        closesocket(serverSock);
        WSACleanup();
        printf("[ INFO ] Resources cleaned up, exiting...\n");
        exit(0);
        break;
    }

    return TRUE;
}

int main() {
    HANDLE hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

    win_printf(hConsoleOut, L"[ INFO ] OrzChat server is starting...\n");

    // init winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != NO_ERROR) {
        win_printf(hConsoleOut, L"[ ERROR ] WSAStartup failed with error code: %d\n", result);
        return 1;
    }

    serverSock = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSock == INVALID_SOCKET) {
        win_printf(hConsoleOut, L"[ ERROR ] socket failed with error code: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in servAddr;
    ZeroMemory(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(PORT);
    if (bind(serverSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
        win_printf(hConsoleOut, L"[ ERROR ] bind failed with error code: %ld\n", WSAGetLastError());
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    result = listen(serverSock, 5);
    if (result == SOCKET_ERROR) {
        win_printf(hConsoleOut, L"[ ERROR ] listen failed with error code: %ld\n", WSAGetLastError());
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE)) {
        win_printf(hConsoleOut, L"[ ERROR ] Unable to install handler!\n");
        return 1;
    }

    // Clear the console at the start
    system("CLS");

    // Wait for clients to connect
    while (running) {
        win_printf(hConsoleOut, L"[ INFO ] Waiting for clients to connect...\n");
        sockaddr_in clientAddr;
        int clntAddrSize = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (SOCKADDR*)&clientAddr, &clntAddrSize);

        if (clientSock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) {
                // Server is shutting down
                win_printf(hConsoleOut, L"[ INFO ] Server is shutting down\n");
                break;
            } else {
                win_printf(hConsoleOut, L"[ WARNING ] accept failed with error code: %ld\n", err);
                continue;
            }
        } else {
            wchar_t* clientIP = ConvertCharToWChar(inet_ntoa(clientAddr.sin_addr));
            win_printf(hConsoleOut, L"[ INFO ] Client connected: %s:%d\n", clientIP, ntohs(clientAddr.sin_port));
            delete[] clientIP;
        }

        // Create a thread to handle the client
        DWORD dwThreadId;
        HANDLE hThread = CreateThread(NULL, 0, ClientHandler, (LPVOID)clientSock, 0, &dwThreadId);
        CloseHandle(hThread);
    }

    // Cleanup
    for (auto& pair : userSockets) {
        closesocket(pair.second);
    }
    closesocket(serverSock);
    WSACleanup();
    printf("[ INFO ] Resources cleaned up, exiting...\n");
    return 0;
}

DWORD WINAPI ClientHandler(LPVOID lpParam) {
    HANDLE hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    SOCKET clientSock = (SOCKET)lpParam;
    char raw_buffer[BUF_SIZE];
    char buffer[BUF_SIZE];
    int recvLen;

    // First wait until client sends login message
    recvLen = recv(clientSock, raw_buffer, BUF_SIZE, 0);

    if (recvLen > 0) {
        // Upack the message to get nickname
        MessageHeader* header = (MessageHeader*)raw_buffer;
        LoginPayload* payload = (LoginPayload*)(raw_buffer + sizeof(MessageHeader));

        if (header->type != MessageType::LOGIN) {
            // Error
            win_printf(hConsoleOut, L"[ ERROR ] Client sent invalid login message\n");
            // send error message
            uint32_t totalSize;
            char* buf = PackError(1, totalSize);
            send(clientSock, buf, totalSize, 0);
            delete[] buf;
            closesocket(clientSock);
            return 0;
        }

        win_printf(hConsoleOut, L"[ INFO ] Client logged in with nickname: %ls\n", payload->nickname);
        uint32_t userID = GetUserID();
        wcscpy(nicknames[userID], payload->nickname);
        userSockets[userID] = clientSock;

        // Send login success message
        uint32_t totalSize;
        char* buf = PackLoginSuccess(userID, channelIds.size(), channelIds.data(), totalSize);

#ifdef DEBUG
        win_printf(hConsoleOut, L"[ INFO ] Send: ");
        for (int i = 0; i < totalSize; i++) {
            win_printf(hConsoleOut, L"%02x ", static_cast<unsigned char>(buffer[i]));
        }
        win_printf(hConsoleOut, L"\n");
#endif

        send(clientSock, buf, totalSize, 0);
        delete[] buf;

    } else if (recvLen == 0) {
        // Client disconnected
        win_printf(hConsoleOut, L"[ INFO ] Client disconnected\n");
        closesocket(clientSock);
        return 0;
    } else {
        // Error occurred
        win_printf(hConsoleOut, L"[ ERROR ] recv failed with error code: %ld\n", WSAGetLastError());
        closesocket(clientSock);
        return 0;
    }

    // memset buffer and raw_buffer
    memset(buffer, 0, BUF_SIZE);
    memset(raw_buffer, 0, BUF_SIZE);
    int offset = 0;

    while (running) {
        recvLen = recv(clientSock, raw_buffer + offset, BUF_SIZE - offset, 0);

        if (recvLen == 0) {
            // Client disconnected
            win_printf(hConsoleOut, L"[ INFO ] Client disconnected\n");
            break;
        } else if (recvLen < 0) {
            // Error occurred
            int err = WSAGetLastError();
            if (err == WSAECONNRESET) {
                // Client disconnected
                win_printf(hConsoleOut, L"[ INFO ] Client disconnected\n");
            } else if (err == WSAECONNABORTED) {
                // Server disconnected
                win_printf(hConsoleOut, L"[ INFO ] Server disconnected\n");
            } else {
                win_printf(hConsoleOut, L"[ ERROR ] recv failed with error code: %ld\n", err);
            }
            break;
        }

        offset += recvLen;

        while (offset >= sizeof(MessageHeader)) {
            MessageHeader* header = (MessageHeader*)raw_buffer;
            if (offset < header->payload_length + sizeof(MessageHeader)) {
                break; // Wait for the complete message to be received
            }

            // Process the message
            memcpy(buffer, raw_buffer, sizeof(MessageHeader) + header->payload_length);

#ifdef DEBUG
            win_printf(hConsoleOut, L"[ INFO ] Received: ");
            for (int i = 0; i < recvLen; i++) {
                win_printf(hConsoleOut, L"%02x ", static_cast<unsigned char>(buffer[i]));
            }
            win_printf(hConsoleOut, L"\n");
#endif

            switch (header->type) {
            case MessageType::JOIN_CHANNEL:
            {
                JoinChannelPayload* payload = reinterpret_cast<JoinChannelPayload*>(buffer + sizeof(MessageHeader));
                win_printf(hConsoleOut, L"[ INFO ] Client %d joined channel %d\n", payload->user_id, payload->channel_id);
                if (channelMembers.find(payload->channel_id) == channelMembers.end()) {
                    // Create a new channel and add the user to it
                    channelMembers[payload->channel_id] = std::vector<uint32_t>{payload->user_id};
                } else {
                    // Add the user to the channel
                    channelMembers[payload->channel_id].push_back(payload->user_id);
                }

                // Send join channel success message
                uint32_t totalSize;
                char* buf = PackJoinChannelSuccess(payload->user_id, payload->channel_id, totalSize);
                send(clientSock, buf, totalSize, 0);
                delete[] buf;
                break;
            }
            case MessageType::LEAVE_CHANNEL:
            {
                LeaveChannelPayload* payload = reinterpret_cast<LeaveChannelPayload*>(buffer + sizeof(MessageHeader));
                win_printf(hConsoleOut, L"[ INFO ] Client %d left channel %d\n", payload->user_id, payload->channel_id);
                if (channelMembers.find(payload->channel_id) != channelMembers.end()) {
                    // Remove the user from the channel
                    std::vector<uint32_t>& members = channelMembers[payload->channel_id];
                    for (int i = 0; i < members.size(); i++) {
                        if (members[i] == payload->user_id) {
                            members.erase(members.begin() + i);
                            break;
                        }
                    }
                }

                // Send leave channel success message
                uint32_t totalSize;
                char* buf = PackLeaveChannelSuccess(payload->user_id, payload->channel_id, totalSize);
                send(clientSock, buf, totalSize, 0);
                delete[] buf;
                break;
            }
            case MessageType::SEND_MSG:
            {   
                SendMsgPayload* payload = reinterpret_cast<SendMsgPayload*>(buffer + sizeof(MessageHeader));
                wchar_t* message = reinterpret_cast<wchar_t*>(buffer + sizeof(MessageHeader) + sizeof(SendMsgPayload));
                win_printf(hConsoleOut, L"[ INFO ] %ls (%d) say to channel %d: %ls\n", 
                            nicknames[payload->user_id], payload->user_id, payload->channel_id, message);

                // channel 0 is the global channel
                if (payload->channel_id == 0) {
                    // Send the message to all members in the global channel
                    for (auto& pair : userSockets) {
                        if (pair.first != payload->user_id) {
                            // Send message
                            uint32_t totalSize;
                            char* buf = PackNewMsg(payload->user_id, payload->channel_id, nicknames[payload->user_id], message, totalSize);
                            send(pair.second, buf, totalSize, 0);
                            delete[] buf;
                        }
                    }
                } else {
                    // Send the message to all members in the channel
                    for (uint32_t member : channelMembers[payload->channel_id]) {
                        if (member != payload->user_id) {
                            // Send message
                            uint32_t totalSize;
                            char* buf = PackNewMsg(payload->user_id, payload->channel_id, nicknames[payload->user_id], message, totalSize);
                            send(userSockets[member], buf, totalSize, 0);
                            delete[] buf;
                        }
                    }
                }
                break;
            }
            case MessageType::DISCONNECT:
            {
                DisconnectPayload* payload = reinterpret_cast<DisconnectPayload*>(buffer + sizeof(MessageHeader));
                win_printf(hConsoleOut, L"[ INFO ] Client %d disconnected\n", payload->user_id);
                // Remove the user from all channels
                for (auto& pair : channelMembers) {
                    std::vector<uint32_t>& members = pair.second;
                    for (int i = 0; i < members.size(); i++) {
                        if (members[i] == payload->user_id) {
                            members.erase(members.begin() + i);
                            break;
                        }
                    }
                }
                // Remove the user from the user list
                userSockets.erase(payload->user_id);
                nicknames.erase(payload->user_id);
                closesocket(clientSock);
                return 0;
            }
            default:
                win_printf(hConsoleOut, L"[ ERROR ] Message type not supported\n");
                break;
            }

            // Shift the unprocessed data to the beginning of the buffer
            offset -= header->payload_length + sizeof(MessageHeader);
            memmove(raw_buffer, raw_buffer + header->payload_length + sizeof(MessageHeader), offset);
        }
    }

    closesocket(clientSock);
    win_printf(hConsoleOut, L"[ INFO ] Client socket closed\n");
    return 0;
}
