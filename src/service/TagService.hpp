#pragma once

#include <optional>

#include "data/ClipRepository.hpp"
#include "data/TagRepository.hpp"

enum class TagFilterKind {
    All,
    BuiltinKind,
    UserTag,
};

struct TagFilterItem {
    TagFilterKind kind = TagFilterKind::All;
    QString label;
    QString color;
    int count = 0;
    std::optional<ContentKind> contentKind;
    std::optional<TagId> tagId;
    QString description;
};

class TagService {
public:
    TagService(TagRepository& tagRepository, ClipRepository& clipRepository);

    QVector<TagFilterItem> buildFilterItems() const;
    Tag createUserTag(const TagDraft& draft);
    QVector<Tag> listUserTags() const;

private:
    TagRepository& m_tagRepository;
    ClipRepository& m_clipRepository;
};
