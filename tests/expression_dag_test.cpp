#include "available_sheep/expression_dag.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

using namespace available_sheep;

namespace {

template <typename Exception, typename Callable>
void expectThrows(Callable&& callable) {
    bool threw = false;
    try {
        callable();
    } catch (const Exception&) {
        threw = true;
    }
    assert(threw);
}

void testCreationAndPureReuse() {
    ExpressionDag dag;
    const ExpressionKey constant{OperatorKind::Constant, 1, {}, {42}};
    const ExprId first = dag.addPureExpression(constant);
    const ExprId repeated = dag.addPureExpression(constant);

    assert(first == 1);
    assert(repeated == first);
    assert(dag.size() == 1);
    assert(dag.findEquivalentPure(constant) == first);

    const ExprId other = dag.addPureExpression(
        {OperatorKind::Constant, 1, {}, {43}});
    assert(other != first);
}

void testKeyFieldsAffectEquivalence() {
    ExpressionDag dag;
    const ExprId left = dag.addPureExpression(
        {OperatorKind::Identifier, 1, {}, {10}});
    const ExprId right = dag.addPureExpression(
        {OperatorKind::Identifier, 1, {}, {11}});

    const ExpressionKey original{OperatorKind::Subtract, 1, {left, right}, {}};
    const ExprId originalId = dag.addPureExpression(original);
    assert(dag.addPureExpression(original) == originalId);
    assert(dag.addPureExpression(
               {OperatorKind::Subtract, 2, {left, right}, {}}) != originalId);
    assert(dag.addPureExpression(
               {OperatorKind::Add, 1, {left, right}, {}}) != originalId);
    assert(dag.addPureExpression(
               {OperatorKind::Subtract, 1, {right, left}, {}}) != originalId);
    assert(dag.addPureExpression(
               {OperatorKind::Subtract, 1, {left, right}, {1}}) != originalId);
}

void testEffectfulExpressionsNeverMerge() {
    ExpressionDag dag;
    const ExpressionKey call{OperatorKind::Call, 1, {}, {7}};

    const ExprId first = dag.addEffectfulExpression(call);
    const ExprId second = dag.addEffectfulExpression(call);
    assert(first != second);
    assert(!dag.findEquivalentPure(call));

    const ExprId pure = dag.addPureExpression(call);
    assert(pure != first && pure != second);
    assert(dag.findEquivalentPure(call) == pure);
}

void testDependenciesAndUniqueUsers() {
    ExpressionDag dag;
    const ExprId value = dag.addPureExpression(
        {OperatorKind::Identifier, 1, {}, {1}});
    const ExprId user = dag.addPureExpression(
        {OperatorKind::Add, 1, {value, value}, {}});
    const ExprId effectfulUser = dag.addEffectfulExpression(
        {OperatorKind::Store, 1, {value}, {}});

    assert((dag.operandsOf(user) == std::vector<ExprId>{value, value}));
    assert((dag.usersOf(value) == std::vector<ExprId>{user, effectfulUser}));
    assert(dag.usersOf(user).empty());
}

void testInvalidIds() {
    ExpressionDag dag;
    expectThrows<std::invalid_argument>([&] {
        dag.addPureExpression({OperatorKind::Add, 1, {0}, {}});
    });
    expectThrows<std::invalid_argument>([&] {
        dag.addEffectfulExpression({OperatorKind::Add, 1, {99}, {}});
    });
    expectThrows<std::out_of_range>([&] { dag.operandsOf(0); });
    expectThrows<std::out_of_range>([&] { dag.usersOf(99); });
    expectThrows<std::invalid_argument>([&] {
        dag.findEquivalentPure({OperatorKind::Add, 1, {99}, {}});
    });
    assert(dag.size() == 0);
}

void testSnapshotIsCopiedAndDeterministic() {
    ExpressionDag dag;
    const ExprId operand = dag.addPureExpression(
        {OperatorKind::Constant, 1, {}, {5}});
    const ExprId pure = dag.addPureExpression(
        {OperatorKind::Negate, 1, {operand}, {}});
    const ExprId effectful = dag.addEffectfulExpression(
        {OperatorKind::Call, 1, {operand}, {2}});

    const auto firstSnapshot = dag.snapshot();
    const auto secondSnapshot = dag.snapshot();
    assert(firstSnapshot == secondSnapshot);
    assert(firstSnapshot.size() == 3);
    assert(firstSnapshot[0].id == operand);
    assert(firstSnapshot[1].id == pure && firstSnapshot[1].isPure);
    assert(firstSnapshot[2].id == effectful && !firstSnapshot[2].isPure);

    auto changedCopy = firstSnapshot;
    changedCopy[0].key.attributes[0] = 99;
    assert(dag.snapshot()[0].key.attributes[0] == 5);
}

}  // namespace

int main() {
    testCreationAndPureReuse();
    testKeyFieldsAffectEquivalence();
    testEffectfulExpressionsNeverMerge();
    testDependenciesAndUniqueUsers();
    testInvalidIds();
    testSnapshotIsCopiedAndDeterministic();
}
