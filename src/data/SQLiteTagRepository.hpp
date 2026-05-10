#pragma once

#include "data/SQLiteDatabase.hpp"
#include "data/TagRepository.hpp"

class SQLiteTagRepository final : public TagRepository {
public:
    explicit SQLiteTagRepository(SQLiteDatabase& database);

    QVector<Tag> listTags() const override;
    std::optional<Tag> findById(TagId id) const override;
    std::optional<Tag> findByNameInsensitive(const QString& name) const override;
    Tag createTag(const TagDraft& draft) override;
    Tag updateTag(const Tag& tag) override;
    void deleteTag(TagId id) override;

private:
    SQLiteDatabase& m_database;
};
