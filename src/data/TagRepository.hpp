#pragma once

#include <optional>

#include <QVector>

#include "domain/Tag.hpp"

class TagRepository {
public:
    virtual ~TagRepository() = default;

    virtual QVector<Tag> listTags() const = 0;
    virtual std::optional<Tag> findById(TagId id) const = 0;
    virtual std::optional<Tag> findByNameInsensitive(const QString& name) const = 0;
    virtual Tag createTag(const TagDraft& draft) = 0;
    virtual Tag updateTag(const Tag& tag) = 0;
    virtual void deleteTag(TagId id) = 0;
};
