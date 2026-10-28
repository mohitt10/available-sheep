#pragma once

#include <vector>

#include <QMainWindow>

#include "Ir.hpp"
#include "identifier_trie.hpp"

class QCheckBox;
class QComboBox;
class QGraphicsView;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QWidget;
struct Diagnostic;
struct PipelineResult;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void compileSource();
    void showPipelineResult(const PipelineResult& result);
    void updateTrieView();
    void runProgram();
    void showDiagnostics(const std::vector<Diagnostic>& diagnostics);
    void navigateToDiagnostic(int line, int column);

    QGroupBox* sourceGroup_ = nullptr;
    QPlainTextEdit* sourceEditor_ = nullptr;
    QPlainTextEdit* initialIrView_ = nullptr;
    QPlainTextEdit* optimizedIrView_ = nullptr;
    QListWidget* diagnosticsView_ = nullptr;
    QPlainTextEdit* resultsView_ = nullptr;
    QGraphicsView* dagView_ = nullptr;
    QGraphicsView* avlView_ = nullptr;
    QPlainTextEdit* trieView_ = nullptr;
    QWidget* runControls_ = nullptr;
    QComboBox* programVersion_ = nullptr;
    QLineEdit* runtimeInput_ = nullptr;
    QPlainTextEdit* runtimeOutput_ = nullptr;
    QLineEdit* trieQuery_ = nullptr;
    QLabel* runSummary_ = nullptr;
    QLabel* instructionSummary_ = nullptr;
    QLabel* iterationSummary_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QCheckBox* passOptions_[4]{};
    QSpinBox* iterationLimit_ = nullptr;
    std::vector<available_sheep::TrieEntry> trieEntries_;
    Program initialProgram_;
    Program optimizedProgram_;
    bool hasInitialProgram_ = false;
    bool hasOptimizedProgram_ = false;
};
