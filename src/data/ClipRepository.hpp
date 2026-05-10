#pragma once

#include "domain/ClipFilter.hpp"
#include "domain/ClipRecord.hpp"

class ClipRepository {
public:
    virtual ~ClipRepository() = default;

    virtual QVector<ClipRecord> listClips(const ClipFilter& filter) const = 0;
    virtual int countClips(const ClipFilter& filter) const = 0;
    virtual ClipRecord createClip(const ClipDraft& draft) = 0;
    virtual void attachTag(ClipId clipId, TagId tagId) = 0;
    virtual void detachTag(ClipId clipId, TagId tagId) = 0;
};
