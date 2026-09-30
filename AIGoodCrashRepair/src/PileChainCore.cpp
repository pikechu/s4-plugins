#include "PileChainCore.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace pile_chain_repair {
namespace {

std::uint8_t ReadByte(const void* base, std::size_t offset) noexcept {
    std::uint8_t value = 0u;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset,
                sizeof(value));
    return value;
}

std::uint16_t ReadWord(const void* base, std::size_t offset) noexcept {
    std::uint16_t value = 0u;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset,
                sizeof(value));
    return value;
}

void WriteWord(void* base, std::size_t offset, std::uint16_t value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value,
                sizeof(value));
}

std::size_t HeadOffset(std::uint16_t goodType) noexcept {
    return kEcoSectorPileHeadsOffset +
           static_cast<std::size_t>(goodType) * sizeof(std::uint16_t);
}

bool CanAccess(const ChainTables& tables, const void* address,
               std::size_t bytes, bool requireWrite) noexcept {
    return address != nullptr &&
           (tables.accessProbe == nullptr ||
            tables.accessProbe(address, bytes, requireWrite));
}

ChainIssue Issue(ChainIssueKind kind, std::size_t sector,
                 std::uint16_t good, std::uint16_t entity,
                 std::uint16_t expectedPrevious = 0u,
                 std::uint16_t actualPrevious = 0u) noexcept {
    ChainIssue issue{};
    issue.kind = kind;
    issue.ecoSector = static_cast<std::uint16_t>(sector);
    issue.goodType = good;
    issue.entity = entity;
    issue.expectedPrevious = expectedPrevious;
    issue.actualPrevious = actualPrevious;
    return issue;
}

}  // namespace

ChainAnalysis AnalyzePileChains(const ChainTables& tables) {
    ChainAnalysis analysis{};
    if (tables.entities == nullptr || tables.ecoSectors == nullptr ||
        tables.entitySlots < 2u || tables.ecoSectorSlots < 2u) {
        analysis.firstIssue.kind = ChainIssueKind::DanglingEntity;
        return analysis;
    }

    if ((tables.visitMarks == nullptr) != (tables.visitGeneration == nullptr)) {
        analysis.firstIssue.kind = ChainIssueKind::InaccessibleMemory;
        return analysis;
    }

    std::vector<std::uint32_t> localVisited;
    std::uint32_t localGeneration = 0u;
    if (tables.visitMarks == nullptr) {
        localVisited.resize(tables.entitySlots, 0u);
    } else if (tables.visitMarkCount < tables.entitySlots) {
        analysis.firstIssue.kind = ChainIssueKind::InaccessibleMemory;
        return analysis;
    }
    auto* const visited = tables.visitMarks != nullptr
                              ? tables.visitMarks
                              : localVisited.data();
    auto* const generation = tables.visitGeneration != nullptr
                                 ? tables.visitGeneration
                                 : &localGeneration;
    for (std::size_t sector = 1u; sector < tables.ecoSectorSlots; ++sector) {
        const auto* manager = tables.ecoSectors[sector];
        if (manager == nullptr) {
            continue;
        }
        if (!CanAccess(tables, manager, kEcoSectorBytesRequired, false)) {
            analysis.firstIssue =
                Issue(ChainIssueKind::InaccessibleMemory, sector, 0u, 0u);
            return analysis;
        }
        ++analysis.managers;
        for (std::uint16_t good = kFirstGoodType; good <= kLastGoodType;
             ++good) {
            std::uint16_t entityId = ReadWord(manager, HeadOffset(good));
            if (entityId == 0u) {
                continue;
            }
            ++analysis.chains;
            if (++(*generation) == 0u) {
                std::fill(visited, visited + tables.entitySlots, 0u);
                *generation = 1u;
            }
            std::uint16_t previous = 0u;
            while (entityId != 0u) {
                if (entityId >= tables.entitySlots ||
                    tables.entities[entityId] == nullptr) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::DanglingEntity, sector, good, entityId,
                        previous);
                    return analysis;
                }
                if (visited[entityId] == *generation) {
                    analysis.firstIssue =
                        Issue(ChainIssueKind::Cycle, sector, good, entityId,
                              previous);
                    return analysis;
                }
                visited[entityId] = *generation;

                const auto* entity = tables.entities[entityId];
                if (!CanAccess(tables, entity, kEntityBytesRequired, false)) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::InaccessibleMemory, sector, good,
                        entityId, previous);
                    return analysis;
                }
                if (ReadByte(entity, kEntityTypeOffset) != kPileEntityType) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::WrongEntityType, sector, good, entityId,
                        previous);
                    return analysis;
                }
                if ((ReadByte(entity, kEntityRegistrationFlagsOffset) &
                     kPileRegisteredFlag) == 0u) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::UnregisteredPile, sector, good, entityId,
                        previous);
                    return analysis;
                }
                if (ReadByte(entity, kPileGoodTypeOffset) !=
                    static_cast<std::uint8_t>(good)) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::WrongGoodType, sector, good, entityId,
                        previous);
                    return analysis;
                }
                const std::uint16_t actualPrevious =
                    ReadWord(entity, kPilePreviousOffset);
                if (actualPrevious != previous) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::PreviousMismatch, sector, good, entityId,
                        previous, actualPrevious);
                    return analysis;
                }

                ++analysis.piles;
                previous = entityId;
                entityId = ReadWord(entity, kPileNextOffset);
            }
        }
    }
    return analysis;
}

ChainAnalysis AnalyzeFatalPileChainIssues(const ChainTables& tables) {
    ChainAnalysis analysis{};
    if (tables.entities == nullptr || tables.ecoSectors == nullptr ||
        tables.entitySlots < 2u || tables.ecoSectorSlots < 2u) {
        analysis.firstIssue.kind = ChainIssueKind::DanglingEntity;
        return analysis;
    }

    if ((tables.visitMarks == nullptr) != (tables.visitGeneration == nullptr)) {
        analysis.firstIssue.kind = ChainIssueKind::InaccessibleMemory;
        return analysis;
    }

    std::vector<std::uint32_t> localVisited;
    std::uint32_t localGeneration = 0u;
    if (tables.visitMarks == nullptr) {
        localVisited.resize(tables.entitySlots, 0u);
    } else if (tables.visitMarkCount < tables.entitySlots) {
        analysis.firstIssue.kind = ChainIssueKind::InaccessibleMemory;
        return analysis;
    }
    auto* const visited = tables.visitMarks != nullptr
                              ? tables.visitMarks
                              : localVisited.data();
    auto* const generation = tables.visitGeneration != nullptr
                                 ? tables.visitGeneration
                                 : &localGeneration;
    for (std::size_t sector = 1u; sector < tables.ecoSectorSlots; ++sector) {
        const auto* manager = tables.ecoSectors[sector];
        if (manager == nullptr) continue;
        if (!CanAccess(tables, manager, kEcoSectorBytesRequired, false)) {
            analysis.firstIssue =
                Issue(ChainIssueKind::InaccessibleMemory, sector, 0u, 0u);
            return analysis;
        }
        ++analysis.managers;
        for (std::uint16_t good = kFirstGoodType; good <= kLastGoodType;
             ++good) {
            std::uint16_t entityId = ReadWord(manager, HeadOffset(good));
            if (entityId == 0u) continue;
            ++analysis.chains;
            if (++(*generation) == 0u) {
                std::fill(visited, visited + tables.entitySlots, 0u);
                *generation = 1u;
            }
            std::uint16_t previous = 0u;
            while (entityId != 0u) {
                if (entityId >= tables.entitySlots ||
                    tables.entities[entityId] == nullptr) {
                    analysis.firstIssue =
                        Issue(ChainIssueKind::DanglingEntity, sector, good,
                              entityId, previous);
                    return analysis;
                }
                if (visited[entityId] == *generation) {
                    analysis.firstIssue =
                        Issue(ChainIssueKind::Cycle, sector, good, entityId,
                              previous);
                    return analysis;
                }
                visited[entityId] = *generation;
                const auto* entity = tables.entities[entityId];
                if (!CanAccess(tables, entity, kEntityBytesRequired, false)) {
                    analysis.firstIssue = Issue(
                        ChainIssueKind::InaccessibleMemory, sector, good,
                        entityId, previous);
                    return analysis;
                }
                if (ReadByte(entity, kEntityTypeOffset) != kPileEntityType) {
                    analysis.firstIssue =
                        Issue(ChainIssueKind::WrongEntityType, sector, good,
                              entityId, previous);
                    return analysis;
                }
                ++analysis.piles;
                previous = entityId;
                entityId = ReadWord(entity, kPileNextOffset);
            }
        }
    }
    return analysis;
}

bool CutFatalPileChainIssue(const ChainTables& tables,
                            const ChainIssue& issue) {
    const bool fatal = issue.kind == ChainIssueKind::DanglingEntity ||
                       issue.kind == ChainIssueKind::WrongEntityType ||
                       issue.kind == ChainIssueKind::Cycle ||
                       issue.kind == ChainIssueKind::InaccessibleMemory;
    if (!fatal || tables.entities == nullptr ||
        tables.ecoSectors == nullptr || issue.entity == 0u ||
        issue.ecoSector == 0u ||
        issue.ecoSector >= tables.ecoSectorSlots ||
        issue.goodType < kFirstGoodType ||
        issue.goodType > kLastGoodType) {
        return false;
    }
    auto* manager = tables.ecoSectors[issue.ecoSector];
    if (!CanAccess(tables, manager, kEcoSectorBytesRequired, true)) {
        return false;
    }

    void* owner = manager;
    std::size_t offset = HeadOffset(issue.goodType);
    if (issue.expectedPrevious != 0u) {
        if (issue.expectedPrevious >= tables.entitySlots) return false;
        owner = tables.entities[issue.expectedPrevious];
        offset = kPileNextOffset;
        if (!CanAccess(tables, owner, kEntityBytesRequired, true) ||
            ReadByte(owner, kEntityTypeOffset) != kPileEntityType) {
            return false;
        }
    }
    if (ReadWord(owner, offset) != issue.entity) return false;
    WriteWord(owner, offset, 0u);
    return true;
}

const char* ChainIssueName(ChainIssueKind kind) noexcept {
    switch (kind) {
        case ChainIssueKind::None: return "none";
        case ChainIssueKind::DanglingEntity: return "dangling-entity";
        case ChainIssueKind::WrongEntityType: return "wrong-entity-type";
        case ChainIssueKind::UnregisteredPile: return "unregistered-pile";
        case ChainIssueKind::WrongGoodType: return "wrong-good-type";
        case ChainIssueKind::PreviousMismatch: return "previous-mismatch";
        case ChainIssueKind::Cycle: return "cycle";
        case ChainIssueKind::InaccessibleMemory:
            return "inaccessible-memory";
    }
    return "unknown";
}

}  // namespace pile_chain_repair
