//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "yaraeditor.h"

#include <expected>

#include <yara.h>

#include <QFileDialog>

#include "formatter/formatter.h"
#include "utils/log.h"
#include "utils/yaraerror.h"
#include "yarahighlighter.h"

class YaraInstance
{
public:
    YaraInstance()
        : initialized_(false)
    {
        int ret = yr_initialize();
        if (ret != ERROR_SUCCESS) {
            LOG_CRITICAL("Failed to initialize Yara instance (code: {})", ret);
            return;
        }

        initialized_ = true;
    }
    ~YaraInstance()
    {
        if (initialized_) {
            yr_finalize();
            initialized_ = false;
        }
    }

private:
    bool initialized_;
};

struct YrRulesDeleter
{
    void operator()(YR_RULES *r) const
    {
        if (r)
            yr_rules_destroy(r);
    }
};

using YrRulesPtr = std::unique_ptr<YR_RULES, YrRulesDeleter>;

class YaraRules
{
public:
    YaraRules() = default;

    YaraRules(const YaraRules &) = delete;
    YaraRules &operator=(const YaraRules &) = delete;
    YaraRules(YaraRules &&) = default;
    YaraRules &operator=(YaraRules &&) = default;

    // Returns a pointer-to-pointer suitable for yr_compiler_get_rules().
    // std::out_ptr resets the unique_ptr with the value written by the callee.
    auto addressOf() { return std::out_ptr(rules_); }

    YR_RULES *get() const { return rules_.get(); }

private:
    YrRulesPtr rules_;
};

void yara_compiler_callback(
    int error_level,
    const char *file_name,
    int line_number,
    const YR_RULE *rule,
    const char *message,
    void *user_data);

class YaraCompiler
{
public:
    struct Error
    {
        int32_t errorLevel_ = 0;
        std::optional<std::string> fileName_;
        int32_t lineNumber_ = 0;
        // YR_RULE* rule;
        std::string message_;
    };

    class YaraCallbackResult
    {
    public:
        std::optional<Error> yaraError_;
    };

    YaraCompiler(const YaraCompiler &) = delete;
    YaraCompiler &operator=(const YaraCompiler &) = delete;
    YaraCompiler(YaraCompiler &&) = delete;
    YaraCompiler &operator=(YaraCompiler &&) = delete;

    // Returns nullptr on failure; check ec.
    static std::unique_ptr<YaraCompiler> create(std::error_code &ec)
    {
        std::unique_ptr<YR_COMPILER, CompilerDeleter> raw;
        if (auto error = yr_compiler_create(std::out_ptr(raw)); error != ERROR_SUCCESS) {
            ec = YaraError::make_error_code(error);
            return nullptr;
        }
        return std::unique_ptr<YaraCompiler>(new YaraCompiler(std::move(raw)));
    }

    [[nodiscard]] std::expected<int, std::error_code> AddRule(QString rule)
    {
        return yr_compiler_add_string(compiler_.get(), rule.toUtf8().data(), nullptr);
    }

    std::expected<YaraRules, std::error_code> Compile()
    {
        YaraRules rules;
        if (auto error = yr_compiler_get_rules(compiler_.get(), rules.addressOf());
            error != ERROR_SUCCESS) {
            return std::unexpected(YaraError::make_error_code(error));
        }

        return rules;
    }

    std::optional<YaraCompiler::Error> LastError()
    {
        if (!callbackResult_.yaraError_.has_value()) {
            return {};
        }

        return callbackResult_.yaraError_.value();
    }

private:
    struct CompilerDeleter
    {
        void operator()(YR_COMPILER *c) const
        {
            if (c)
                yr_compiler_destroy(c);
        }
    };

    explicit YaraCompiler(std::unique_ptr<YR_COMPILER, CompilerDeleter> raw)
        : compiler_(std::move(raw))
    {
        // callbackResult_ is stable for the lifetime of this object because
        // YaraCompiler is non-movable and non-copyable.
        yr_compiler_set_callback(compiler_.get(), yara_compiler_callback, &callbackResult_);
    }

    std::unique_ptr<YR_COMPILER, CompilerDeleter> compiler_;
    YaraCallbackResult callbackResult_;
};

using YaraCompilerPtr = std::unique_ptr<YaraCompiler>;

void yara_compiler_callback(
    int error_level,
    const char *file_name,
    int line_number,
    const YR_RULE *rule,
    const char *message,
    void *user_data)
{
    auto result = reinterpret_cast<YaraCompiler::YaraCallbackResult *>(user_data);
    result->yaraError_ = YaraCompiler::Error();
    result->yaraError_->errorLevel_ = error_level;

    if (file_name) {
        result->yaraError_->fileName_ = file_name;
    }

    result->yaraError_->lineNumber_ = line_number;
    if (message) {
        result->yaraError_->message_ = message;
    }
}

std::unique_ptr<YaraInstance> YaraEditor::g_instance = nullptr;

YaraEditor::YaraEditor(QWidget *parent)
    : TextEditWidget(parent)
{
    validationTimer_.setSingleShot(true);
    connect(&validationTimer_, &QTimer::timeout, this, &YaraEditor::validate);
    connect(
        editor_->document(),
        &QTextDocument::contentsChange,
        &editorSignals(),
        &EditorInterface::Signals::contentsChanged);

    connect(
        editor_->document(),
        &QTextDocument::contentsChange,
        this,
        [this](int position, int charsRemoved, int charsAdded) {
            if (charsRemoved > 0 || charsAdded > 0) {
                validationTimer_.start(250); // restarts if already running
            }
        });

    if (g_instance == nullptr) {
        g_instance = std::make_unique<YaraInstance>();
    }

    new YaraHighlighter(editor_->document());

    auto *commentAction = new QAction(this);
    commentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
    connect(commentAction, &QAction::triggered, this, &YaraEditor::toggleLineComment);
    addAction(commentAction);
}

YaraEditor::~YaraEditor() {}

bool YaraEditor::load(std::optional<QString> path)
{
    if (!path) {
        path = QFileDialog::getOpenFileName(
            this, "Open File", "", "All Files (*.*);;Yara rule file (*.yara *.yar *.txt)");
    }

    QFile file(*path);
    if (!file.open(QIODevice::ReadWrite)) {
        LOG_CRITICAL("Failed to open file (path: {})", *path);
        return false;
    }

    editor_->setPlainText(QString::fromUtf8(file.readAll()));
    path_ = path;
    return true;
}

bool YaraEditor::load(const QByteArray &data, EditorFormat format)
{
    if (format != EditorFormat::YaraRule && format != EditorFormat::Unspecified) {
        return false;
    }

    editor_->setPlainText(QString::fromUtf8(data));
    return true;
}

bool YaraEditor::save()
{
    auto path = path_;
    if (!path) {
        return saveAs();
    }

    if (!saveToFile(*path)) {
        return false;
    }

    return true;
}

bool YaraEditor::saveAs()
{
    auto path = QFileDialog::getSaveFileName(
        this, "Save file", "", QString("Yara rule file(*.yara *.yar *.txt);;All Files(*.*)"));
    if (path.isEmpty()) {
        return false;
    }

    if (!saveToFile(path)) {
        return false;
    }

    return true;
}

bool YaraEditor::saveToFile(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_CRITICAL("Failed to open file to save (path: {})", path);
        return false;
    }

    auto data = editor_->toPlainText().toUtf8();
    if (file.write(data) != data.size()) {
        return false;
    }

    path_ = path;
    emit editorSignals().fileSaved(path);
    return true;
}

std::expected<QByteArray, std::error_code> YaraEditor::content() const
{
    return editor_->toPlainText().toUtf8();
}

std::optional<QString> YaraEditor::path() const
{
    return path_;
}

void YaraEditor::validate()
{
    QString errorMsg = "Yara rule compilation failed: ";

    std::error_code ec;
    YaraCompilerPtr compiler = YaraCompiler::create(ec);
    if (!compiler) {
        LOG_CRITICAL("Failed to initialize Yara compiler: {}", ec.message());
        return;
    }

    auto errorCount = compiler->AddRule(editor_->toPlainText());
    if (!errorCount) {
        auto error = compiler->LastError();
        if (error.has_value()) {
            status_->setText(
                QString("%1 line %2").arg(errorMsg).arg(error->lineNumber_), StatusType::Error);
        } else {
            status_->setText(
                QString("%1%2").arg(errorMsg).arg(errorCount.error().message().c_str()),
                StatusType::Error);
        }
        return;
    }

    if (*errorCount > 0) {
        auto error = compiler->LastError();
        if (error.has_value()) {
            status_->setText(
                QString("%1 line %2").arg(errorMsg).arg(error->lineNumber_), StatusType::Error);
        } else {
            status_
                ->setText(QString("%1%2 error(s)").arg(errorMsg).arg(*errorCount), StatusType::Error);
        }
        return;
    }

    auto rules = compiler->Compile();
    if (!rules) {
        auto error = compiler->LastError();
        if (error.has_value()) {
            status_->setText(
                QString("%1%2 (line %3)")
                    .arg(errorMsg)
                    .arg(rules.error().message().c_str())
                    .arg(error->lineNumber_),
                StatusType::Error);
            return;
        } else {
            status_->setText(
                QString("%1%2").arg(errorMsg).arg(rules.error().message().c_str()),
                StatusType::Error);
            return;
        }
    }

    status_->setText("Yara rule compiled successfully", StatusType::Success);
}

void YaraEditor::toggleLineComment()
{
    auto cursor = editor_->textCursor();
    cursor.beginEditBlock();

    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    QTextCursor startCursor(editor_->document());
    startCursor.setPosition(start);
    QTextCursor endCursor(editor_->document());
    endCursor.setPosition(end);

    int startBlock = startCursor.blockNumber();
    int endBlock = endCursor.blockNumber();
    // If selection ends exactly at block start, don't include that block
    if (end > start && endCursor.atBlockStart())
        --endBlock;

    // Determine whether all selected lines are commented
    bool allCommented = true;
    QTextBlock block = editor_->document()->findBlockByNumber(startBlock);
    for (int i = startBlock; i <= endBlock; ++i, block = block.next()) {
        if (!block.text().startsWith(QStringLiteral("//"))) {
            allCommented = false;
            break;
        }
    }

    block = editor_->document()->findBlockByNumber(startBlock);
    for (int i = startBlock; i <= endBlock; ++i, block = block.next()) {
        QTextCursor c(block);
        if (allCommented) {
            c.movePosition(QTextCursor::StartOfBlock);
            c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
            c.removeSelectedText();
        } else {
            c.movePosition(QTextCursor::StartOfBlock);
            c.insertText(QStringLiteral("//"));
        }
    }

    cursor.endEditBlock();
}
