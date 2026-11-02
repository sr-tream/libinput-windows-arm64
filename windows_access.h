#pragma once
#include <windows.h>
#include <sddl.h>
#include <string>
#include <vector>

inline bool processIsElevated() {
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return false;
    TOKEN_ELEVATION elevation{}; DWORD size=0;
    bool ok=GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size)!=FALSE;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated!=0;
}

// Allow this user's ordinary Stop.cmd to signal an elevated worker. Only the
// stop event has a medium integrity label; input injection remains elevated.
inline HANDLE createUserStopEvent(const wchar_t* name) {
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return nullptr;
    DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<BYTE> data(size);
    bool ok=size && GetTokenInformation(token,TokenUser,data.data(),size,&size);
    CloseHandle(token); if(!ok) return nullptr;
    LPWSTR sid=nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&sid)) return nullptr;
    std::wstring sddl=L"D:P(A;;GA;;;SY)(A;;GA;;;"+std::wstring(sid)+L")S:(ML;;NW;;;ME)";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)) return nullptr;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
    HANDLE event=CreateEventW(&attributes,TRUE,FALSE,name);
    LocalFree(descriptor); return event;
}
