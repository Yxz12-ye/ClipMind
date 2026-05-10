#include "service/TagService.hpp"

#include <stdexcept>

#include "domain/ContentKind.hpp"

TagService::TagService(TagRepository& tagRepository, ClipRepository& clipRepository)
    : m_tagRepository(tagRepository),
      m_clipRepository(clipRepository) {
}

QVector<TagFilterItem> TagService::buildFilterItems() const {
    QVector<TagFilterItem> items;

    TagFilterItem allItem;
    allItem.kind = TagFilterKind::All;
    allItem.label = QStringLiteral("全部");
    allItem.count = m_clipRepository.countClips(ClipFilter{});
    items.push_back(allItem);

    for (const ContentKind kind : {ContentKind::Text, ContentKind::Link, ContentKind::Code}) {
        TagFilterItem item;
        item.kind = TagFilterKind::BuiltinKind;
        item.label = contentKindToDisplayName(kind);
        item.contentKind = kind;
        ClipFilter filter;
        filter.kind = kind;
        item.count = m_clipRepository.countClips(filter);
        items.push_back(item);
    }

    const QVector<Tag> tags = m_tagRepository.listTags();
    for (const Tag& tag : tags) {
        TagFilterItem item;
        item.kind = TagFilterKind::UserTag;
        item.label = tag.name;
        item.color = tag.color;
        item.tagId = tag.id;
        item.description = tag.description;
        ClipFilter filter;
        filter.tagId = tag.id;
        item.count = m_clipRepository.countClips(filter);
        items.push_back(item);
    }

    return items;
}

Tag TagService::createUserTag(const TagDraft& draft) {
    const QString trimmedName = draft.name.trimmed();
    if (trimmedName.isEmpty()) {
        throw std::invalid_argument("Tag name cannot be empty");
    }

    if (m_tagRepository.findByNameInsensitive(trimmedName).has_value()) {
        throw std::invalid_argument("Tag name already exists");
    }

    TagDraft normalizedDraft = draft;
    normalizedDraft.name = trimmedName;
    return m_tagRepository.createTag(normalizedDraft);
}

QVector<Tag> TagService::listUserTags() const {
    return m_tagRepository.listTags();
}
