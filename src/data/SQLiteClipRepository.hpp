#pragma once

#include "data/ClipRepository.hpp"
#include "data/SQLiteDatabase.hpp"

class SQLiteClipRepository final : public ClipRepository {
public:
    explicit SQLiteClipRepository(SQLiteDatabase& database);

    QVector<ClipRecord> listClips(const ClipFilter& filter) const override;
    int countClips(const ClipFilter& filter) const override;
    ClipRecord createClip(const ClipDraft& draft) override;
    void attachTag(ClipId clipId, TagId tagId) override;
    void detachTag(ClipId clipId, TagId tagId) override;

private:
    SQLiteDatabase& m_database;

    QVector<TagId> listTagIdsForClip(ClipId clipId) const;
    ClipRecord findClipById(ClipId clipId) const;
};
