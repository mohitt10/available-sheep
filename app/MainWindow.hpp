#pragma once

#include <QMainWindow>

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
struct PipelineResult;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void compileSource();
    void showPipelineResult(const PipelineResult& result);

    QGroupBox* sourceGroup_ = nullptr;
    QPlainTextEdit* sourceEditor_ = nullptr;
    QPlainTextEdit* initialIrView_ = nullptr;
    QPlainTextEdit* optimizedIrView_ = nullptr;
    QPlainTextEdit* diagnosticsView_ = nullptr;
    QPlainTextEdit* resultsView_ = nullptr;
    QPlainTextEdit* dagView_ = nullptr;
    QPlainTextEdit* avlView_ = nullptr;
    QPlainTextEdit* trieView_ = nullptr;
    QLineEdit* trieQuery_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QCheckBox* passOptions_[4]{};
    QSpinBox* iterationLimit_ = nullptr;
};
