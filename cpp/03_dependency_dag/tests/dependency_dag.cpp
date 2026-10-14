#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DependencyDAG.h"

namespace {

DefUseAnalysisResult analyze_def_use(
    std::initializer_list<std::string_view> lines,
    std::unordered_set<std::string> external_inputs = {}) {
    Lexer lexer;
    size_t line_number = 1;
    for (const auto line : lines) {
        lexer.tokenize_line(line, line_number++);
    }

    Parser parser;
    parser.parser_tokenize_line(lexer.results());

    DefUser def_user(std::move(external_inputs));
    def_user.analyze_def_use(parser.results());
    return def_user.results();
}

const DAGAnalysisInfo& require_info(const DependencyDAG& dag) {
    const auto& result = dag.results();
    EXPECT_FALSE(result.error.has_value());
    EXPECT_TRUE(result.info.has_value());
    return *result.info;
}

}  // namespace

// Instructions:
//   0: %a = add %x, 1       start=0, finish=1
//   1: %b = mul %y, 2       start=0, finish=3
//   2: %c = add %a, %b      start=3, finish=4
//   3: %d = mul %a, 3       start=1, finish=4
//   4: %e = add %c, %d      start=4, finish=5
// Expected critical path: 1 -> 2 -> 4; total_cycles=5.
TEST(DependencyDAGTest, BuildsGraphAndComputesInternalOpcodeLatencies) {
    const auto def_use = analyze_def_use(
        {"%a = add %x, 1", "%b = mul %y, 2", "%c = add %a, %b",
         "%d = mul %a, 3", "%e = add %c, %d"},
        {"%x", "%y"});

    DependencyDAG dag;
    dag.analyze(def_use);
    const auto& info = require_info(dag);

    ASSERT_EQ(info.graph.size(), 5u);
    EXPECT_EQ(info.graph[0].predecessors, std::vector<InstructionId>{});
    EXPECT_EQ(info.graph[0].successors,
              (std::vector<InstructionId>{2, 3}));
    EXPECT_EQ(info.graph[1].predecessors, std::vector<InstructionId>{});
    EXPECT_EQ(info.graph[1].successors, std::vector<InstructionId>{2});
    EXPECT_EQ(info.graph[2].predecessors,
              (std::vector<InstructionId>{0, 1}));
    EXPECT_EQ(info.graph[2].successors, std::vector<InstructionId>{4});
    EXPECT_EQ(info.graph[3].predecessors, std::vector<InstructionId>{0});
    EXPECT_EQ(info.graph[3].successors, std::vector<InstructionId>{4});
    EXPECT_EQ(info.graph[4].predecessors,
              (std::vector<InstructionId>{2, 3}));
    EXPECT_EQ(info.graph[4].successors, std::vector<InstructionId>{});

    ASSERT_EQ(info.timings.size(), 5u);
    EXPECT_EQ(info.timings[0].start, 0u);
    EXPECT_EQ(info.timings[0].finish, 1u);
    EXPECT_EQ(info.timings[1].start, 0u);
    EXPECT_EQ(info.timings[1].finish, 3u);
    EXPECT_EQ(info.timings[2].start, 3u);
    EXPECT_EQ(info.timings[2].finish, 4u);
    EXPECT_EQ(info.timings[3].start, 1u);
    EXPECT_EQ(info.timings[3].finish, 4u);
    EXPECT_EQ(info.timings[4].start, 4u);
    EXPECT_EQ(info.timings[4].finish, 5u);
    EXPECT_EQ(info.timings[4].critical_predecessor,
              std::optional<InstructionId>{2});
    EXPECT_EQ(info.critical_path,
              (std::vector<InstructionId>{1, 2, 4}));
    EXPECT_EQ(info.total_cycles, 5u);
}

// Instructions:
//   0: %a = add 1, 2         start=0, finish=1
//   1: %b = mul %a, %a       start=1, finish=4
// The repeated operand creates one graph edge: 0 -> 1.
// Expected critical path: 0 -> 1; total_cycles=4.
TEST(DependencyDAGTest, DeduplicatesRepeatedOperandDependencies) {
    const auto def_use = analyze_def_use({
        "%a = add 1, 2", "%b = mul %a, %a"});

    DependencyDAG dag;
    dag.analyze(def_use);
    const auto& info = require_info(dag);

    ASSERT_EQ(info.graph.size(), 2u);
    EXPECT_EQ(info.graph[0].predecessors, std::vector<InstructionId>{});
    EXPECT_EQ(info.graph[0].successors, std::vector<InstructionId>{1});
    EXPECT_EQ(info.graph[1].predecessors, std::vector<InstructionId>{0});
    EXPECT_EQ(info.graph[1].successors, std::vector<InstructionId>{});
    EXPECT_EQ(info.timings[0].finish, 1u);
    EXPECT_EQ(info.timings[1].start, 1u);
    EXPECT_EQ(info.timings[1].finish, 4u);
    EXPECT_EQ(info.critical_path,
              (std::vector<InstructionId>{0, 1}));
    EXPECT_EQ(info.total_cycles, 4u);
}

// Instructions:
//   0: %a = add 1, 2         start=0, finish=1
//   1: %b = add 3, 4         start=0, finish=1
//   2: %c = add %b, %a       start=1, finish=2
// Both predecessors finish at 1, so predecessor 0 is selected by ID.
// Expected critical path: 0 -> 2; total_cycles=2.
TEST(DependencyDAGTest, ChoosesSmallestCriticalPredecessorOnTie) {
    const auto def_use = analyze_def_use({
        "%a = add 1, 2", "%b = add 3, 4", "%c = add %b, %a"});

    DependencyDAG dag;
    dag.analyze(def_use);
    const auto& info = require_info(dag);

    EXPECT_EQ(info.graph[2].predecessors,
              (std::vector<InstructionId>{0, 1}));
    EXPECT_EQ(info.timings[2].critical_predecessor,
              std::optional<InstructionId>{0});
    EXPECT_EQ(info.critical_path,
              (std::vector<InstructionId>{0, 2}));
    EXPECT_EQ(info.total_cycles, 2u);
}

// Instructions:
//   0: %a = add 1, 2         start=0, finish=1
//   1: %b = mul 3, 4         start=0, finish=3
// The instructions are independent, so the critical path is node 1.
// Expected critical path: 1; total_cycles=3.
TEST(DependencyDAGTest, KeepsIndependentNodesAndUsesOpcodeLatency) {
    const auto def_use = analyze_def_use({
        "%a = add 1, 2", "%b = mul 3, 4"});

    DependencyDAG dag;
    dag.analyze(def_use);
    const auto& info = require_info(dag);

    EXPECT_TRUE(info.graph[0].predecessors.empty());
    EXPECT_TRUE(info.graph[0].successors.empty());
    EXPECT_TRUE(info.graph[1].predecessors.empty());
    EXPECT_TRUE(info.graph[1].successors.empty());
    EXPECT_EQ(info.timings[0].start, 0u);
    EXPECT_EQ(info.timings[0].finish, 1u);
    EXPECT_EQ(info.timings[1].start, 0u);
    EXPECT_EQ(info.timings[1].finish, 3u);
    EXPECT_EQ(info.critical_path, (std::vector<InstructionId>{1}));
    EXPECT_EQ(info.total_cycles, 3u);
}

// Instructions:
//   0: %a = add 1, 2         start=0, finish=1
//   1: %a = mul %a, %a      start=1, finish=4
//   2: %b = add %a, 3       start=4, finish=5
// The second definition of %a is a distinct instruction node.
// Expected critical path: 0 -> 1 -> 2; total_cycles=5.
TEST(DependencyDAGTest, DistinguishesRepeatedDefinitions) {
    const auto def_use = analyze_def_use({
        "%a = add 1, 2", "%a = mul %a, %a", "%b = add %a, 3"});

    DependencyDAG dag;
    dag.analyze(def_use);
    const auto& info = require_info(dag);

    EXPECT_EQ(info.graph[1].predecessors, std::vector<InstructionId>{0});
    EXPECT_EQ(info.graph[2].predecessors, std::vector<InstructionId>{1});
    EXPECT_EQ(info.timings[0].finish, 1u);
    EXPECT_EQ(info.timings[1].start, 1u);
    EXPECT_EQ(info.timings[1].finish, 4u);
    EXPECT_EQ(info.timings[2].start, 4u);
    EXPECT_EQ(info.timings[2].finish, 5u);
    EXPECT_EQ(info.critical_path,
              (std::vector<InstructionId>{0, 1, 2}));
    EXPECT_EQ(info.total_cycles, 5u);
}

// Instructions:
//   %a = load %x, store 1, %x, and ret %x
// These opcodes are rejected by DependencyDAG, so no start/finish times exist.
// Expected result: info is empty and error is "Illegal Opcode!".
TEST(DependencyDAGTest, RejectsUnsupportedOpcodes) {
    for (const auto lines : {
             std::initializer_list<std::string_view>{"%a = load %x"},
             std::initializer_list<std::string_view>{"store 1, %x"},
             std::initializer_list<std::string_view>{"ret %x"},
         }) {
        const auto def_use = analyze_def_use(lines, {"%x"});
        ASSERT_FALSE(def_use.error.has_value());

        DependencyDAG dag;
        dag.analyze(def_use);

        EXPECT_FALSE(dag.results().info.has_value());
        ASSERT_TRUE(dag.results().error.has_value());
        EXPECT_EQ(*dag.results().error, "Illegal Opcode!");
    }
}

// Instructions:
//   0: %a = add %missing, 1
// DefUser rejects the undefined input before DAG timing begins.
// Expected result: info is empty and no start/finish times are produced.
TEST(DependencyDAGTest, PropagatesUpstreamErrors) {
    const auto def_use = analyze_def_use({"%a = add %missing, 1"});
    ASSERT_TRUE(def_use.error.has_value());

    DependencyDAG dag;
    dag.analyze(def_use);

    EXPECT_FALSE(dag.results().info.has_value());
    ASSERT_TRUE(dag.results().error.has_value());
    EXPECT_EQ(*dag.results().error,
              "DefUse Error: Undefined value: %missing");
}

// Instructions: none.
// Expected result: an empty graph and timing table, an empty critical path,
// and total_cycles=0.
TEST(DependencyDAGTest, SupportsEmptyInput) {
    DependencyDAG dag;
    dag.analyze(analyze_def_use({}));

    ASSERT_FALSE(dag.results().error.has_value());
    ASSERT_TRUE(dag.results().info.has_value());
    EXPECT_TRUE(dag.results().info->graph.empty());
    EXPECT_TRUE(dag.results().info->timings.empty());
    EXPECT_TRUE(dag.results().info->critical_path.empty());
    EXPECT_EQ(dag.results().info->total_cycles, 0u);
}

// Calls:
//   1. %a = add 1, 2       start=0, finish=1; analysis succeeds.
//   2. ret 1               analysis fails; info must be empty.
//   3. empty input         analysis succeeds; all timing data is empty,
//                          and total_cycles=0.
TEST(DependencyDAGTest, ClearsStateBetweenCalls) {
    DependencyDAG dag;

    dag.analyze(analyze_def_use({"%a = add 1, 2"}));
    ASSERT_TRUE(dag.results().info.has_value());
    ASSERT_FALSE(dag.results().error.has_value());

    dag.analyze(analyze_def_use({"ret 1"}));
    EXPECT_FALSE(dag.results().info.has_value());
    ASSERT_TRUE(dag.results().error.has_value());

    dag.analyze(analyze_def_use({}));
    ASSERT_TRUE(dag.results().info.has_value());
    EXPECT_FALSE(dag.results().error.has_value());
    EXPECT_TRUE(dag.results().info->graph.empty());
    EXPECT_TRUE(dag.results().info->timings.empty());
    EXPECT_TRUE(dag.results().info->critical_path.empty());
    EXPECT_EQ(dag.results().info->total_cycles, 0u);
}

