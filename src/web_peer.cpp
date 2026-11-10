#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include "web_peer.h"

#include <cstddef>
#include <vector>

namespace web_http {
namespace {

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
};

bool ReadUser(HANDLE process, std::vector<unsigned char>& user)
{
    Handle token;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token.value)) return false;
    DWORD length = 0;
    if (GetTokenInformation(token.value, TokenUser, nullptr, 0, &length) || GetLastError() != ERROR_INSUFFICIENT_BUFFER) return false;
    if (!length || length > 65536) return false;
    user.resize(length);
    return GetTokenInformation(token.value, TokenUser, user.data(), length, &length) != FALSE;
}

bool SameUser(DWORD pid)
{
    if (!pid) return false;
    Handle process;
    process.value = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process.value) return false;
    std::vector<unsigned char> current, peer;
    if (!ReadUser(GetCurrentProcess(), current) || !ReadUser(process.value, peer)) return false;
    const auto currentUser = reinterpret_cast<const TOKEN_USER*>(current.data());
    const auto peerUser = reinterpret_cast<const TOKEN_USER*>(peer.data());
    return IsValidSid(currentUser->User.Sid) && IsValidSid(peerUser->User.Sid) && EqualSid(currentUser->User.Sid, peerUser->User.Sid);
}

} // namespace

bool IsSameUserPeer(SOCKET acceptedSocket)
{
    sockaddr_in local = {}, remote = {};
    int localLength = sizeof(local), remoteLength = sizeof(remote);
    if (getsockname(acceptedSocket, reinterpret_cast<sockaddr*>(&local), &localLength) != 0
        || getpeername(acceptedSocket, reinterpret_cast<sockaddr*>(&remote), &remoteLength) != 0
        || local.sin_family != AF_INET || remote.sin_family != AF_INET
        || local.sin_addr.s_addr != htonl(INADDR_LOOPBACK) || remote.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) return false;
    DWORD length = 0;
    if (GetExtendedTcpTable(nullptr, &length, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != ERROR_INSUFFICIENT_BUFFER) return false;
    std::vector<unsigned char> storage;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (length < offsetof(MIB_TCPTABLE_OWNER_PID, table) || length > 4 * 1024 * 1024) return false;
        storage.resize(length);
        DWORD status = GetExtendedTcpTable(storage.data(), &length, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        if (status == ERROR_INSUFFICIENT_BUFFER) continue;
        if (status != NO_ERROR) return false;
        const auto table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(storage.data());
        const auto available = (storage.size() - offsetof(MIB_TCPTABLE_OWNER_PID, table)) / sizeof(MIB_TCPROW_OWNER_PID);
        if (table->dwNumEntries > available) return false;
        for (DWORD i = 0; i < table->dwNumEntries; i++) {
            const auto& row = table->table[i];
            // Match the client side of this exact connection, not our server row.
            if (row.dwLocalAddr == remote.sin_addr.s_addr && row.dwRemoteAddr == local.sin_addr.s_addr
                && static_cast<unsigned short>(row.dwLocalPort) == remote.sin_port
                && static_cast<unsigned short>(row.dwRemotePort) == local.sin_port) return SameUser(row.dwOwningPid);
        }
        return false;
    }
    return false;
}

} // namespace web_http
