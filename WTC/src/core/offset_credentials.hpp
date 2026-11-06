#pragma once
#include <Windows.h>
#include <wincred.h>
#include <string>
#include <string_view>
#include <utility>
#pragma comment(lib, "Advapi32.lib")

namespace offset_credentials {
// A Windows Credential Manager secret, scoped to the signed-in user on this PC.
// This target is intentionally unrelated to the public/open-source project.
class store {
    std::wstring target_;
public:
    explicit store(std::wstring target=L"WTC.Private.Offsets.API.v1") : target_(std::move(target)) {}
    bool save(std::string_view token) const {
        if(token.empty() || token.size()>512) return false;
        for(unsigned char c:token) if(c<33 || c>126) return false;
        CREDENTIALW credential{};
        credential.Type=CRED_TYPE_GENERIC;
        credential.TargetName=const_cast<wchar_t*>(target_.c_str());
        credential.CredentialBlobSize=static_cast<DWORD>(token.size());
        credential.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
        credential.Persist=CRED_PERSIST_LOCAL_MACHINE;
        return CredWriteW(&credential,0)!=FALSE;
    }
    template<class Callback> bool with_secret(Callback&& callback) const {
        PCREDENTIALW credential=nullptr;
        if(!CredReadW(target_.c_str(),CRED_TYPE_GENERIC,0,&credential)) return false;
        struct cleanup {
            PCREDENTIALW value;
            ~cleanup() {
                if(value) {
                    if(value->CredentialBlob && value->CredentialBlobSize)
                        SecureZeroMemory(value->CredentialBlob,value->CredentialBlobSize);
                    CredFree(value);
                }
            }
        } guard{credential};
        if(!credential->CredentialBlob || credential->CredentialBlobSize==0 ||
           credential->CredentialBlobSize>512) return false;
        callback(std::string_view(reinterpret_cast<const char*>(credential->CredentialBlob),
                                  credential->CredentialBlobSize));
        return true;
    }
    bool exists() const {return with_secret([](std::string_view){});}
    bool erase() const {return CredDeleteW(target_.c_str(),CRED_TYPE_GENERIC,0)!=FALSE;}
};
}
