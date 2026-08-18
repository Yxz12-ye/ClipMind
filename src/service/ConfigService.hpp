#pragma once

#include <QDir>

#include "EmbeddingService.hpp"

struct ApplicationSettings {
    bool hideAfterPaste = true;
    bool showTrayIcon = true;
    EmbeddingConfig embeddingConfig;
};

class ConfigService {
public:
    explicit ConfigService(const QDir& configDirectory);

    ApplicationSettings load() const;
    bool save(const ApplicationSettings& settings) const;

private:
    QDir configDirectory;

    QString configFilePath() const;
};
