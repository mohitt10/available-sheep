#pragma once

#include <QMainWindow>

#include "SymbolRegistry.hpp"

struct FrontendResult;

class QCheckBox;
class QPlainTextEdit;
class QSpinBox;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void compileSource();
    void showFrontendFailure(const FrontendResult& result);
    void showFrontendSuccess(const FrontendResult& result);

    QPlainTextEdit* sourceEditor_ = nullptr;
    QPlainTextEdit* initialIrView_ = nullptr;
    QPlainTextEdit* optimizedIrView_ = nullptr;
    QPlainTextEdit* diagnosticsView_ = nullptr;
    QPlainTextEdit* resultsView_ = nullptr;
    QPlainTextEdit* dagView_ = nullptr;
    QPlainTextEdit* avlView_ = nullptr;
    QPlainTextEdit* trieView_ = nullptr;
    QCheckBox* passOptions_[4]{};
    QSpinBox* iterationLimit_ = nullptr;
    SimpleSymbolRegistry symbolRegistry_;
};
