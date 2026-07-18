#include "diagnostics/ModuleInventory.h"

#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace campaign_completion {
namespace {

constexpr char kApprovedVersion[] = "2.50.1516.0";
constexpr char kApprovedSha256[] =
    "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";

std::wstring Lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool NeedsDetails(const std::wstring& name) {
    const auto lower = Lowercase(name);
    if (lower == L"s4_main.exe" || lower == L"s4modapi.dll") {
        return true;
    }
    return lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".asi") == 0;
}

}  // namespace

std::optional<std::string> Sha256File(const std::filesystem::path& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    const bool initialized =
        CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT) != FALSE &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE;
    if (!initialized) {
        if (provider != 0) {
            CryptReleaseContext(provider, 0);
        }
        CloseHandle(file);
        return std::nullopt;
    }

    std::array<BYTE, 64 * 1024> buffer{};
    bool success = true;
    for (;;) {
        DWORD bytesRead = 0;
        if (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                     &bytesRead, nullptr) == FALSE) {
            success = false;
            break;
        }
        if (bytesRead == 0) {
            break;
        }
        if (CryptHashData(hash, buffer.data(), bytesRead, 0) == FALSE) {
            success = false;
            break;
        }
    }

    std::array<BYTE, 32> digest{};
    DWORD digestSize = static_cast<DWORD>(digest.size());
    if (!success ||
        CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digestSize, 0) == FALSE) {
        success = false;
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    CloseHandle(file);
    if (!success || digestSize != static_cast<DWORD>(digest.size())) {
        return std::nullopt;
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

std::optional<std::string> Sha256FileRange(
    const std::filesystem::path& path, std::uint64_t offset,
    std::size_t length) {
    if (length == 0u) return std::nullopt;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE) {
        CloseHandle(file);
        return std::nullopt;
    }
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    bool success =
        CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT) != FALSE &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE;
    std::array<BYTE, 64 * 1024> buffer{};
    std::size_t remaining = length;
    while (success && remaining != 0u) {
        const DWORD requested = static_cast<DWORD>(
            (std::min)(remaining, buffer.size()));
        DWORD read = 0u;
        if (ReadFile(file, buffer.data(), requested, &read, nullptr) == FALSE ||
            read != requested ||
            CryptHashData(hash, buffer.data(), read, 0) == FALSE) {
            success = false;
            break;
        }
        remaining -= read;
    }
    std::array<BYTE, 32u> digest{};
    DWORD digestSize = static_cast<DWORD>(digest.size());
    if (!success ||
        CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digestSize, 0) ==
            FALSE) {
        success = false;
    }
    if (hash != 0) CryptDestroyHash(hash);
    if (provider != 0) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    if (!success || digestSize != digest.size()) return std::nullopt;
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        output << std::setw(2) << static_cast<unsigned>(byte);
    }
    return output.str();
}

std::string FileVersion(const std::filesystem::path& path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) {
        return {};
    }

    std::vector<BYTE> data(size);
    if (GetFileVersionInfoW(path.c_str(), 0, size, data.data()) == FALSE) {
        return {};
    }

    void* value = nullptr;
    UINT infoSize = 0;
    if (VerQueryValueW(data.data(), L"\\", &value, &infoSize) == FALSE ||
        value == nullptr ||
        infoSize < static_cast<UINT>(sizeof(VS_FIXEDFILEINFO))) {
        return {};
    }
    const auto* info = static_cast<const VS_FIXEDFILEINFO*>(value);

    std::ostringstream output;
    output << HIWORD(info->dwFileVersionMS) << '.'
           << LOWORD(info->dwFileVersionMS) << '.'
           << HIWORD(info->dwFileVersionLS) << '.'
           << LOWORD(info->dwFileVersionLS);
    return output.str();
}

std::vector<ModuleInfo> EnumerateLoadedModules() {
    std::vector<ModuleInfo> modules;
    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        return modules;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = static_cast<DWORD>(sizeof(entry));
    if (Module32FirstW(snapshot, &entry) != FALSE) {
        do {
            ModuleInfo module;
            module.name = entry.szModule;
            module.path = entry.szExePath;
            module.baseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            module.size = entry.modBaseSize;
            if (NeedsDetails(module.name)) {
                module.version = FileVersion(module.path);
                const auto hash = Sha256File(module.path);
                if (hash.has_value()) {
                    module.sha256 = *hash;
                }
            }
            modules.push_back(std::move(module));
        } while (Module32NextW(snapshot, &entry) != FALSE);
    }
    CloseHandle(snapshot);

    std::sort(modules.begin(), modules.end(), [](const auto& left, const auto& right) {
        return Lowercase(left.path.wstring()) < Lowercase(right.path.wstring());
    });
    return modules;
}

CompatibilityResult CheckTargetExecutable(const ModuleInfo& executable) {
    if (executable.version != kApprovedVersion) {
        return CompatibilityResult::VersionMismatch;
    }
    if (executable.sha256 != kApprovedSha256) {
        return CompatibilityResult::HashMismatch;
    }
    return CompatibilityResult::Compatible;
}

}  // namespace campaign_completion
