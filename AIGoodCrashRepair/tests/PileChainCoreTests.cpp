#include "PileChainCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using namespace pile_chain_repair;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename T, std::size_t Size>
void Put(std::array<std::byte, Size>& bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template <typename T, std::size_t Size>
T Get(const std::array<std::byte, Size>& bytes, std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

struct Fixture {
    std::array<void*, 32> entities{};
    std::array<void*, 8> sectors{};
    std::array<std::array<std::byte, 0x60>, 4> pileBytes{};
    std::array<std::array<std::byte, 0x190>, 2> managerBytes{};

    Fixture() {
        sectors[2] = managerBytes[0].data();
        sectors[3] = managerBytes[1].data();
        for (std::size_t index = 0; index < pileBytes.size(); ++index) {
            const auto id = static_cast<std::uint16_t>(10u + index);
            entities[id] = pileBytes[index].data();
            Put(pileBytes[index], kEntityTypeOffset, kPileEntityType);
            Put(pileBytes[index], kEntityRegistrationFlagsOffset,
                kPileRegisteredFlag);
        }
    }

    ChainTables Tables() {
        return {entities.data(), entities.size(), sectors.data(),
                sectors.size()};
    }

    void Good(std::uint16_t id, std::uint8_t good) {
        Put(pileBytes[id - 10u], kPileGoodTypeOffset, good);
    }

    void Links(std::uint16_t id, std::uint16_t previous,
               std::uint16_t next) {
        Put(pileBytes[id - 10u], kPilePreviousOffset, previous);
        Put(pileBytes[id - 10u], kPileNextOffset, next);
    }

    void Head(std::uint16_t sector, std::uint16_t good, std::uint16_t id) {
        Put(managerBytes[sector - 2u],
            kEcoSectorPileHeadsOffset + good * sizeof(std::uint16_t), id);
    }

    auto Snapshot() const {
        return std::make_pair(pileBytes, managerBytes);
    }
};

void TestCleanAndCorruptChains() {
    Fixture fixture;
    fixture.Good(10, 7);
    fixture.Good(11, 7);
    fixture.Head(2, 7, 10);
    fixture.Links(10, 0, 11);
    fixture.Links(11, 10, 0);
    const auto clean = AnalyzePileChains(fixture.Tables());
    Require(clean.Clean() && clean.piles == 2u, "clean chain rejected");

    fixture.Links(11, 99, 0);
    auto issue = AnalyzePileChains(fixture.Tables()).firstIssue;
    Require(issue.kind == ChainIssueKind::PreviousMismatch &&
                issue.entity == 11u && issue.expectedPrevious == 10u,
            "previous mismatch not identified");

    fixture.Links(11, 10, 10);
    issue = AnalyzePileChains(fixture.Tables()).firstIssue;
    Require(issue.kind == ChainIssueKind::Cycle,
            "cycle not identified");

    fixture.Links(10, 0, 15);
    issue = AnalyzePileChains(fixture.Tables()).firstIssue;
    Require(issue.kind == ChainIssueKind::DanglingEntity &&
                issue.entity == 15u,
            "dangling entity not identified");
}

bool RejectWrites(const void*, std::size_t, bool requireWrite) noexcept {
    return !requireWrite;
}

void TestCutRejectsInaccessibleAndStaleTargets() {
    Fixture fixture;
    fixture.Good(10, 7);
    fixture.Head(2, 7, 10);
    fixture.Links(10, 0, 15);
    const auto issue = AnalyzeFatalPileChainIssues(fixture.Tables()).firstIssue;
    const auto before = fixture.Snapshot();

    auto tables = fixture.Tables();
    tables.accessProbe = &RejectWrites;
    Require(!CutFatalPileChainIssue(tables, issue),
            "cut accepted an inaccessible write target");
    Require(fixture.Snapshot() == before,
            "rejected inaccessible cut changed memory");

    tables = fixture.Tables();
    tables.ecoSectors = nullptr;
    Require(!CutFatalPileChainIssue(tables, issue),
            "cut accepted a null sector table");
    tables = fixture.Tables();
    tables.entities = nullptr;
    Require(!CutFatalPileChainIssue(tables, issue),
            "cut accepted a null entity table");
    auto nonfatal = issue;
    nonfatal.kind = ChainIssueKind::PreviousMismatch;
    Require(!CutFatalPileChainIssue(fixture.Tables(), nonfatal),
            "cut accepted a nonfatal bookkeeping issue");
    Require(fixture.Snapshot() == before,
            "rejected malformed cut changed memory");

    fixture.Links(10, 0, 11);
    const auto staleBefore = fixture.Snapshot();
    Require(!CutFatalPileChainIssue(fixture.Tables(), issue),
            "cut accepted a stale link target");
    Require(fixture.Snapshot() == staleBefore,
            "rejected stale cut changed memory");
}

void TestMalformedTableBoundsFailClosed() {
    Fixture fixture;
    fixture.Head(2, 7, 15);
    auto tables = fixture.Tables();
    tables.entitySlots = 10u;
    auto result = AnalyzeFatalPileChainIssues(tables);
    Require(result.firstIssue.kind == ChainIssueKind::DanglingEntity,
            "out-of-range head was not rejected");
    Require(CutFatalPileChainIssue(tables, result.firstIssue),
            "out-of-range head could not be safely detached");
    Require(Get<std::uint16_t>(fixture.managerBytes[0],
                               kEcoSectorPileHeadsOffset +
                                   7u * sizeof(std::uint16_t)) == 0u,
            "out-of-range head was not detached");

    tables = fixture.Tables();
    tables.ecoSectors = nullptr;
    result = AnalyzeFatalPileChainIssues(tables);
    Require(!result.Clean(), "null table pointers were accepted");
}

void TestDynamicSectorsAndReusableVisitMarks() {
    Fixture fixture;
    fixture.sectors[3] = nullptr;
    auto tables = fixture.Tables();
    std::array<std::uint32_t, 32> visitMarks{};
    std::uint32_t generation = 0u;
    tables.visitMarks = visitMarks.data();
    tables.visitMarkCount = visitMarks.size();
    tables.visitGeneration = &generation;

    const auto noActiveFault = AnalyzeFatalPileChainIssues(tables);
    Require(noActiveFault.Clean() && noActiveFault.managers == 1u,
            "null manager was not skipped");

    fixture.sectors[3] = fixture.managerBytes[1].data();
    fixture.Head(3, 9, 15);
    const auto newSector = AnalyzeFatalPileChainIssues(tables);
    Require(newSector.firstIssue.kind == ChainIssueKind::DanglingEntity &&
                newSector.firstIssue.ecoSector == 3u,
            "sector created after the first scan was omitted");
    fixture.Head(3, 9, 0);

    fixture.Good(10, 7);
    fixture.Head(2, 7, 10);
    fixture.Links(10, 0, 0);
    Require(AnalyzeFatalPileChainIssues(tables).Clean() &&
                AnalyzeFatalPileChainIssues(tables).Clean(),
            "reuse created a false cycle on a healthy chain");
    generation = std::numeric_limits<std::uint32_t>::max();
    Require(AnalyzeFatalPileChainIssues(tables).Clean(),
            "generation rollover created a false cycle");
    fixture.Links(10, 0, 10);
    const auto cycle = AnalyzeFatalPileChainIssues(tables);
    Require(cycle.firstIssue.kind == ChainIssueKind::Cycle,
            "reused visit marks missed a cycle");
    Require(generation > 0u, "visit generation was not advanced");

    tables.visitGeneration = nullptr;
    Require(!AnalyzeFatalPileChainIssues(tables).Clean(),
            "workspace without persistent generation was accepted");
    tables.visitGeneration = &generation;
    tables.visitMarkCount = 4u;
    const auto undersized = AnalyzeFatalPileChainIssues(tables);
    Require(undersized.firstIssue.kind == ChainIssueKind::InaccessibleMemory,
            "undersized visit workspace was accepted");
}

void TestLocalFatalCutPreservesValidChains() {
    Fixture fixture;
    fixture.Good(10, 7);
    fixture.Good(11, 7);
    fixture.Head(2, 7, 10);
    fixture.Links(10, 99, 11);
    fixture.Links(11, 77, 15);

    auto fatal = AnalyzeFatalPileChainIssues(fixture.Tables());
    Require(fatal.firstIssue.kind == ChainIssueKind::DanglingEntity &&
                fatal.firstIssue.entity == 15u &&
                fatal.firstIssue.expectedPrevious == 11u,
            "fatal scan did not find dangling tail");
    Require(CutFatalPileChainIssue(fixture.Tables(), fatal.firstIssue),
            "local dangling-tail cut failed");
    Require(Get<std::uint16_t>(fixture.pileBytes[1],
                               kPileNextOffset) == 0u,
            "local cut changed the wrong link");
    Require(AnalyzeFatalPileChainIssues(fixture.Tables()).Clean(),
            "fatal chain remained after local cut");
    Require(Get<std::uint16_t>(fixture.managerBytes[0],
                               kEcoSectorPileHeadsOffset +
                                   7 * sizeof(std::uint16_t)) == 10u,
            "local cut replaced a valid chain head");

    fixture.Head(3, 32, 15);
    fatal = AnalyzeFatalPileChainIssues(fixture.Tables());
    Require(fatal.firstIssue.kind == ChainIssueKind::DanglingEntity &&
                fatal.firstIssue.expectedPrevious == 0u,
            "fatal scan did not find dangling head");
    Require(CutFatalPileChainIssue(fixture.Tables(), fatal.firstIssue),
            "local dangling-head cut failed");
    Require(Get<std::uint16_t>(fixture.managerBytes[1],
                               kEcoSectorPileHeadsOffset +
                                   32 * sizeof(std::uint16_t)) == 0u,
            "dangling head was not cleared");
}

}  // namespace

int RunPileChainCoreTests() {
    TestCleanAndCorruptChains();
    TestCutRejectsInaccessibleAndStaleTargets();
    TestMalformedTableBoundsFailClosed();
    TestDynamicSectorsAndReusableVisitMarks();
    TestLocalFatalCutPreservesValidChains();
    return 0;
}

#ifdef PILE_CHAIN_CORE_STANDALONE
int main() {
    return RunPileChainCoreTests();
}
#endif
