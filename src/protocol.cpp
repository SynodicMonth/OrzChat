#include <stdint.h>
#include <cstring>
#include <vector>
#pragma pack(1)

// Message Header
// +------------+---------+--------------+
// | MagicNumber|  Type   | PayloadLength|
// +------------+---------+--------------+
// |  O r z C   | 2 bytes |    4 bytes   |
// +------------+---------+--------------+
// |                                     |
// |              Payload                |
// |                                     |
// +-------------------------------------+
// MagicNumber: ASCII code of 'OrzC', 0x4F727A43
// Type: 0x00 -- Login
//       0x01 -- JoinChannel
//       0x02 -- SendMsg
//       0x03 -- LeaveChannel
//       0x04 -- Disconnect
// ------------------ Server --------------------------
//       0x05 -- LoginSuccess
//       0x06 -- JoinChannelSuccess
//       0x07 -- NewMsg
//       0x08 -- LeaveChannelSuccess
//       0x09 -- Error
// PayloadLength: length of payload
// Payload: See below

typedef struct {
    uint32_t magic_number;
    uint8_t type;
    uint32_t payload_length;
} MessageHeader;

enum MessageType : uint8_t {
    LOGIN = 0x00,
    JOIN_CHANNEL = 0x01,
    SEND_MSG = 0x02,
    LEAVE_CHANNEL = 0x03,
    DISCONNECT = 0x04,
    LOGIN_SUCCESS = 0x05,
    JOIN_CHANNEL_SUCCESS = 0x06,
    NEW_MSG = 0x07,
    LEAVE_CHANNEL_SUCCESS = 0x08,
    ERR = 0x09
};

// Login Payload
// +----------------+
// |    Nickname    |
// +----------------+
// |    64 bytes    |
// +----------------+
// Client sends nickname to server

typedef struct {
    wchar_t nickname[32];
} LoginPayload;

// LoginSuccess Payload
// +----------+---------------+-----------+-----------+-------+
// |  UserID  | ChannelAmount | ChannelID | ChannelID |  ...  |
// +----------+---------------+-----------+-----------+-------+
// |  4 bytes |    4 bytes    |  4 bytes  |  4 bytes  |  ...  |
// +----------+---------------+-----------+-----------+-------+
// Server responds to login request
// UserID: assign a unique ID to user
// ChannelAmount: amount of channels available on server
// ChannelID: ID of channel

typedef struct {
    uint32_t user_id;
    uint32_t channel_amount;
} LoginSuccessPayload;

// JoinChannel Payload
// +----------+-----------+
// |  UserID  | ChannelID |
// +----------+-----------+
// |  4 bytes |  4 bytes  |
// +----------+-----------+
// Client joins a channel
// UserID: ID of user
// ChannelID: ID of channel

typedef struct {
    uint32_t user_id;
    uint32_t channel_id;
} JoinChannelPayload;

// JoinChannelSuccess Payload
// +----------+-----------+
// |  UserID  | ChannelID |
// +----------+-----------+
// |  4 bytes |  4 bytes  |
// +----------+-----------+

typedef JoinChannelPayload JoinChannelSuccessPayload;

// SendMsg Payload
// +----------+-----------+----------+-----------+----------+
// |  UserID  | ChannelID | Nickname | MsgLength |   Msg    |
// +----------+-----------+----------+-----------+----------+
// |  4 bytes |  4 bytes  | 64 bytes |  4 bytes  |  ...     |
// +----------+-----------+----------+-----------+----------+
// Client sends message to channel
// UserID: ID of user
// ChannelID: ID of channel
// Nickname: nickname of sender
// MsgLength: length of message
// Msg: message, in UTF-8 encoding

typedef struct {
    uint32_t user_id;
    uint32_t channel_id;
    wchar_t nickname[32];
    uint32_t msg_length;
} SendMsgPayload;

// NewMsg Payload
// +----------+-----------+----------+-----------+----------+
// |  UserID  | ChannelID | Nickname | MsgLength |   Msg    |
// +----------+-----------+----------+-----------+----------+
// |  4 bytes |  4 bytes  | 64 bytes |  4 bytes  |  ...     |
// +----------+-----------+----------+-----------+----------+
// Server sends message to all users in channel
// UserID: ID of sender
// ChannelID: ID of channel
// Nickname: nickname of sender
// MsgLength: length of message
// Msg: message, in UTF-8 encoding

typedef SendMsgPayload NewMsgPayload;

// LeaveChannel Payload
// +----------+-----------+
// |  UserID  | ChannelID |
// +----------+-----------+
// |  4 bytes |  4 bytes  |
// +----------+-----------+

typedef JoinChannelPayload LeaveChannelPayload;

// LeaveChannelSuccess Payload
// +----------+-----------+
// |  UserID  | ChannelID |
// +----------+-----------+
// |  4 bytes |  4 bytes  |
// +----------+-----------+

typedef JoinChannelPayload LeaveChannelSuccessPayload;

// Disconnect Payload
// +----------+
// |  UserID  |
// +----------+
// |  4 bytes |
// +----------+
// Client disconnects from server
// UserID: ID of user

typedef struct {
    uint32_t user_id;
} DisconnectPayload;

// Error Payload
// +----------+----------+
// | ErrCode  | ErrMsg   |
// +----------+----------+
// |  4 bytes |  ...     |
// +----------+----------+
// Server sends error message to client
// ErrCode: error code
// ErrMsg: error message, in UTF-8 encoding

typedef struct {
    uint32_t err_code;
} ErrorPayload;

char* PackLogin(const wchar_t* nickname, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(LoginPayload);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = LOGIN;
    header->payload_length = sizeof(LoginPayload);

    LoginPayload* payload = reinterpret_cast<LoginPayload*>(buffer + sizeof(MessageHeader));
    memcpy(payload->nickname, nickname, 32 * sizeof(wchar_t));

    return buffer;
}

char* PackSendMsg(uint32_t userId, uint32_t channelId, wchar_t nickname[32], const wchar_t* msg, uint32_t& totalPackSize) {
    uint32_t msgLength = wcslen(msg) + 1;

    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(SendMsgPayload) + msgLength * sizeof(wchar_t);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);

    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = SEND_MSG;
    header->payload_length = sizeof(SendMsgPayload) + msgLength * sizeof(wchar_t);

    SendMsgPayload* payload = reinterpret_cast<SendMsgPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_id = channelId;
    payload->msg_length = msgLength;

    wchar_t* message = reinterpret_cast<wchar_t*>(buffer + sizeof(MessageHeader) + sizeof(SendMsgPayload));
    memcpy(message, msg, msgLength * sizeof(wchar_t));

    return buffer;
}

char* PackError(uint32_t errCode, const wchar_t* errMsg, uint32_t& totalPackSize) {
    uint32_t errMsgLength = wcslen(errMsg);

    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(ErrorPayload) + errMsgLength * sizeof(wchar_t);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = ERR;
    header->payload_length = sizeof(ErrorPayload) + errMsgLength * sizeof(wchar_t);

    ErrorPayload* payload = reinterpret_cast<ErrorPayload*>(buffer + sizeof(MessageHeader));
    payload->err_code = errCode;

    wchar_t* message = reinterpret_cast<wchar_t*>(buffer + sizeof(MessageHeader) + sizeof(ErrorPayload));
    memcpy(message, errMsg, errMsgLength * sizeof(wchar_t));

    return buffer;
}

char* PackLoginSuccess(uint32_t userId, uint32_t channelAmount, uint32_t* channelIds, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(LoginSuccessPayload) + channelAmount * sizeof(uint32_t);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = LOGIN_SUCCESS;
    header->payload_length = sizeof(LoginSuccessPayload) + channelAmount * sizeof(uint32_t);

    LoginSuccessPayload* payload = reinterpret_cast<LoginSuccessPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_amount = channelAmount;
    uint32_t* channelsBuffer = reinterpret_cast<uint32_t*>(buffer + sizeof(MessageHeader) + sizeof(LoginSuccessPayload));
    for (uint32_t i = 0; i < channelAmount; i++) {
        channelsBuffer[i] = channelIds[i];
    }

    return buffer;
}

char* PackJoinChannel(uint32_t userId, uint32_t channelId, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(JoinChannelPayload);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = JOIN_CHANNEL;
    header->payload_length = sizeof(JoinChannelPayload);

    JoinChannelPayload* payload = reinterpret_cast<JoinChannelPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_id = channelId;

    return buffer;
}

char* PackJoinChannelSuccess(uint32_t userId, uint32_t channelId, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(JoinChannelSuccessPayload);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = JOIN_CHANNEL_SUCCESS;
    header->payload_length = sizeof(JoinChannelSuccessPayload);

    JoinChannelSuccessPayload* payload = reinterpret_cast<JoinChannelSuccessPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_id = channelId;

    return buffer;
}

char* PackLeaveChannel(uint32_t userId, uint32_t channelId, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(LeaveChannelPayload);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = LEAVE_CHANNEL;
    header->payload_length = sizeof(LeaveChannelPayload);

    LeaveChannelPayload* payload = reinterpret_cast<LeaveChannelPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_id = channelId;

    return buffer;
}

char* PackLeaveChannelSuccess(uint32_t userId, uint32_t channelId, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(LeaveChannelSuccessPayload);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43;
    header->type = LEAVE_CHANNEL_SUCCESS;
    header->payload_length = sizeof(LeaveChannelSuccessPayload);

    LeaveChannelSuccessPayload* payload = reinterpret_cast<LeaveChannelSuccessPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_id = channelId;

    return buffer;
}

char* PackNewMsg(uint32_t userId, uint32_t channelId, wchar_t nickname[32], const wchar_t* msg, uint32_t& totalPackSize) {
    uint32_t msgLength = wcslen(msg) + 1;

    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(NewMsgPayload) + msgLength * sizeof(wchar_t);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);

    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = NEW_MSG;
    header->payload_length = sizeof(NewMsgPayload) + msgLength * sizeof(wchar_t);

    NewMsgPayload* payload = reinterpret_cast<NewMsgPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;
    payload->channel_id = channelId;
    wcscpy(payload->nickname, nickname);
    payload->msg_length = msgLength;

    wchar_t* message = reinterpret_cast<wchar_t*>(buffer + sizeof(MessageHeader) + sizeof(NewMsgPayload));
    memcpy(message, msg, msgLength * sizeof(wchar_t));

    return buffer;
}

char* PackDisconnect(uint32_t userId, uint32_t& totalPackSize) {
    // Calculate total size of the message (header + payload)
    totalPackSize = sizeof(MessageHeader) + sizeof(DisconnectPayload);

    char* buffer = new char[totalPackSize];
    memset(buffer, 0, totalPackSize);

    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
    header->magic_number = 0x4F727A43; // ASCII for 'OrzC'
    header->type = DISCONNECT;
    header->payload_length = sizeof(DisconnectPayload);

    DisconnectPayload* payload = reinterpret_cast<DisconnectPayload*>(buffer + sizeof(MessageHeader));
    payload->user_id = userId;

    return buffer;
}

wchar_t* ConvertCharToWChar(const char* c) {
    // Get the length needed for the wchar buffer
    int cSize = MultiByteToWideChar(CP_UTF8, 0, c, -1, nullptr, 0);
    if(cSize == 0) {
        // Handle error, perhaps with GetLastError()
        return nullptr;
    }
    
    // Allocate memory for wchar buffer.
    wchar_t* wc = new wchar_t[cSize];

    // Do the conversion.
    int convResult = MultiByteToWideChar(CP_UTF8, 0, c, -1, wc, cSize);
    if(convResult == 0) {
        delete[] wc;
        // Handle error, perhaps with GetLastError()
        return nullptr;
    }
    
    return wc;
}