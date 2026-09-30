#pragma once

// Core pile-chain validation and local repair primitives.

#include <cstddef>
#include <cstdint>

namespace pile_chain_repair {

using MemoryAccessProbe =
    bool (*)(const void* address, std::size_t bytes,
             bool requireWrite) noexcept;

inline constexpr std::size_t kEntityTypeOffset = 0x0Au;
inline constexpr std::size_t kEntityRegistrationFlagsOffset = 0x14u;
inline constexpr std::size_t kEntityXOffset = 0x18u;
inline constexpr std::size_t kEntityYOffset = 0x1Au;
inline constexpr std::size_t kPilePreviousOffset = 0x28u;
inline constexpr std::size_t kPileNextOffset = 0x2Au;
inline constexpr std::size_t kPileGoodTypeOffset = 0x40u;
inline constexpr std::size_t kEcoSectorPileHeadsOffset = 0x130u;
inline constexpr std::uint8_t kPileEntityType = 0x10u;
inline constexpr std::uint8_t kPileRegisteredFlag = 0x40u;
inline constexpr std::uint16_t kFirstGoodType = 1u;
inline constexpr std::uint16_t kLastGoodType = 42u;
inline constexpr std::size_t kEntityBytesRequired = 0x41u;
inline constexpr std::size_t kEcoSectorBytesRequired =
    kEcoSectorPileHeadsOffset +
    (static_cast<std::size_t>(kLastGoodType) + 1u) * sizeof(std::uint16_t);

struct ChainTables final {
    void* const* entities = nullptr;
    std::size_t entitySlots = 0u;
    void* const* ecoSectors = nullptr;
    std::size_t ecoSectorSlots = 0u;
    std::uint32_t* visitMarks = nullptr;
    std::size_t visitMarkCount = 0u;
    std::uint32_t* visitGeneration = nullptr;
    MemoryAccessProbe accessProbe = nullptr;
};

enum class ChainIssueKind {
    None,
    DanglingEntity,
    WrongEntityType,
    UnregisteredPile,
    WrongGoodType,
    PreviousMismatch,
    Cycle,
    InaccessibleMemory,
};

struct ChainIssue final {
    ChainIssueKind kind = ChainIssueKind::None;
    std::uint16_t ecoSector = 0u;
    std::uint16_t goodType = 0u;
    std::uint16_t entity = 0u;
    std::uint16_t expectedPrevious = 0u;
    std::uint16_t actualPrevious = 0u;

    explicit operator bool() const noexcept {
        return kind != ChainIssueKind::None;
    }
};

struct ChainAnalysis final {
    ChainIssue firstIssue{};
    std::size_t managers = 0u;
    std::size_t chains = 0u;
    std::size_t piles = 0u;

    bool Clean() const noexcept { return !firstIssue; }
};

ChainAnalysis AnalyzePileChains(const ChainTables& tables);
ChainAnalysis AnalyzeFatalPileChainIssues(const ChainTables& tables);
bool CutFatalPileChainIssue(const ChainTables& tables,
                            const ChainIssue& issue);
const char* ChainIssueName(ChainIssueKind kind) noexcept;

}  // namespace pile_chain_repair
