#include "marker/FixedMapMenuReader.h"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::uintptr_t kScrollBaseRva = 0x00E938D8u;
constexpr std::uintptr_t kEntryArrayRva = 0x00E97848u;
constexpr std::uintptr_t kEntryCountRva = 0x0109C1D8u;
// These are image RVAs, not offsets relative to the .text section. The PE
// .text section begins at RVA 0x1000.
constexpr std::uintptr_t kRenderWindowRva = 0x0008D660u;
constexpr std::uintptr_t kScrollWindowRva = 0x0008E700u;
constexpr std::uintptr_t kConstructionWindowRva = 0x001202AEu;
constexpr std::uint32_t kImageSize = 0x012BF000u;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Write32(BYTE* destination, std::uint32_t value) {
    std::memcpy(destination, &value, sizeof(value));
}

class ImageFixture final {
public:
    ImageFixture() {
        base_ = static_cast<BYTE*>(VirtualAlloc(
            nullptr, kImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base_ != nullptr, "synthetic approved image must allocate");
        const auto baseAddress = reinterpret_cast<std::uintptr_t>(base_);
        Require(baseAddress <= UINT32_MAX,
                "Win32 synthetic image address must fit an x86 operand");

        const std::array<BYTE, 20u> render{
            0x8B, 0x35, 0, 0, 0, 0, 0x03, 0xF1, 0x89, 0xB5,
            0xD0, 0xFE, 0xFF, 0xFF, 0x3B, 0x0D, 0, 0, 0, 0};
        const std::array<BYTE, 22u> scroll{
            0x8B, 0x0D, 0, 0, 0, 0, 0x8D, 0x41, 0x07, 0x3B, 0x05,
            0, 0, 0, 0, 0x0F, 0x8D, 0, 0, 0, 0, 0x41};
        const std::array<BYTE, 18u> construction{
            0xA1, 0, 0, 0, 0, 0x8D, 0x8D, 0xD0, 0xFD,
            0xFF, 0xFF, 0x89, 0x34, 0x85, 0, 0, 0, 0};
        std::memcpy(base_ + kRenderWindowRva, render.data(), render.size());
        std::memcpy(base_ + kScrollWindowRva, scroll.data(), scroll.size());
        std::memcpy(base_ + kConstructionWindowRva, construction.data(),
                    construction.size());

        const auto scrollAddress =
            static_cast<std::uint32_t>(baseAddress + kScrollBaseRva);
        const auto arrayAddress =
            static_cast<std::uint32_t>(baseAddress + kEntryArrayRva);
        const auto countAddress =
            static_cast<std::uint32_t>(baseAddress + kEntryCountRva);
        Write32(base_ + kRenderWindowRva + 2u, scrollAddress);
        Write32(base_ + kRenderWindowRva + 16u, countAddress);
        Write32(base_ + kScrollWindowRva + 2u, scrollAddress);
        Write32(base_ + kScrollWindowRva + 11u, countAddress);
        Write32(base_ + kConstructionWindowRva + 1u, countAddress);
        Write32(base_ + kConstructionWindowRva + 14u, arrayAddress);
    }

    ~ImageFixture() {
        if (base_ != nullptr) VirtualFree(base_, 0u, MEM_RELEASE);
    }

    campaign_completion::ModuleInfo Module() const {
        campaign_completion::ModuleInfo module{};
        module.name = L"S4_Main.exe";
        module.baseAddress = reinterpret_cast<std::uintptr_t>(base_);
        module.size = kImageSize;
        module.version = "2.50.1516.0";
        module.sha256 =
            "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
        return module;
    }

    BYTE* Data() const noexcept { return base_; }

private:
    BYTE* base_ = nullptr;
};

#pragma pack(push, 1)
struct X86WideString final {
    wchar_t storage[8]{};
    std::uint32_t length = 0u;
    std::uint32_t capacity = 7u;
};

struct Entry final {
    std::array<BYTE, 0x18u> prefix{};
    X86WideString name{};
};
#pragma pack(pop)

static_assert(sizeof(X86WideString) == 24u);

void SetName(Entry& entry, std::wstring_view name) {
    Require(!name.empty() && name.size() <= 7u,
            "test fixture uses inline MSVC x86 strings");
    std::memcpy(entry.name.storage, name.data(),
                name.size() * sizeof(wchar_t));
    entry.name.storage[name.size()] = L'\0';
    entry.name.length = static_cast<std::uint32_t>(name.size());
    entry.name.capacity = 7u;
}

template <std::size_t Size>
void SetHeapName(Entry& entry, std::array<wchar_t, Size>& storage,
                 std::wstring_view name) {
    Require(!name.empty() && name.size() + 1u <= storage.size(),
            "heap string fixture must fit its owned buffer");
    std::memcpy(storage.data(), name.data(), name.size() * sizeof(wchar_t));
    storage[name.size()] = L'\0';
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    Require(address <= UINT32_MAX,
            "Win32 heap-text address must fit the game pointer layout");
    std::memset(entry.name.storage, 0, sizeof(entry.name.storage));
    Write32(reinterpret_cast<BYTE*>(entry.name.storage),
            static_cast<std::uint32_t>(address));
    entry.name.length = static_cast<std::uint32_t>(name.size());
    entry.name.capacity = static_cast<std::uint32_t>(storage.size() - 1u);
}

struct MenuFixture final {
    std::uint32_t scroll = 0u;
    std::uint32_t count = 8u;
    std::uint32_t alias = 0x12345678u;
    std::array<std::uint32_t, 1000u> pointers{};
    std::array<Entry, 8u> entries{};
    std::array<wchar_t, 32u> heapName{};

    MenuFixture() {
        constexpr std::array<std::wstring_view, 8u> names{
            L"Aeneas", L"Atlas", L"Borea", L"Circe",
            L"Dido", L"Eos", L"Fides", L"Gaia"};
        for (std::size_t index = 0u; index < entries.size(); ++index) {
            SetName(entries[index], names[index]);
            const auto address = reinterpret_cast<std::uintptr_t>(&entries[index]);
            Require(address <= UINT32_MAX,
                    "Win32 entry address must fit the game pointer layout");
            pointers[index] = static_cast<std::uint32_t>(address);
        }
        SetHeapName(entries[6], heapName, L"Antares Prime");
    }

    campaign_completion::FixedMapMenuMemoryView View() const noexcept {
        return {&scroll, &count, &alias, pointers.data(), 0x12345678u, true};
    }
};

}  // namespace

int RunFixedMapMenuReaderTests() {
    using namespace campaign_completion;

    {
        ImageFixture image;
        const auto admitted = AdmitFixedMapMenuMemory(image.Module());
        Require(admitted.admitted && admitted.entries != nullptr &&
                    admitted.expectedArrayAddress ==
                        static_cast<std::uint32_t>(
                            image.Module().baseAddress + kEntryArrayRva),
                "exact approved image and relocated operands are admitted");

        image.Data()[kRenderWindowRva] ^= 0x01u;
        Require(!AdmitFixedMapMenuMemory(image.Module()).admitted,
                "an instruction opcode mutation fails closed");
        image.Data()[kRenderWindowRva] ^= 0x01u;
        image.Data()[kScrollWindowRva + 2u] ^= 0x01u;
        Require(!AdmitFixedMapMenuMemory(image.Module()).admitted,
                "a relocated operand mutation fails closed");
    }

    MenuFixture menu;
    auto snapshot = ReadFixedMapMenuSnapshot(menu.View());
    Require(snapshot.status == FixedMapMenuReadStatus::Success &&
                snapshot.entryCount == 8u && snapshot.scrollBase == 0u &&
                snapshot.rowCount == 6u &&
                snapshot.labels[0].view() == L"Aeneas" &&
                snapshot.labels[5].view() == L"Eos",
            "initial snapshot copies the first six labels without hovering");

    const auto initial = snapshot;
    menu.scroll = 2u;
    snapshot = ReadFixedMapMenuSnapshot(menu.View());
    Require(snapshot.status == FixedMapMenuReadStatus::Success &&
                snapshot.rowCount == 6u &&
                snapshot.labels[0].view() == L"Borea" &&
                snapshot.labels[4].view() == L"Antares Prime" &&
                snapshot.labels[5].view() == L"Gaia" &&
                !EqualFixedMapMenuSnapshot(initial, snapshot),
            "scroll base deterministically remaps all six visible slots");

    menu.count = 0u;
    menu.scroll = 0u;
    snapshot = ReadFixedMapMenuSnapshot(menu.View());
    Require(snapshot.status == FixedMapMenuReadStatus::Success &&
                snapshot.rowCount == 0u,
            "an empty admitted menu produces an empty successful snapshot");

    menu.count = 8u;
    menu.alias = 0x87654321u;
    snapshot = ReadFixedMapMenuSnapshot(menu.View());
    Require(snapshot.status == FixedMapMenuReadStatus::AliasMismatch &&
                snapshot.rowCount == 0u,
            "array alias mismatch rejects the complete snapshot");
    menu.alias = 0x12345678u;

    menu.count = 1001u;
    Require(ReadFixedMapMenuSnapshot(menu.View()).status ==
                FixedMapMenuReadStatus::CountOutOfRange,
            "entry counts above the reviewed capacity fail closed");
    menu.count = 8u;
    menu.scroll = 9u;
    Require(ReadFixedMapMenuSnapshot(menu.View()).status ==
                FixedMapMenuReadStatus::ScrollOutOfRange,
            "scroll bases beyond the entry count fail closed");
    menu.scroll = 0u;

    const auto savedPointer = menu.pointers[0];
    menu.pointers[0] = 0u;
    Require(ReadFixedMapMenuSnapshot(menu.View()).status ==
                FixedMapMenuReadStatus::LabelInvalid,
            "a missing row entry rejects the entire snapshot");
    menu.pointers[0] = savedPointer;

    const auto savedLength = menu.entries[0].name.length;
    menu.entries[0].name.length = 0u;
    snapshot = ReadFixedMapMenuSnapshot(menu.View());
    Require(snapshot.status == FixedMapMenuReadStatus::LabelInvalid &&
                snapshot.rowCount == 0u && snapshot.entryCount == 0u,
            "malformed text fails without publishing partial labels");
    menu.entries[0].name.length = savedLength;

    auto unreadable = menu.View();
    unreadable.entryCount = reinterpret_cast<const std::uint32_t*>(1u);
    Require(ReadFixedMapMenuSnapshot(unreadable).status ==
                FixedMapMenuReadStatus::HeaderUnreadable,
            "an unreadable header is rejected through the guarded read path");

    menu.scroll = 0u;
    const auto repeated = ReadFixedMapMenuSnapshot(menu.View());
    Require(EqualFixedMapMenuSnapshot(initial, repeated),
            "equal bounded snapshots compare by status and copied content");
    return 0;
}
