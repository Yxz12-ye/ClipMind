#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <memory>
#include <string>

namespace {

class Statement final {
public:
    Statement(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            stmt_ = nullptr;
        }
    }

    ~Statement() {
        if (stmt_ != nullptr) {
            sqlite3_finalize(stmt_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    explicit operator bool() const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

std::string sqliteError(sqlite3* db) {
    return db == nullptr ? "unknown sqlite error" : sqlite3_errmsg(db);
}

QString runChecks() {
    QStringList lines;

    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    spdlog::info("Qt Widgets demo started");
    lines << "spdlog: OK";
    lines << "  wrote startup log via spdlog::info()";

    sqlite3* rawDb = nullptr;
    const int openRc = sqlite3_open(":memory:", &rawDb);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, &sqlite3_close);

    if (openRc != SQLITE_OK || db == nullptr) {
        const QString error = QString::fromStdString(sqliteError(rawDb));
        spdlog::error("sqlite3_open failed: {}", error.toStdString());
        lines << "sqlite: FAILED";
        lines << QString("  sqlite3_open(:memory:) failed: %1").arg(error);
        return lines.join('\n');
    }

    lines << "sqlite: OK";
    lines << "  opened in-memory database";

    char* execError = nullptr;
    const char* setupSql =
        "CREATE TABLE demo ("
        "    id INTEGER PRIMARY KEY,"
        "    name TEXT NOT NULL"
        ");"
        "INSERT INTO demo(name) VALUES ('ClipMind');"
        "INSERT INTO demo(name) VALUES ('Qt Widgets');";

    const int execRc = sqlite3_exec(db.get(), setupSql, nullptr, nullptr, &execError);
    if (execRc != SQLITE_OK) {
        const QString error =
            execError != nullptr ? QString::fromUtf8(execError) : QString::fromStdString(sqliteError(db.get()));
        if (execError != nullptr) {
            sqlite3_free(execError);
        }
        spdlog::error("sqlite setup failed: {}", error.toStdString());
        lines << "sqlite query: FAILED";
        lines << QString("  setup SQL failed: %1").arg(error);
        return lines.join('\n');
    }

    Statement versionStmt(db.get(), "SELECT sqlite_version()");
    if (!versionStmt) {
        const QString error = QString::fromStdString(sqliteError(db.get()));
        spdlog::error("prepare version query failed: {}", error.toStdString());
        lines << "sqlite query: FAILED";
        lines << QString("  prepare version query failed: %1").arg(error);
        return lines.join('\n');
    }

    if (sqlite3_step(versionStmt.get()) == SQLITE_ROW) {
        const auto* version = reinterpret_cast<const char*>(sqlite3_column_text(versionStmt.get(), 0));
        lines << QString("  sqlite_version(): %1").arg(version != nullptr ? version : "null");
    }

    Statement dataStmt(db.get(), "SELECT id, name FROM demo ORDER BY id");
    if (!dataStmt) {
        const QString error = QString::fromStdString(sqliteError(db.get()));
        spdlog::error("prepare data query failed: {}", error.toStdString());
        lines << "sqlite query: FAILED";
        lines << QString("  prepare data query failed: %1").arg(error);
        return lines.join('\n');
    }

    lines << "sqlite rows:";
    while (sqlite3_step(dataStmt.get()) == SQLITE_ROW) {
        const int id = sqlite3_column_int(dataStmt.get(), 0);
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(dataStmt.get(), 1));
        lines << QString("  %1 -> %2").arg(id).arg(name != nullptr ? name : "null");
    }

    spdlog::info("sqlite check completed successfully");
    return lines.join('\n');
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    auto window = std::make_unique<QMainWindow>();
    window->setWindowTitle("ClipMind Dependency Check");
    window->resize(640, 420);

    auto* central = new QWidget(window.get());
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel("Qt Widgets / spdlog / SQLite smoke test", central);
    auto* output = new QPlainTextEdit(central);
    auto* rerunButton = new QPushButton("Re-run checks", central);

    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);

    layout->addWidget(title);
    layout->addWidget(output, 1);
    layout->addWidget(rerunButton);

    QObject::connect(rerunButton, &QPushButton::clicked, output, [output]() {
        output->setPlainText(runChecks());
    });

    output->setPlainText(runChecks());

    window->setCentralWidget(central);
    window->show();

    return app.exec();
}
