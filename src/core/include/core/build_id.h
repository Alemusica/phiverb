#pragma once

#include <string>
#include <string_view>

#ifndef WAYVERB_BUILD_GIT_DESC
#define WAYVERB_BUILD_GIT_DESC "unknown"
#endif

#ifndef WAYVERB_BUILD_GIT_BRANCH
#define WAYVERB_BUILD_GIT_BRANCH "unknown"
#endif

#ifndef WAYVERB_BUILD_TIMESTAMP
#define WAYVERB_BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

namespace wayverb {
namespace core {

inline std::string_view build_commit() {
    return WAYVERB_BUILD_GIT_DESC;
}

inline std::string_view build_branch() {
    return WAYVERB_BUILD_GIT_BRANCH;
}

inline std::string_view build_timestamp() {
    return WAYVERB_BUILD_TIMESTAMP;
}

inline std::string build_identifier(std::string_view version = {}) {
    auto commit = std::string(build_commit());
    auto branch = std::string(build_branch());
    auto timestamp = std::string(build_timestamp());

    auto normalize = [](std::string value) {
        for (auto& ch : value) {
            if (ch == '\n' || ch == '\r') {
                ch = ' ';
            }
        }
        return value;
    };

    commit = normalize(std::move(commit));
    branch = normalize(std::move(branch));
    timestamp = normalize(std::move(timestamp));

    std::string result;
    auto append_chunk = [&](const std::string& chunk) {
        if (chunk.empty()) {
            return;
        }
        if (!result.empty()) {
            result.append(" • ");
        }
        result.append(chunk);
    };

    if (!version.empty()) {
        append_chunk(std::string(version));
    }

    append_chunk("commit " + (commit.empty() ? std::string{"unknown"}
                                             : commit));

    if (!branch.empty() && branch != "unknown") {
        append_chunk("branch " + branch);
    }

    append_chunk("built " +
                 (timestamp.empty() ? std::string{"unknown"} : timestamp));

    return result;
}

}  // namespace core
}  // namespace wayverb

