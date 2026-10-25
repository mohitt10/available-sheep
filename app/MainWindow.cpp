#include "MainWindow.hpp"

#include <array>

#include <QCheckBox>
#include <QStringList>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Frontend.hpp"
#include "Optimizertypes.hpp"

namespace {

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

QString unavailableSnapshot(const QString& structure) {
    return QStringLiteral(
               "%1 snapshot unavailable.\n\n"
               "The optimizer has not been integrated with this data structure "
               "yet. The UI only displays copied snapshots supplied by the "
               "engine; it does not construct or mutate internal nodes.")
        .arg(structure);
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

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Available Sheep — Compiler Optimization Engine"));
    resize(1180, 820);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    auto* sourceGroup = new QGroupBox(QStringLiteral("Source code"), central);
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
    auto* runButton = new QPushButton(QStringLiteral("Compile source"), sourceGroup);
    runButton->setDefault(true);
    controls->addWidget(runButton);
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
        passOptions_[i]->setEnabled(false);
        optionsLayout->addWidget(passOptions_[i]);
    }
    auto* iterationLabel = new QLabel(QStringLiteral("Max iterations:"), optionsGroup);
    iterationLimit_ = new QSpinBox(optionsGroup);
    iterationLimit_->setRange(0, 100000);
    iterationLimit_->setValue(10);
    iterationLimit_->setEnabled(false);
    optionsLayout->addWidget(iterationLabel);
    optionsLayout->addWidget(iterationLimit_);
    optionsGroup->setToolTip(
        QStringLiteral("These controls become active when the optimization pipeline is integrated."));
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
    trieView_ = makeReadOnlyView(structureTabs);
    structureTabs->addTab(dagView_, QStringLiteral("DAG"));
    structureTabs->addTab(avlView_, QStringLiteral("AVL"));
    structureTabs->addTab(trieView_, QStringLiteral("Trie"));
    outputTabs->addTab(structureTabs, QStringLiteral("Data structures"));
    rootLayout->addWidget(outputTabs, 3);

    setCentralWidget(central);
    connect(runButton, &QPushButton::clicked, this, [this] { compileSource(); });
    sourceEditor_->setFocus();

    const QString waiting =
        QStringLiteral("Compile source to produce the initial IR and diagnostics.");
    initialIrView_->setPlainText(waiting);
    optimizedIrView_->setPlainText(
        QStringLiteral("Unavailable: the optimization pipeline is not implemented on this branch."));
    diagnosticsView_->setPlainText(waiting);
    resultsView_->setPlainText(
        QStringLiteral("Optimization results will appear after the optimizer is integrated."));
    dagView_->setPlainText(unavailableSnapshot(QStringLiteral("DAG")));
    avlView_->setPlainText(unavailableSnapshot(QStringLiteral("AVL")));
    trieView_->setPlainText(unavailableSnapshot(QStringLiteral("Trie")));
}

void MainWindow::compileSource() {
    initialIrView_->clear();
    optimizedIrView_->clear();
    diagnosticsView_->clear();
    resultsView_->clear();

    const FrontendResult result =
        ::compileSource(sourceEditor_->toPlainText().toStdString(), symbolRegistry_);
    diagnosticsView_->setPlainText(diagnosticText(result.diagnostics));
    if (!result.success) {
        showFrontendFailure(result);
        return;
    }
    showFrontendSuccess(result);
}

void MainWindow::showFrontendFailure(const FrontendResult&) {
    initialIrView_->setPlainText(
        QStringLiteral("No initial IR was produced because frontend validation failed."));
    optimizedIrView_->setPlainText(
        QStringLiteral("Unavailable: source did not produce a valid initial IR."));
    resultsView_->setPlainText(
        QStringLiteral("Compilation failed. See Diagnostics. No optimization was run."));
    dagView_->setPlainText(unavailableSnapshot(QStringLiteral("DAG")));
    avlView_->setPlainText(unavailableSnapshot(QStringLiteral("AVL")));
    trieView_->setPlainText(unavailableSnapshot(QStringLiteral("Trie")));
}

void MainWindow::showFrontendSuccess(const FrontendResult& result) {
    initialIrView_->setPlainText(QString::fromStdString(toString(result.program)));
    optimizedIrView_->setPlainText(
        QStringLiteral("Unavailable: the optimization pipeline is not implemented on this branch."));
    resultsView_->setPlainText(
        QStringLiteral("Frontend: succeeded\n\n"
                       "Initial IR metrics:\n%1\n\n"
                       "Optimization: not run\n"
                       "Reason: optimizer passes and the optimization driver are not integrated.")
            .arg(metricsText(computeMetrics(result.program))));
    dagView_->setPlainText(unavailableSnapshot(QStringLiteral("DAG")));
    avlView_->setPlainText(unavailableSnapshot(QStringLiteral("AVL")));
    trieView_->setPlainText(unavailableSnapshot(QStringLiteral("Trie")));
}
