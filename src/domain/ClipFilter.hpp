#pragma once

#include <optional>

#include "domain/ContentKind.hpp"
#include "domain/Tag.hpp"

struct ClipFilter {
    std::optional<ContentKind> kind;
    std::optional<TagId> tagId;
};
