#include "MainWindow.hpp"

#include <array>
#include <exception>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QSpinBox>
#include <QStatusBar>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QtConcurrent/QtConcurrentRun>

#include "Frontend.hpp"
#include "identifier_trie.hpp"
#include "Optimizer.hpp"
#include "Optimizertypes.hpp"
#include "SymbolRegistry.hpp"

struct PipelineResult {
    FrontendResult frontend;
    OptimizationResult optimization;
    std::string trieQuery;
    std::string error;
    std::vector<available_sheep::TrieEntry> trieEntries;
    std::vector<SymbolId> exactMatches;
    std::vector<std::string> prefixMatches;
    std::vector<std::string> suggestions;
};

namespace {

PipelineResult runPipeline(const std::string& source, const PassOptions& options,
                           const std::string& trieQuery) {
    PipelineResult result;
    result.trieQuery = trieQuery;
    SimpleSymbolRegistry registry;
    available_sheep::IdentifierTrie trie;
    try {
        result.frontend = ::compileSource(source, registry, &trie);
        if (result.frontend.success)
            result.optimization =
                optimizeProgram(result.frontend.program, options, &trie);

        result.trieEntries = trie.snapshot();
        result.exactMatches = trie.findExact(trieQuery);
        result.prefixMatches = trie.findByPrefix(trieQuery, 10);
        result.suggestions = trie.suggest(trieQuery, 10);
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

QPlainTextEdit* makeReadOnlyView(QWidget* parent) {
    auto* view = new QPlainTextEdit(parent);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    return view;
}

QString stageName(DiagStage stage) {
    switch (stage) {
        case DiagStage::Lexical:
            return QStringLiteral("Lexical");
        case DiagStage::Syntax:
            return QStringLiteral("Syntax");
        case DiagStage::Semantic:
            return QStringLiteral("Semantic");
        case DiagStage::Optimization:
            return QStringLiteral("Optimization");
    }
    return QStringLiteral("Unknown");
}

QString severityName(Severity severity) {
    switch (severity) {
        case Severity::Error:
            return QStringLiteral("Error");
        case Severity::Warning:
            return QStringLiteral("Warning");
        case Severity::Note:
            return QStringLiteral("Note");
    }
    return QStringLiteral("Unknown");
}

QString diagnosticText(const std::vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return QStringLiteral("No diagnostics.");
    }

    QStringList lines;
    for (const Diagnostic& diagnostic : diagnostics) {
        QString location;
        if (diagnostic.line > 0) {
            location = QStringLiteral("%1:%2: ")
                           .arg(diagnostic.line)
                           .arg(diagnostic.column);
        }
        lines.push_back(QStringLiteral("%1 %2 %3%4")
                            .arg(severityName(diagnostic.severity),
                                 stageName(diagnostic.stage),
                                 location,
                                 QString::fromStdString(diagnostic.message)));
    }
    return lines.join(QLatin1Char('\n'));
}

QString metricsText(const IrMetrics& metrics) {
    return QStringLiteral(
               "Instructions: %1\n"
               "Copies: %2\n"
               "Unary operations: %3\n"
               "Binary operations: %4\n"
               "Inputs: %5\n"
               "Prints: %6")
        .arg(metrics.instructions)
        .arg(metrics.copies)
        .arg(metrics.unaryOps)
        .arg(metrics.binaryOps)
        .arg(metrics.inputs)
        .arg(metrics.prints);
}

QString convergenceName(Convergence convergence) {
    switch (convergence) {
        case Convergence::Converged:
            return QStringLiteral("Converged");
        case Convergence::IterationLimit:
            return QStringLiteral("Iteration limit reached");
        case Convergence::RepeatedState:
            return QStringLiteral("Stopped after a repeated IR state");
    }
    return QStringLiteral("Unknown");
}

QString trieText(const PipelineResult& result, const QString& query) {
    QStringList lines;
    lines.push_back(result.trieEntries.empty()
                        ? QStringLiteral("No identifiers indexed.")
                        : QStringLiteral("Indexed identifiers:"));
    for (const auto& entry : result.trieEntries) {
        QStringList ids;
        for (SymbolId id : entry.symbols) ids.push_back(QString::number(id));
        lines.push_back(QStringLiteral("%1 -> %2")
                            .arg(QString::fromStdString(entry.spelling),
                                 ids.join(QStringLiteral(", "))));
    }

    const QString queryLabel = query.isEmpty() ? QStringLiteral("<empty>") : query;
    QStringList exactIds;
    for (SymbolId id : result.exactMatches)
        exactIds.push_back(QString::number(id));
    const QString exactResult =
        exactIds.isEmpty()
            ? QStringLiteral("not found")
            : QStringLiteral("Symbol IDs %1").arg(exactIds.join(QStringLiteral(", ")));
    lines.push_back(QStringLiteral("\nExact lookup for \"%1\": %2")
                        .arg(queryLabel, exactResult));
    auto spellingsText = [](const std::vector<std::string>& spellings) {
        if (spellings.empty()) return QStringLiteral("none");
        QStringList values;
        for (const std::string& spelling : spellings)
            values.push_back(QString::fromStdString(spelling));
        return values.join(QStringLiteral(", "));
    };
    lines.push_back(QStringLiteral("Prefix matches (up to 10): %1")
                        .arg(spellingsText(result.prefixMatches)));
    lines.push_back(QStringLiteral("Suggestions (up to 10, edit distance <= 2): %1")
                        .arg(spellingsText(result.suggestions)));
    return lines.join(QLatin1Char('\n'));
}

QString dagText(const OptimizationResult& result) {
    if (result.dagNodes.empty()) return QStringLiteral("No expressions in this program.");
    QStringList lines;
    for (const auto& node : result.dagNodes) {
        QStringList operands;
        for (auto operand : node.key.operands)
            operands.push_back(QString::number(static_cast<qulonglong>(operand)));
        lines.push_back(
            QStringLiteral("e%1 = op%2<type %3>(%4)%5")
                .arg(static_cast<qulonglong>(node.id))
                .arg(static_cast<unsigned>(node.key.op))
                .arg(node.key.type)
                .arg(operands.join(QStringLiteral(", ")))
                .arg(node.isPure ? QStringLiteral(" [pure]") :
                                   QStringLiteral(" [effectful]")));
    }
    return lines.join(QLatin1Char('\n'));
}

QString avlText(const OptimizationResult& result) {
    QStringList lines;
    for (const AvlNodeSnapshot& node : result.avlNodes) {
        lines.push_back(QStringLiteral("e%1: uses=%2 height=%3")
                            .arg(static_cast<qulonglong>(node.id))
                            .arg(static_cast<qulonglong>(node.info.useCount))
                            .arg(node.height));
    }
    if (!result.avlRotations.empty()) {
        lines.push_back(QStringLiteral("\nRotations:"));
        for (const RotationEvent& rotation : result.avlRotations) {
            QString kind;
            switch (rotation.kind) {
                case AvlRotationKind::Left: kind = QStringLiteral("Left"); break;
                case AvlRotationKind::Right: kind = QStringLiteral("Right"); break;
                case AvlRotationKind::LeftRight:
                    kind = QStringLiteral("LeftRight");
                    break;
                case AvlRotationKind::RightLeft:
                    kind = QStringLiteral("RightLeft");
                    break;
            }
            lines.push_back(QStringLiteral("%1 at e%2")
                                .arg(kind)
                                .arg(static_cast<qulonglong>(rotation.pivot)));
        }
    }
    if (lines.empty()) return QStringLiteral("No expression metadata.");
    return lines.join(QLatin1Char('\n'));
}

QString optimizationText(const OptimizationResult& result) {
    QStringList lines;
    lines.push_back(QStringLiteral("Outcome: %1")
                        .arg(result.outcome == RunOutcome::Optimized
                                 ? QStringLiteral("Optimized")
                                 : QStringLiteral("Optimization failed")));
    lines.push_back(QStringLiteral("Convergence: %1")
                        .arg(convergenceName(result.convergence)));
    lines.push_back(QStringLiteral("Iterations: %1").arg(result.iterationsRun));
    lines.push_back(QStringLiteral("\nBefore:\n%1").arg(metricsText(result.before)));
    lines.push_back(QStringLiteral("\nAfter:\n%1").arg(metricsText(result.after)));
    for (const IterationReport& iteration : result.iterations) {
        lines.push_back(QStringLiteral("\nIteration %1%2")
                            .arg(iteration.iteration)
                            .arg(iteration.changed ? QStringLiteral(" (changed)") :
                                                     QStringLiteral(" (no changes)")));
        for (const PassStats& pass : iteration.passes) {
            lines.push_back(QStringLiteral("  %1: rewritten %2, removed %3")
                                .arg(QString::fromLatin1(passName(pass.pass)))
                                .arg(pass.instructionsRewritten)
                                .arg(pass.instructionsRemoved));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Available Sheep — Compiler Optimization Engine"));
    resize(1180, 820);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    sourceGroup_ = new QGroupBox(QStringLiteral("Source code"), central);
    auto* sourceGroup = sourceGroup_;
    auto* sourceLayout = new QVBoxLayout(sourceGroup);
    sourceEditor_ = new QPlainTextEdit(sourceGroup);
    sourceEditor_->setPlaceholderText(
        QStringLiteral("Enter a program in the supported integer language."));
    sourceEditor_->setPlainText(
        QStringLiteral("int value = 2 + 3 * 4;\n"
                       "print(value);\n"));
    sourceEditor_->setTabStopDistance(4 * sourceEditor_->fontMetrics().horizontalAdvance(' '));
    sourceLayout->addWidget(sourceEditor_);

    auto* controls = new QHBoxLayout();
    runButton_ = new QPushButton(QStringLiteral("Run pipeline"), sourceGroup);
    runButton_->setDefault(true);
    controls->addWidget(runButton_);
    controls->addSpacing(12);

    auto* optionsGroup = new QGroupBox(QStringLiteral("Optimization options"), sourceGroup);
    auto* optionsLayout = new QHBoxLayout(optionsGroup);
    const std::array<const char*, kPassCount> passLabels{
        "Constant folding",
        "Constant propagation",
        "Common subexpression elimination",
        "Dead-code elimination"};
    for (int i = 0; i < kPassCount; ++i) {
        passOptions_[i] = new QCheckBox(QString::fromLatin1(passLabels[i]), optionsGroup);
        passOptions_[i]->setChecked(true);
        passOptions_[i]->setEnabled(true);
        optionsLayout->addWidget(passOptions_[i]);
    }
    auto* iterationLabel = new QLabel(QStringLiteral("Max iterations:"), optionsGroup);
    iterationLimit_ = new QSpinBox(optionsGroup);
    iterationLimit_->setRange(0, 100000);
    iterationLimit_->setValue(10);
    iterationLimit_->setEnabled(true);
    optionsLayout->addWidget(iterationLabel);
    optionsLayout->addWidget(iterationLimit_);
    optionsGroup->setToolTip(
        QStringLiteral("Choose enabled passes and the maximum fixed-point iterations."));
    controls->addWidget(optionsGroup, 1);
    sourceLayout->addLayout(controls);
    rootLayout->addWidget(sourceGroup, 2);

    auto* outputTabs = new QTabWidget(central);

    auto* irSplitter = new QSplitter(Qt::Horizontal, outputTabs);
    auto* initialPane = new QWidget(irSplitter);
    auto* initialLayout = new QVBoxLayout(initialPane);
    initialLayout->setContentsMargins(0, 0, 0, 0);
    initialLayout->addWidget(new QLabel(QStringLiteral("Initial IR"), initialPane));
    initialIrView_ = makeReadOnlyView(initialPane);
    initialLayout->addWidget(initialIrView_);

    auto* optimizedPane = new QWidget(irSplitter);
    auto* optimizedLayout = new QVBoxLayout(optimizedPane);
    optimizedLayout->setContentsMargins(0, 0, 0, 0);
    optimizedLayout->addWidget(new QLabel(QStringLiteral("Optimized IR"), optimizedPane));
    optimizedIrView_ = makeReadOnlyView(optimizedPane);
    optimizedLayout->addWidget(optimizedIrView_);
    outputTabs->addTab(irSplitter, QStringLiteral("IR"));

    diagnosticsView_ = makeReadOnlyView(outputTabs);
    outputTabs->addTab(diagnosticsView_, QStringLiteral("Diagnostics"));

    resultsView_ = makeReadOnlyView(outputTabs);
    outputTabs->addTab(resultsView_, QStringLiteral("Results"));

    auto* structureTabs = new QTabWidget(outputTabs);
    dagView_ = makeReadOnlyView(structureTabs);
    avlView_ = makeReadOnlyView(structureTabs);
    structureTabs->addTab(dagView_, QStringLiteral("DAG"));
    structureTabs->addTab(avlView_, QStringLiteral("AVL"));
    auto* triePane = new QWidget(structureTabs);
    auto* trieLayout = new QVBoxLayout(triePane);
    auto* trieQueryRow = new QHBoxLayout();
    trieQueryRow->addWidget(new QLabel(QStringLiteral("Lookup / prefix / suggestion:"), triePane));
    trieQuery_ = new QLineEdit(triePane);
    trieQuery_->setPlaceholderText(QStringLiteral("Enter an identifier to query"));
    trieQuery_->setToolTip(
        QStringLiteral("Exact, prefix, and spelling suggestions are computed on the next run."));
    trieQueryRow->addWidget(trieQuery_, 1);
    trieLayout->addLayout(trieQueryRow);
    trieView_ = makeReadOnlyView(triePane);
    trieLayout->addWidget(trieView_);
    structureTabs->addTab(triePane, QStringLiteral("Trie"));
    outputTabs->addTab(structureTabs, QStringLiteral("Data structures"));
    rootLayout->addWidget(outputTabs, 3);

    setCentralWidget(central);
    connect(runButton_, &QPushButton::clicked, this, [this] { compileSource(); });
    sourceEditor_->setFocus();

    const QString waiting =
        QStringLiteral("Compile source to produce the initial IR and diagnostics.");
    initialIrView_->setPlainText(waiting);
    optimizedIrView_->setPlainText(QStringLiteral("Compile source to produce optimized IR."));
    diagnosticsView_->setPlainText(waiting);
    resultsView_->setPlainText(QStringLiteral("Optimization results will appear here."));
    dagView_->setPlainText(QStringLiteral("Compile source to build the expression DAG."));
    avlView_->setPlainText(QStringLiteral("Compile source to build AVL metadata."));
    trieView_->setPlainText(QStringLiteral("Compile source to index identifiers."));
}

void MainWindow::compileSource() {
    if (!sourceGroup_->isEnabled()) return;

    initialIrView_->clear();
    optimizedIrView_->clear();
    diagnosticsView_->clear();
    resultsView_->clear();
    dagView_->setPlainText(QStringLiteral("Building expression DAG..."));
    avlView_->setPlainText(QStringLiteral("Building AVL metadata..."));
    trieView_->setPlainText(QStringLiteral("Querying identifier trie..."));

    PassOptions options;
    for (int i = 0; i < kPassCount; ++i)
        options.enabled[i] = passOptions_[i]->isChecked();
    options.maxIterations = iterationLimit_->value();
    const std::string source = sourceEditor_->toPlainText().toStdString();
    const std::string trieQuery = trieQuery_->text().toStdString();

    sourceGroup_->setEnabled(false);
    trieQuery_->setEnabled(false);
    runButton_->setText(QStringLiteral("Running..."));
    statusBar()->showMessage(QStringLiteral("Frontend and optimization pipeline running..."));
    auto* watcher = new QFutureWatcher<PipelineResult>(this);
    connect(watcher, &QFutureWatcher<PipelineResult>::finished, this,
            [this, watcher] {
                const PipelineResult result = watcher->result();
                watcher->deleteLater();
                sourceGroup_->setEnabled(true);
                trieQuery_->setEnabled(true);
                runButton_->setText(QStringLiteral("Run pipeline"));
                statusBar()->clearMessage();
                showPipelineResult(result);
            });
    watcher->setFuture(QtConcurrent::run(
        [source, options, trieQuery] { return runPipeline(source, options, trieQuery); }));
}

void MainWindow::showPipelineResult(const PipelineResult& result) {
    std::vector<Diagnostic> diagnostics = result.frontend.diagnostics;
    if (result.frontend.success) {
        initialIrView_->setPlainText(QString::fromStdString(toString(result.frontend.program)));
        if (!result.error.empty()) {
            optimizedIrView_->setPlainText(
                QStringLiteral("Unavailable: optimization failed."));
            resultsView_->setPlainText(QStringLiteral("Optimization failed: %1")
                                           .arg(QString::fromStdString(result.error)));
        } else {
            optimizedIrView_->setPlainText(
                QString::fromStdString(toString(result.optimization.optimizedIr)));
            resultsView_->setPlainText(optimizationText(result.optimization));
            diagnostics.insert(diagnostics.end(),
                               result.optimization.diagnostics.begin(),
                               result.optimization.diagnostics.end());
            dagView_->setPlainText(dagText(result.optimization));
            avlView_->setPlainText(avlText(result.optimization));
        }
    } else {
        initialIrView_->setPlainText(
            QStringLiteral("No initial IR was produced because frontend validation failed."));
        optimizedIrView_->setPlainText(
            QStringLiteral("Unavailable: source did not produce a valid initial IR."));
        resultsView_->setPlainText(
            result.error.empty()
                ? QStringLiteral("Compilation failed. See Diagnostics. No optimization was run.")
                : QStringLiteral("Pipeline failed: %1").arg(QString::fromStdString(result.error)));
        dagView_->setPlainText(QStringLiteral("No DAG was produced."));
        avlView_->setPlainText(QStringLiteral("No AVL metadata was produced."));
    }
    if (!result.error.empty()) {
        diagnostics.push_back(
            Diagnostic{Severity::Error,
                       result.frontend.success ? DiagStage::Optimization : DiagStage::Semantic,
                       result.error, 0, 0});
    }
    diagnosticsView_->setPlainText(diagnosticText(diagnostics));
    trieView_->setPlainText(trieText(result, QString::fromStdString(result.trieQuery)));
}
