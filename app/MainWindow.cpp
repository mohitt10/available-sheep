#include "MainWindow.hpp"

#include <array>
#include <algorithm>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QFont>
#include <QGraphicsTextItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRegularExpression>
#include <QShortcut>
#include <QStringList>
#include <QSpinBox>
#include <QStatusBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>
#include <QVBoxLayout>
#include <QWidget>
#include <QWheelEvent>

#include <QtConcurrent/QtConcurrentRun>

#include "Diagnostics.hpp"
#include "Frontend.hpp"
#include "Interpreter.hpp"
#include "identifier_trie.hpp"
#include "Optimizer.hpp"
#include "Optimizertypes.hpp"
#include "SymbolRegistry.hpp"

struct PipelineResult {
    FrontendResult frontend;
    OptimizationResult optimization;
    std::string error;
    std::vector<available_sheep::TrieEntry> trieEntries;
};

namespace {

PipelineResult runPipeline(const std::string& source, const PassOptions& options) {
    PipelineResult result;
    SimpleSymbolRegistry registry;
    available_sheep::IdentifierTrie trie;
    try {
        result.frontend = ::compileSource(source, registry, &trie);
        if (result.frontend.success)
            result.optimization =
                optimizeProgram(result.frontend.program, options, &trie);

        result.trieEntries = trie.snapshot();
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

QFrame* makeSummaryCard(const QString& title, QLabel*& value, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("summaryCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(3);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("summaryTitle"));
    value = new QLabel(QStringLiteral("—"), card);
    value->setObjectName(QStringLiteral("summaryValue"));
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    return card;
}

class InteractiveGraphView final : public QGraphicsView {
public:
    explicit InteractiveGraphView(QWidget* parent = nullptr)
        : QGraphicsView(parent), graphScene_(new QGraphicsScene(this)) {
        setScene(graphScene_);
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setBackgroundBrush(QColor(QStringLiteral("#0c1221")));
        setFrameShape(QFrame::NoFrame);
        setToolTip(QStringLiteral("Scroll to zoom · drag to pan · hover over a node for details"));
    }

    QGraphicsScene* graphScene() const { return graphScene_; }

protected:
    void wheelEvent(QWheelEvent* event) override {
        const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        const qreal current = transform().m11();
        if ((factor > 1.0 && current < 3.0) ||
            (factor < 1.0 && current > 0.25))
            scale(factor, factor);
        event->accept();
    }

private:
    QGraphicsScene* graphScene_;
};

QGraphicsView* makeGraphView(QWidget* parent) {
    return new InteractiveGraphView(parent);
}

void showGraphMessage(QGraphicsView* view, const QString& message) {
    auto* scene = view->scene();
    scene->clear();
    auto* text = scene->addText(message, QFont(QStringLiteral("Segoe UI"), 12));
    text->setDefaultTextColor(QColor(QStringLiteral("#aab8d2")));
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-30, -30, 30, 30));
}

void addGraphNode(QGraphicsScene* scene, const QPointF& position,
                  const QSizeF& size, const QString& title,
                  const QString& detail, const QString& tooltip,
                  const QColor& fill) {
    QPainterPath boxPath;
    boxPath.addRoundedRect(QRectF(position, size), 10, 10);
    auto* box = scene->addPath(boxPath,
                               QPen(QColor(QStringLiteral("#445574")), 1.4),
                               QBrush(fill));
    box->setToolTip(tooltip);
    auto* heading = scene->addText(title);
    heading->setDefaultTextColor(QColor(QStringLiteral("#f0f3fb")));
    QFont headingFont(QStringLiteral("Segoe UI"));
    headingFont.setBold(true);
    headingFont.setPointSize(10);
    heading->setFont(headingFont);
    heading->setTextWidth(size.width() - 20);
    heading->setPos(position.x() + 10, position.y() + 10);
    heading->setToolTip(tooltip);
    auto* caption = scene->addText(detail);
    caption->setDefaultTextColor(QColor(QStringLiteral("#aab8d2")));
    QFont captionFont(QStringLiteral("Segoe UI"));
    captionFont.setPointSize(9);
    caption->setFont(captionFont);
    caption->setTextWidth(size.width() - 20);
    caption->setPos(position.x() + 10, position.y() + 39);
    caption->setToolTip(tooltip);
}

void addGraphEdge(QGraphicsScene* scene, const QPointF& from, const QPointF& to) {
    const qreal middleX = (from.x() + to.x()) / 2.0;
    QPainterPath path(from);
    path.cubicTo(middleX, from.y(), middleX, to.y(), to.x(), to.y());
    scene->addPath(path, QPen(QColor(QStringLiteral("#536483")), 1.5));
}

QString operatorName(available_sheep::OperatorKind op) {
    using available_sheep::OperatorKind;
    switch (op) {
        case OperatorKind::Constant: return QStringLiteral("Constant");
        case OperatorKind::Identifier: return QStringLiteral("Identifier");
        case OperatorKind::Add: return QStringLiteral("Add");
        case OperatorKind::Subtract: return QStringLiteral("Subtract");
        case OperatorKind::Multiply: return QStringLiteral("Multiply");
        case OperatorKind::Divide: return QStringLiteral("Divide");
        case OperatorKind::Remainder: return QStringLiteral("Remainder");
        case OperatorKind::Negate: return QStringLiteral("Negate");
        case OperatorKind::LogicalNot: return QStringLiteral("Logical not");
        case OperatorKind::BitwiseNot: return QStringLiteral("Bitwise not");
        case OperatorKind::Equal: return QStringLiteral("Equal");
        case OperatorKind::NotEqual: return QStringLiteral("Not equal");
        case OperatorKind::Less: return QStringLiteral("Less than");
        case OperatorKind::LessEqual: return QStringLiteral("Less or equal");
        case OperatorKind::Greater: return QStringLiteral("Greater than");
        case OperatorKind::GreaterEqual: return QStringLiteral("Greater or equal");
        case OperatorKind::LogicalAnd: return QStringLiteral("Logical and");
        case OperatorKind::LogicalOr: return QStringLiteral("Logical or");
        case OperatorKind::BitwiseAnd: return QStringLiteral("Bitwise and");
        case OperatorKind::BitwiseOr: return QStringLiteral("Bitwise or");
        case OperatorKind::BitwiseXor: return QStringLiteral("Bitwise xor");
        case OperatorKind::ShiftLeft: return QStringLiteral("Shift left");
        case OperatorKind::ShiftRight: return QStringLiteral("Shift right");
        case OperatorKind::Cast: return QStringLiteral("Cast");
        case OperatorKind::Call: return QStringLiteral("Call / output");
        case OperatorKind::Load: return QStringLiteral("Load");
        case OperatorKind::Store: return QStringLiteral("Store");
        case OperatorKind::Assign: return QStringLiteral("Assign");
        case OperatorKind::Select: return QStringLiteral("Select");
        case OperatorKind::Other: return QStringLiteral("Other");
    }
    return QStringLiteral("Unknown");
}

void drawDag(QGraphicsView* view, const OptimizationResult& result) {
    auto* scene = view->scene();
    scene->clear();
    if (result.dagNodes.empty()) {
        scene->addText(QStringLiteral("No expressions in this program."),
                       QFont(QStringLiteral("Segoe UI"), 12))
            ->setDefaultTextColor(QColor(QStringLiteral("#aab8d2")));
        scene->setSceneRect(scene->itemsBoundingRect().adjusted(-30, -30, 30, 30));
        return;
    }

    constexpr qreal nodeWidth = 190;
    constexpr qreal nodeHeight = 82;
    constexpr qreal columnGap = 255;
    constexpr qreal rowGap = 112;
    std::map<available_sheep::ExprId, int> depths;
    std::map<int, std::vector<const available_sheep::ExpressionNodeSnapshot*>> columns;
    for (const auto& node : result.dagNodes) {
        int depth = 0;
        for (const auto operand : node.key.operands) {
            const auto found = depths.find(operand);
            if (found != depths.end()) depth = std::max(depth, found->second + 1);
        }
        depths[node.id] = depth;
        columns[depth].push_back(&node);
    }

    std::map<available_sheep::ExprId, QPointF> positions;
    for (const auto& [depth, nodes] : columns) {
        for (std::size_t row = 0; row < nodes.size(); ++row) {
            positions[nodes[row]->id] =
                QPointF(45 + depth * columnGap, 42 + row * rowGap);
        }
    }

    for (const auto& node : result.dagNodes) {
        const QPointF target = positions.at(node.id);
        for (const auto operand : node.key.operands) {
            const QPointF source = positions.at(operand);
            addGraphEdge(scene,
                         QPointF(source.x() + nodeWidth, source.y() + nodeHeight / 2),
                         QPointF(target.x(), target.y() + nodeHeight / 2));
        }
    }

    for (const auto& node : result.dagNodes) {
        const QPointF position = positions.at(node.id);
        QString detail;
        if (node.key.op == available_sheep::OperatorKind::Constant &&
            !node.key.attributes.empty()) {
            detail = QStringLiteral("value %1 · Int32")
                         .arg(static_cast<qint32>(
                             static_cast<quint32>(node.key.attributes.front())));
        } else {
            QStringList operands;
            for (const auto operand : node.key.operands)
                operands.push_back(QStringLiteral("e%1")
                                       .arg(static_cast<qulonglong>(operand)));
            detail = operands.isEmpty() ? QStringLiteral("no operands")
                                        : QStringLiteral("uses %1").arg(operands.join(", "));
        }

        const QString purity = node.isPure ? QStringLiteral("Pure")
                                           : QStringLiteral("Effectful");
        const QString title =
            QStringLiteral("e%1 · %2")
                .arg(static_cast<qulonglong>(node.id))
                .arg(operatorName(node.key.op));
        const QString tooltip =
            QStringLiteral("Expression e%1\nOperator: %2\nType ID: %3\nOperands: %4\n"
                           "Attributes: %5\n%6")
                .arg(static_cast<qulonglong>(node.id))
                .arg(operatorName(node.key.op))
                .arg(node.key.type)
                .arg(detail)
                .arg(node.key.attributes.empty()
                         ? QStringLiteral("none")
                         : [&node] {
                               QStringList values;
                               for (const auto value : node.key.attributes)
                                   values.push_back(QString::number(
                                       static_cast<qulonglong>(value)));
                               return values.join(QStringLiteral(", "));
                           }())
                .arg(purity);
        addGraphNode(scene, position, QSizeF(nodeWidth, nodeHeight), title,
                     QStringLiteral("%1 · %2").arg(detail, purity), tooltip,
                     node.isPure ? QColor(QStringLiteral("#182b3d"))
                                 : QColor(QStringLiteral("#382944")));
    }
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-35, -35, 45, 45));
}

QString rotationName(AvlRotationKind kind) {
    switch (kind) {
        case AvlRotationKind::Left: return QStringLiteral("Left");
        case AvlRotationKind::Right: return QStringLiteral("Right");
        case AvlRotationKind::LeftRight: return QStringLiteral("Left-right");
        case AvlRotationKind::RightLeft: return QStringLiteral("Right-left");
    }
    return QStringLiteral("Unknown");
}

void drawAvl(QGraphicsView* view, const OptimizationResult& result) {
    auto* scene = view->scene();
    scene->clear();
    if (result.avlNodes.empty()) {
        scene->addText(QStringLiteral("No AVL nodes in this program."),
                       QFont(QStringLiteral("Segoe UI"), 12))
            ->setDefaultTextColor(QColor(QStringLiteral("#aab8d2")));
        scene->setSceneRect(scene->itemsBoundingRect().adjusted(-30, -30, 30, 30));
        return;
    }

    constexpr qreal nodeWidth = 180;
    constexpr qreal nodeHeight = 76;
    constexpr qreal horizontalGap = 220;
    constexpr qreal verticalGap = 132;
    std::map<available_sheep::ExprId, const AvlNodeSnapshot*> nodes;
    std::set<available_sheep::ExprId> children;
    for (const auto& node : result.avlNodes) {
        nodes[node.id] = &node;
        if (node.leftChild) children.insert(*node.leftChild);
        if (node.rightChild) children.insert(*node.rightChild);
    }
    const AvlNodeSnapshot* root = nullptr;
    for (const auto& node : result.avlNodes) {
        if (!children.count(node.id)) {
            root = &node;
            break;
        }
    }
    if (!root) return;

    std::map<available_sheep::ExprId, QPointF> positions;
    std::size_t inorderIndex = 0;
    const auto layout = [&](const auto& self, available_sheep::ExprId id,
                            int depth) -> void {
        const auto found = nodes.find(id);
        if (found == nodes.end()) return;
        const AvlNodeSnapshot& node = *found->second;
        if (node.leftChild) self(self, *node.leftChild, depth + 1);
        positions[id] = QPointF(42 + inorderIndex++ * horizontalGap,
                                42 + depth * verticalGap);
        if (node.rightChild) self(self, *node.rightChild, depth + 1);
    };
    layout(layout, root->id, 0);

    std::map<available_sheep::ExprId, QStringList> rotationsByNode;
    for (const RotationEvent& rotation : result.avlRotations)
        rotationsByNode[rotation.pivot].push_back(rotationName(rotation.kind));

    for (const auto& node : result.avlNodes) {
        const QPointF parent = positions.at(node.id);
        const auto drawChildEdge = [&](const std::optional<available_sheep::ExprId>& child) {
            if (!child || !positions.count(*child)) return;
            const QPointF childPosition = positions.at(*child);
            QPainterPath edge(QPointF(parent.x() + nodeWidth / 2, parent.y() + nodeHeight));
            const qreal middleY = (parent.y() + nodeHeight + childPosition.y()) / 2;
            edge.cubicTo(parent.x() + nodeWidth / 2, middleY,
                         childPosition.x() + nodeWidth / 2, middleY,
                         childPosition.x() + nodeWidth / 2, childPosition.y());
            scene->addPath(edge, QPen(QColor(QStringLiteral("#536483")), 1.5));
        };
        drawChildEdge(node.leftChild);
        drawChildEdge(node.rightChild);
    }

    for (const auto& node : result.avlNodes) {
        const bool rotated = rotationsByNode.count(node.id) != 0;
        const QString title = QStringLiteral("e%1%2")
                                  .arg(static_cast<qulonglong>(node.id))
                                  .arg(node.id == root->id ? QStringLiteral(" · root")
                                                           : QString());
        const QString detail =
            QStringLiteral("height %1 · uses %2")
                .arg(node.height)
                .arg(static_cast<qulonglong>(node.info.useCount));
        QStringList childIds;
        childIds.push_back(node.leftChild
                               ? QStringLiteral("left e%1").arg(
                                     static_cast<qulonglong>(*node.leftChild))
                               : QStringLiteral("left —"));
        childIds.push_back(node.rightChild
                               ? QStringLiteral("right e%1").arg(
                                     static_cast<qulonglong>(*node.rightChild))
                               : QStringLiteral("right —"));
        QString rotationInfo = rotationsByNode.count(node.id)
                                   ? QStringLiteral("\nRotations: %1")
                                         .arg(rotationsByNode.at(node.id).join(", "))
                                   : QString();
        const QString tooltip =
            QStringLiteral("AVL node e%1\n%2\n%3%4")
                .arg(static_cast<qulonglong>(node.id))
                .arg(detail)
                .arg(childIds.join(QStringLiteral("\n")))
                .arg(rotationInfo);
        addGraphNode(scene, positions.at(node.id), QSizeF(nodeWidth, nodeHeight),
                     title, detail, tooltip,
                     rotated ? QColor(QStringLiteral("#493b2b"))
                             : QColor(QStringLiteral("#182b3d")));
    }

    const QStringList rotationLabels = [&result] {
        QStringList labels;
        for (const RotationEvent& event : result.avlRotations)
            labels.push_back(QStringLiteral("%1 @ e%2")
                                 .arg(rotationName(event.kind))
                                 .arg(static_cast<qulonglong>(event.pivot)));
        return labels;
    }();
    const QString rotationSummary =
        rotationLabels.isEmpty()
            ? QStringLiteral("No rotations during AVL construction")
            : QStringLiteral("Rotations: %1").arg(rotationLabels.join(QStringLiteral("  ·  ")));
    auto* summary = scene->addText(rotationSummary);
    summary->setDefaultTextColor(QColor(QStringLiteral("#c1cbe0")));
    summary->setPos(42, 55 + (positions.size() > 0
                                  ? std::max_element(
                                        positions.begin(), positions.end(),
                                        [](const auto& left, const auto& right) {
                                            return left.second.y() < right.second.y();
                                        })->second.y() +
                                        nodeHeight
                                  : 0));
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-35, -35, 45, 45));
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

QString trieText(const std::vector<available_sheep::TrieEntry>& entries,
                 const QString& query) {
    QStringList lines;
    lines.push_back(entries.empty()
                        ? QStringLiteral("No identifiers indexed.")
                        : QStringLiteral("Indexed identifiers:"));
    available_sheep::IdentifierTrie trie;
    for (const auto& entry : entries) {
        QStringList ids;
        for (available_sheep::SymbolId id : entry.symbols) {
            ids.push_back(QString::number(id));
            trie.insert(entry.spelling, id);
        }
        lines.push_back(QStringLiteral("%1 -> %2")
                            .arg(QString::fromStdString(entry.spelling),
                                 ids.join(QStringLiteral(", "))));
    }

    const QString queryLabel = query.isEmpty() ? QStringLiteral("<empty>") : query;
    QStringList exactIds;
    for (available_sheep::SymbolId id : trie.findExact(query.toStdString()))
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
                        .arg(spellingsText(trie.findByPrefix(query.toStdString(), 10))));
    lines.push_back(QStringLiteral("Suggestions (up to 10, edit distance <= 2): %1")
                        .arg(spellingsText(trie.suggest(query.toStdString(), 10))));
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
    setWindowTitle(QStringLiteral("Available Sheep | Compiler Workbench"));
    resize(1440, 900);
    setMinimumSize(980, 640);
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: #0b1020; color: #e6eaf2; "
        "font-family: 'Segoe UI', 'Inter', sans-serif; font-size: 13px; }"
        "QLabel#brandMark { background: #6d7cff; color: white; border-radius: 10px; "
        "font-size: 17px; font-weight: 800; min-width: 42px; max-width: 42px; "
        "min-height: 42px; max-height: 42px; qproperty-alignment: AlignCenter; }"
        "QLabel#appTitle { font-size: 19px; font-weight: 700; color: #f5f7ff; }"
        "QLabel#appSubtitle, QLabel#mutedLabel { color: #8994ad; }"
        "QGroupBox { background: #11182a; border: 1px solid #252f47; "
        "border-radius: 12px; margin-top: 12px; padding: 18px 14px 14px; "
        "font-weight: 650; color: #dce3f2; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; "
        "color: #aab5cd; }"
        "QPlainTextEdit { background: #0c1221; color: #dfe7f7; border: 1px solid #28334d; "
        "border-radius: 8px; padding: 12px; selection-background-color: #394b83; }"
        "QListWidget { background: #0c1221; color: #dfe7f7; border: 1px solid #28334d; "
        "border-radius: 8px; padding: 6px; outline: 0; }"
        "QListWidget::item { padding: 10px 9px; border-bottom: 1px solid #202a40; }"
        "QListWidget::item:selected { background: #202c49; color: #f1f3fb; }"
        "QPlainTextEdit:focus, QLineEdit:focus, QSpinBox:focus { border-color: #7285ff; }"
        "QLineEdit, QSpinBox { background: #0c1221; color: #e6eaf2; "
        "border: 1px solid #28334d; border-radius: 7px; padding: 7px 9px; }"
        "QCheckBox { spacing: 9px; color: #c3cce0; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; "
        "border: 1px solid #52607c; background: #0c1221; }"
        "QCheckBox::indicator:checked { background: #7182ff; border-color: #7182ff; }"
        "QPushButton { background: #1b2438; color: #cbd4e8; border: 1px solid #303c56; "
        "border-radius: 8px; padding: 9px 14px; font-weight: 650; }"
        "QPushButton:hover { background: #26324b; }"
        "QPushButton:disabled { color: #69758e; background: #151c2d; }"
        "QPushButton#primaryButton { background: #7182ff; border: 1px solid #8593ff; "
        "color: white; padding: 10px 18px; }"
        "QPushButton#primaryButton:hover { background: #8190ff; }"
        "QTabWidget::pane { border: 1px solid #252f47; border-radius: 9px; "
        "background: #11182a; top: -1px; }"
        "QTabBar::tab { background: #0e1525; color: #8490a9; border: 1px solid #252f47; "
        "padding: 9px 15px; margin-right: 4px; border-top-left-radius: 7px; "
        "border-top-right-radius: 7px; }"
        "QTabBar::tab:selected { background: #182239; color: #e7ebf6; "
        "border-bottom-color: #7182ff; }"
        "QFrame#summaryCard { background: #11182a; border: 1px solid #252f47; "
        "border-radius: 9px; }"
        "QLabel#summaryTitle { color: #8793ad; font-size: 11px; font-weight: 600; }"
        "QLabel#summaryValue { color: #eef1fa; font-size: 16px; font-weight: 700; }"
        "QSplitter::handle { background: #0b1020; width: 10px; }"
        "QStatusBar { background: #0b1020; color: #8793ad; border-top: 1px solid #202a40; }"));

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(22, 18, 22, 14);
    rootLayout->setSpacing(14);

    auto* header = new QHBoxLayout();
    header->setSpacing(13);
    auto* brandMark = new QLabel(QStringLiteral("AS"), central);
    brandMark->setObjectName(QStringLiteral("brandMark"));
    header->addWidget(brandMark);
    auto* titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(2);
    auto* title = new QLabel(QStringLiteral("Compiler workbench"), central);
    title->setObjectName(QStringLiteral("appTitle"));
    auto* subtitle = new QLabel(
        QStringLiteral("Explore optimization passes and the structures behind them"),
        central);
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    titleColumn->addWidget(title);
    titleColumn->addWidget(subtitle);
    header->addLayout(titleColumn, 1);
    auto* shortcutHint = new QLabel(QStringLiteral("Ctrl + Enter"), central);
    shortcutHint->setObjectName(QStringLiteral("mutedLabel"));
    header->addWidget(shortcutHint);
    runButton_ = new QPushButton(QStringLiteral("Run pipeline"), central);
    runButton_->setObjectName(QStringLiteral("primaryButton"));
    runButton_->setDefault(true);
    header->addWidget(runButton_);
    rootLayout->addLayout(header);

    auto* workbench = new QSplitter(Qt::Horizontal, central);
    workbench->setChildrenCollapsible(false);
    workbench->setHandleWidth(10);

    sourceGroup_ = new QGroupBox(QStringLiteral("01  SOURCE"), workbench);
    auto* sourceLayout = new QVBoxLayout(sourceGroup_);
    sourceLayout->setContentsMargins(15, 17, 15, 14);
    sourceLayout->setSpacing(10);
    auto* editorToolbar = new QHBoxLayout();
    auto* fileName = new QLabel(QStringLiteral("main.sheep"), sourceGroup_);
    fileName->setStyleSheet(QStringLiteral("font-weight: 700; color: #d7def0;"));
    auto* languageHint = new QLabel(QStringLiteral("C-like  ·  Int32"), sourceGroup_);
    languageHint->setObjectName(QStringLiteral("mutedLabel"));
    editorToolbar->addWidget(fileName);
    editorToolbar->addStretch();
    editorToolbar->addWidget(languageHint);
    sourceLayout->addLayout(editorToolbar);

    sourceEditor_ = new QPlainTextEdit(sourceGroup_);
    sourceEditor_->setObjectName(QStringLiteral("sourceEditor"));
    sourceEditor_->setPlaceholderText(
        QStringLiteral("Write a program in the supported integer language..."));
    sourceEditor_->setPlainText(
        QStringLiteral("int value = 2 + 3 * 4;\n"
                       "print(value);\n"));
    QFont editorFont(QStringLiteral("JetBrains Mono"));
    if (!editorFont.exactMatch()) editorFont = QFont(QStringLiteral("Monospace"));
    editorFont.setStyleHint(QFont::Monospace);
    editorFont.setPointSize(10);
    sourceEditor_->setFont(editorFont);
    sourceEditor_->setTabStopDistance(
        4 * sourceEditor_->fontMetrics().horizontalAdvance(' '));
    sourceLayout->addWidget(sourceEditor_, 1);

    auto* optionsGroup = new QGroupBox(QStringLiteral("Optimization passes"), sourceGroup_);
    auto* optionsLayout = new QGridLayout(optionsGroup);
    optionsLayout->setContentsMargins(12, 14, 12, 12);
    optionsLayout->setHorizontalSpacing(14);
    optionsLayout->setVerticalSpacing(12);
    const std::array<const char*, kPassCount> passLabels{
        "Constant folding",
        "Constant propagation",
        "Common subexpression elimination",
        "Dead-code elimination"};
    for (int i = 0; i < kPassCount; ++i) {
        passOptions_[i] = new QCheckBox(QString::fromLatin1(passLabels[i]), optionsGroup);
        passOptions_[i]->setChecked(true);
        optionsLayout->addWidget(passOptions_[i], i / 2, i % 2);
    }
    auto* iterationLabel = new QLabel(QStringLiteral("Max fixed-point iterations"), optionsGroup);
    iterationLimit_ = new QSpinBox(optionsGroup);
    iterationLimit_->setRange(0, 100000);
    iterationLimit_->setValue(10);
    auto* iterationRow = new QHBoxLayout();
    iterationRow->addWidget(iterationLabel);
    iterationRow->addStretch();
    iterationRow->addWidget(iterationLimit_);
    optionsLayout->addLayout(iterationRow, 2, 0, 1, 2);
    optionsGroup->setToolTip(
        QStringLiteral("Choose enabled passes and the maximum fixed-point iterations."));
    sourceLayout->addWidget(optionsGroup);
    workbench->addWidget(sourceGroup_);

    auto* outputPane = new QWidget(workbench);
    auto* outputLayout = new QVBoxLayout(outputPane);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(10);
    auto* summaryRow = new QHBoxLayout();
    summaryRow->setSpacing(9);
    summaryRow->addWidget(makeSummaryCard(QStringLiteral("PIPELINE"), runSummary_, outputPane));
    summaryRow->addWidget(
        makeSummaryCard(QStringLiteral("INSTRUCTIONS"), instructionSummary_, outputPane));
    summaryRow->addWidget(
        makeSummaryCard(QStringLiteral("ITERATIONS"), iterationSummary_, outputPane));
    outputLayout->addLayout(summaryRow);

    auto* outputTabs = new QTabWidget(outputPane);

    auto* irSplitter = new QSplitter(Qt::Horizontal, outputTabs);
    irSplitter->setChildrenCollapsible(false);
    auto* initialPane = new QWidget(irSplitter);
    auto* initialLayout = new QVBoxLayout(initialPane);
    initialLayout->setContentsMargins(9, 9, 9, 9);
    auto* initialTitle = new QLabel(QStringLiteral("INITIAL IR"), initialPane);
    initialTitle->setObjectName(QStringLiteral("mutedLabel"));
    initialLayout->addWidget(initialTitle);
    initialIrView_ = makeReadOnlyView(initialPane);
    initialIrView_->setFont(editorFont);
    initialLayout->addWidget(initialIrView_);

    auto* optimizedPane = new QWidget(irSplitter);
    auto* optimizedLayout = new QVBoxLayout(optimizedPane);
    optimizedLayout->setContentsMargins(9, 9, 9, 9);
    auto* optimizedTitle = new QLabel(QStringLiteral("OPTIMIZED IR"), optimizedPane);
    optimizedTitle->setObjectName(QStringLiteral("mutedLabel"));
    optimizedLayout->addWidget(optimizedTitle);
    optimizedIrView_ = makeReadOnlyView(optimizedPane);
    optimizedIrView_->setFont(editorFont);
    optimizedLayout->addWidget(optimizedIrView_);
    outputTabs->addTab(irSplitter, QStringLiteral("IR"));

    diagnosticsView_ = new QListWidget(outputTabs);
    diagnosticsView_->setWordWrap(true);
    outputTabs->addTab(diagnosticsView_, QStringLiteral("Diagnostics"));

    resultsView_ = makeReadOnlyView(outputTabs);
    resultsView_->setFont(editorFont);
    outputTabs->addTab(resultsView_, QStringLiteral("Results"));

    auto* runPane = new QWidget(outputTabs);
    auto* runLayout = new QVBoxLayout(runPane);
    runLayout->setContentsMargins(12, 12, 12, 12);
    runLayout->setSpacing(10);
    auto* runDescription = new QLabel(
        QStringLiteral("Execute the program and inspect its printed output. Programs that call "
                       "input() consume values from the input field in order."),
        runPane);
    runDescription->setObjectName(QStringLiteral("mutedLabel"));
    runDescription->setWordWrap(true);
    runLayout->addWidget(runDescription);
    runControls_ = new QWidget(runPane);
    auto* runControlsLayout = new QHBoxLayout(runControls_);
    runControlsLayout->setContentsMargins(0, 0, 0, 0);
    runControlsLayout->setSpacing(9);
    programVersion_ = new QComboBox(runControls_);
    programVersion_->addItem(QStringLiteral("Optimized IR"), false);
    programVersion_->addItem(QStringLiteral("Initial IR"), true);
    runControlsLayout->addWidget(programVersion_);
    runtimeInput_ = new QLineEdit(runControls_);
    runtimeInput_->setPlaceholderText(QStringLiteral("Input values, e.g. 12, -3, 5"));
    runtimeInput_->setToolTip(
        QStringLiteral("Comma- or whitespace-separated signed 32-bit integers, consumed by input()."));
    runControlsLayout->addWidget(runtimeInput_, 1);
    auto* executeButton = new QPushButton(QStringLiteral("Run program"), runControls_);
    executeButton->setObjectName(QStringLiteral("primaryButton"));
    runControlsLayout->addWidget(executeButton);
    runLayout->addWidget(runControls_);
    runtimeOutput_ = makeReadOnlyView(runPane);
    runtimeOutput_->setFont(editorFont);
    runtimeOutput_->setPlaceholderText(
        QStringLiteral("Program output and runtime status will appear here."));
    runLayout->addWidget(runtimeOutput_, 1);
    outputTabs->addTab(runPane, QStringLiteral("Run program"));

    auto* structureTabs = new QTabWidget(outputTabs);
    dagView_ = makeGraphView(structureTabs);
    auto* dagPane = new QWidget(structureTabs);
    auto* dagLayout = new QVBoxLayout(dagPane);
    dagLayout->setContentsMargins(0, 0, 0, 0);
    auto* dagHint = new QLabel(
        QStringLiteral("Dependencies flow left to right · blue nodes are pure · purple nodes are effectful"),
        dagPane);
    dagHint->setObjectName(QStringLiteral("mutedLabel"));
    dagHint->setContentsMargins(10, 8, 10, 2);
    dagLayout->addWidget(dagHint);
    dagView_->setParent(dagPane);
    dagLayout->addWidget(dagView_, 1);
    structureTabs->addTab(dagPane, QStringLiteral("DAG"));

    avlView_ = makeGraphView(structureTabs);
    auto* avlPane = new QWidget(structureTabs);
    auto* avlLayout = new QVBoxLayout(avlPane);
    avlLayout->setContentsMargins(0, 0, 0, 0);
    auto* avlHint = new QLabel(
        QStringLiteral("Tree links show the actual AVL topology · amber nodes participated in rotations"),
        avlPane);
    avlHint->setObjectName(QStringLiteral("mutedLabel"));
    avlHint->setContentsMargins(10, 8, 10, 2);
    avlLayout->addWidget(avlHint);
    avlView_->setParent(avlPane);
    avlLayout->addWidget(avlView_, 1);
    structureTabs->addTab(avlPane, QStringLiteral("AVL"));
    auto* triePane = new QWidget(structureTabs);
    auto* trieLayout = new QVBoxLayout(triePane);
    trieLayout->setContentsMargins(10, 10, 10, 10);
    trieLayout->setSpacing(9);
    auto* trieQueryRow = new QHBoxLayout();
    auto* trieQueryLabel = new QLabel(QStringLiteral("Live identifier lookup"), triePane);
    trieQueryLabel->setObjectName(QStringLiteral("mutedLabel"));
    trieQueryRow->addWidget(trieQueryLabel);
    trieQuery_ = new QLineEdit(triePane);
    trieQuery_->setPlaceholderText(QStringLiteral("Type an identifier..."));
    trieQuery_->setToolTip(
        QStringLiteral("Exact matches, prefixes, and spelling suggestions update as you type."));
    trieQueryRow->addWidget(trieQuery_, 1);
    trieLayout->addLayout(trieQueryRow);
    trieView_ = makeReadOnlyView(triePane);
    trieView_->setFont(editorFont);
    trieLayout->addWidget(trieView_);
    structureTabs->addTab(triePane, QStringLiteral("Trie"));
    outputTabs->addTab(structureTabs, QStringLiteral("Data structures"));
    outputLayout->addWidget(outputTabs, 1);
    workbench->addWidget(outputPane);
    workbench->setStretchFactor(0, 4);
    workbench->setStretchFactor(1, 6);
    workbench->setSizes({530, 820});
    rootLayout->addWidget(workbench, 1);

    setCentralWidget(central);
    connect(runButton_, &QPushButton::clicked, this, [this] { compileSource(); });
    connect(executeButton, &QPushButton::clicked, this, [this] { runProgram(); });
    connect(runtimeInput_, &QLineEdit::returnPressed, this, [this] { runProgram(); });
    connect(diagnosticsView_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                navigateToDiagnostic(item->data(Qt::UserRole).toInt(),
                                     item->data(Qt::UserRole + 1).toInt());
            });
    auto* runShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(runShortcut, &QShortcut::activated, this, [this] { compileSource(); });
    auto* runEnterShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Enter")), this);
    connect(runEnterShortcut, &QShortcut::activated, this, [this] { compileSource(); });
    connect(trieQuery_, &QLineEdit::textChanged, this, [this] { updateTrieView(); });
    sourceEditor_->setFocus();

    const QString waiting = QStringLiteral(
        "Ready when you are.\n\nEdit the sample, choose optimization passes, then run the pipeline "
        "to inspect generated IR and diagnostics.");
    initialIrView_->setPlainText(waiting);
    optimizedIrView_->setPlainText(QStringLiteral("Optimized output will appear here."));
    auto* initialDiagnostic = new QListWidgetItem(waiting, diagnosticsView_);
    initialDiagnostic->setFlags(Qt::NoItemFlags);
    resultsView_->setPlainText(
        QStringLiteral("Pass statistics and before/after metrics will appear here."));
    showGraphMessage(dagView_, QStringLiteral("The expression DAG will appear here after a run."));
    showGraphMessage(avlView_, QStringLiteral("AVL topology will appear here after a run."));
    trieView_->setPlainText(QStringLiteral("Indexed names and live lookup results will appear here."));
    runSummary_->setText(QStringLiteral("Ready"));
    instructionSummary_->setText(QStringLiteral("—"));
    iterationSummary_->setText(QStringLiteral("—"));
    runControls_->setEnabled(false);
    runtimeOutput_->setPlainText(QStringLiteral("Compile a program before running it."));
    statusBar()->showMessage(QStringLiteral("Ready · edit the sample or enter your own program"));
}

void MainWindow::compileSource() {
    if (!sourceGroup_->isEnabled() || !runButton_->isEnabled()) return;

    initialIrView_->clear();
    optimizedIrView_->clear();
    diagnosticsView_->clear();
    runControls_->setEnabled(false);
    runtimeOutput_->clear();
    hasInitialProgram_ = false;
    hasOptimizedProgram_ = false;
    resultsView_->clear();
    showGraphMessage(dagView_, QStringLiteral("Building expression DAG…"));
    showGraphMessage(avlView_, QStringLiteral("Building AVL tree…"));
    trieView_->setPlainText(QStringLiteral("Querying identifier trie..."));
    runSummary_->setText(QStringLiteral("Running…"));
    instructionSummary_->setText(QStringLiteral("—"));
    iterationSummary_->setText(QStringLiteral("—"));

    PassOptions options;
    for (int i = 0; i < kPassCount; ++i)
        options.enabled[i] = passOptions_[i]->isChecked();
    options.maxIterations = iterationLimit_->value();
    const std::string source = sourceEditor_->toPlainText().toStdString();

    sourceGroup_->setEnabled(false);
    trieQuery_->setEnabled(false);
    runButton_->setEnabled(false);
    runButton_->setText(QStringLiteral("Running..."));
    statusBar()->showMessage(QStringLiteral("Compiling source and optimizing IR…"));
    auto* watcher = new QFutureWatcher<PipelineResult>(this);
    connect(watcher, &QFutureWatcher<PipelineResult>::finished, this,
            [this, watcher] {
                const PipelineResult result = watcher->result();
                watcher->deleteLater();
                sourceGroup_->setEnabled(true);
                trieQuery_->setEnabled(true);
                runButton_->setEnabled(true);
                runButton_->setText(QStringLiteral("Run pipeline"));
                showPipelineResult(result);
            });
    watcher->setFuture(QtConcurrent::run(
        [source, options] { return runPipeline(source, options); }));
}

void MainWindow::showPipelineResult(const PipelineResult& result) {
    trieEntries_ = result.trieEntries;
    std::vector<Diagnostic> diagnostics = result.frontend.diagnostics;
    programVersion_->clear();
    if (result.frontend.success) {
        initialProgram_ = result.frontend.program;
        hasInitialProgram_ = true;
        programVersion_->addItem(QStringLiteral("Initial IR"), true);
        initialIrView_->setPlainText(QString::fromStdString(toString(result.frontend.program)));
        const bool optimizationSucceeded =
            result.error.empty() &&
            result.optimization.outcome == RunOutcome::Optimized;
        diagnostics.insert(diagnostics.end(),
                           result.optimization.diagnostics.begin(),
                           result.optimization.diagnostics.end());
        if (!optimizationSucceeded) {
            optimizedIrView_->setPlainText(
                QStringLiteral("Unavailable: optimization failed."));
            resultsView_->setPlainText(
                result.error.empty()
                    ? QStringLiteral("Optimization failed. See Diagnostics.")
                    : QStringLiteral("Optimization failed: %1")
                          .arg(QString::fromStdString(result.error)));
            runSummary_->setText(QStringLiteral("Pipeline failed"));
            instructionSummary_->setText(QStringLiteral("—"));
            iterationSummary_->setText(QStringLiteral("—"));
            showGraphMessage(dagView_, QStringLiteral("Unavailable: optimization failed."));
            showGraphMessage(avlView_, QStringLiteral("Unavailable: optimization failed."));
            programVersion_->setCurrentIndex(0);
        } else {
            optimizedProgram_ = result.optimization.optimizedIr;
            hasOptimizedProgram_ = true;
            programVersion_->insertItem(0, QStringLiteral("Optimized IR"), false);
            optimizedIrView_->setPlainText(
                QString::fromStdString(toString(result.optimization.optimizedIr)));
            resultsView_->setPlainText(optimizationText(result.optimization));
            runSummary_->setText(
                result.optimization.outcome == RunOutcome::Optimized
                    ? QStringLiteral("Optimized")
                    : QStringLiteral("Optimization failed"));
            instructionSummary_->setText(
                QStringLiteral("%1 → %2")
                    .arg(result.optimization.before.instructions)
                    .arg(result.optimization.after.instructions));
            iterationSummary_->setText(
                QStringLiteral("%1 run%2")
                    .arg(result.optimization.iterationsRun)
                    .arg(result.optimization.iterationsRun == 1 ? QString() :
                                                                  QStringLiteral("s")));
            drawDag(dagView_, result.optimization);
            drawAvl(avlView_, result.optimization);
        }
    } else {
        runSummary_->setText(QStringLiteral("Compile failed"));
        instructionSummary_->setText(QStringLiteral("—"));
        iterationSummary_->setText(QStringLiteral("—"));
        initialIrView_->setPlainText(
            QStringLiteral("No initial IR was produced because frontend validation failed."));
        optimizedIrView_->setPlainText(
            QStringLiteral("Unavailable: source did not produce a valid initial IR."));
        resultsView_->setPlainText(
            result.error.empty()
                ? QStringLiteral("Compilation failed. See Diagnostics. No optimization was run.")
                : QStringLiteral("Pipeline failed: %1").arg(QString::fromStdString(result.error)));
        showGraphMessage(dagView_, QStringLiteral("No DAG was produced."));
        showGraphMessage(avlView_, QStringLiteral("No AVL tree was produced."));
    }
    if (!result.error.empty()) {
        diagnostics.push_back(
            Diagnostic{Severity::Error,
                       result.frontend.success ? DiagStage::Optimization : DiagStage::Semantic,
                       result.error, 0, 0});
    }
    runControls_->setEnabled(hasInitialProgram_);
    runtimeOutput_->setPlainText(
        hasInitialProgram_ ? QStringLiteral("Ready to run.")
                           : QStringLiteral("Compile a valid program before running it."));
    showDiagnostics(diagnostics);
    updateTrieView();
    statusBar()->showMessage(
        result.frontend.success && result.error.empty() &&
                result.optimization.outcome == RunOutcome::Optimized
            ? QStringLiteral("Run complete · inspect IR, diagnostics, and data structures")
            : QStringLiteral("Pipeline failed · review diagnostics"),
        6000);
}

void MainWindow::updateTrieView() {
    if (trieView_ != nullptr && trieQuery_ != nullptr)
        trieView_->setPlainText(trieText(trieEntries_, trieQuery_->text()));
}

void MainWindow::runProgram() {
    if (!runControls_->isEnabled()) return;

    QStringList inputTokens;
    const QString inputText = runtimeInput_->text().trimmed();
    if (!inputText.isEmpty()) {
        inputTokens = inputText.split(
            QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    }

    std::vector<std::int32_t> inputs;
    inputs.reserve(static_cast<std::size_t>(inputTokens.size()));
    for (const QString& token : inputTokens) {
        bool valid = false;
        const qlonglong value = token.toLongLong(&valid, 10);
        if (!valid || value < std::numeric_limits<std::int32_t>::min() ||
            value > std::numeric_limits<std::int32_t>::max()) {
            runtimeOutput_->setPlainText(
                QStringLiteral("Invalid input value: \"%1\". Enter signed 32-bit integers.")
                    .arg(token));
            return;
        }
        inputs.push_back(static_cast<std::int32_t>(value));
    }

    const bool runInitial = programVersion_->currentData().toBool();
    if ((runInitial && !hasInitialProgram_) || (!runInitial && !hasOptimizedProgram_)) {
        runtimeOutput_->setPlainText(QStringLiteral("The selected program is not available."));
        return;
    }

    const RunResult result =
        run(runInitial ? initialProgram_ : optimizedProgram_, inputs);
    QStringList outputLines;
    for (std::int32_t value : result.output)
        outputLines.push_back(QString::number(value));

    QString status;
    switch (result.status) {
        case RunStatus::Ok:
            status = QStringLiteral("Finished successfully.");
            break;
        case RunStatus::DivisionByZero:
            status = QStringLiteral("Runtime error: division or remainder by zero.");
            break;
        case RunStatus::InputExhausted:
            status = QStringLiteral("Runtime error: the program requested more input values.");
            break;
        case RunStatus::InvalidProgram:
            status = QStringLiteral("Runtime error: the selected IR program is invalid.");
            break;
    }
    runtimeOutput_->setPlainText(
        QStringLiteral("Output:\n%1\n\n%2")
            .arg(outputLines.isEmpty() ? QStringLiteral("(no output)")
                                       : outputLines.join(QLatin1Char('\n')),
                 status));
}

void MainWindow::showDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    diagnosticsView_->clear();
    if (diagnostics.empty()) {
        auto* item = new QListWidgetItem(QStringLiteral("No diagnostics."), diagnosticsView_);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const Diagnostic& diagnostic : diagnostics) {
        QString location;
        if (diagnostic.line > 0)
            location = QStringLiteral(" · %1:%2").arg(diagnostic.line).arg(diagnostic.column);
        const QString text =
            QStringLiteral("%1 · %2%3\n%4")
                .arg(severityName(diagnostic.severity), stageName(diagnostic.stage), location,
                     QString::fromStdString(diagnostic.message));
        auto* item = new QListWidgetItem(text, diagnosticsView_);
        item->setData(Qt::UserRole, diagnostic.line);
        item->setData(Qt::UserRole + 1, diagnostic.column);
        item->setToolTip(diagnostic.line > 0
                             ? QStringLiteral("Click to jump to line %1, column %2")
                                   .arg(diagnostic.line)
                                   .arg(diagnostic.column)
                             : QString::fromStdString(diagnostic.message));
        if (diagnostic.severity == Severity::Error)
            item->setForeground(QColor(QStringLiteral("#ff8b93")));
        else if (diagnostic.severity == Severity::Warning)
            item->setForeground(QColor(QStringLiteral("#f1c879")));
        else
            item->setForeground(QColor(QStringLiteral("#9eabc5")));
    }
}

void MainWindow::navigateToDiagnostic(int line, int column) {
    if (line <= 0 || column <= 0 || line > sourceEditor_->document()->blockCount()) return;

    QTextCursor cursor(sourceEditor_->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column - 1);
    cursor.select(QTextCursor::LineUnderCursor);
    sourceEditor_->setTextCursor(cursor);
    sourceEditor_->centerCursor();

    QTextEdit::ExtraSelection highlight;
    highlight.cursor = cursor;
    highlight.format.setBackground(QColor(QStringLiteral("#263456")));
    highlight.format.setProperty(QTextFormat::FullWidthSelection, true);
    sourceEditor_->setExtraSelections({highlight});
    sourceEditor_->setFocus();
}
