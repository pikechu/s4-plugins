#pragma once

#include "completion/CompletionRecord.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace campaign_completion {

inline constexpr std::size_t kMaximumCompletionJsonBytes =
    4u * 1024u * 1024u;
inline constexpr std::size_t kMaximumCompletionRecords = 8192u;

struct CompletionDatabaseSnapshot final {
    std::vector<CompletionRecord> records;
};

enum class CompletionJsonFailure {
    None,
    Oversized,
    Syntax,
    Utf8,
    DuplicateKey,
    UnknownField,
    MissingField,
    WrongType,
    Schema,
    FieldLimit,
    InvalidRecord,
    DuplicateStableId,
};

struct CompletionJsonResult final {
    CompletionDatabaseSnapshot snapshot;
    CompletionJsonFailure failure = CompletionJsonFailure::None;

    explicit operator bool() const noexcept {
        return failure == CompletionJsonFailure::None;
    }
};

std::optional<std::string> EncodeCompletionJson(
    CompletionDatabaseSnapshot snapshot) noexcept;
CompletionJsonResult DecodeCompletionJson(std::string_view bytes) noexcept;

}  // namespace campaign_completion
