#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>

#include "DefUser.h"

namespace {

std::map<size_t, ParseResult> parse_program(
    std::initializer_list<std::string_view> lines) {
    Lexer lexer;
    size_t line_number = 1;
    for (std::string_view line : lines) {
        lexer.tokenize_line(line, line_number++);
    }

    Parser parser;
    parser.parser_tokenize_line(lexer.results());
    return parser.results();
}

}  // namespace

TEST(DefUserTest, BuildsSourcesAndUses) {
    const auto parsed = parse_program({
        "%a = add %x, 1",
        "%b = mul %a, 2",
        "%c = add %a, %b",
        "ret %c",
    });

    DefUser def_user({"%x"});
    def_user.analyze_def_use(parsed);

    const auto& results = def_user.results();
    ASSERT_EQ(results.size(), 4u);
    for (const auto& [id, result] : results) {
        SCOPED_TRACE(id);
        ASSERT_TRUE(result.info.has_value());
        EXPECT_FALSE(result.error.has_value());
    }

    const auto& first = results.at(0).info;
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(first->sources.size(), 2u);
    EXPECT_EQ(first->sources[0].kind, SourceKind::ExternalInput);
    EXPECT_EQ(first->sources[0].operand_index, 0u);
    EXPECT_FALSE(first->sources[0].defining_instruction.has_value());
    EXPECT_EQ(first->sources[1].kind, SourceKind::Constant);
    EXPECT_EQ(first->sources[1].operand_index, 1u);
    EXPECT_FALSE(first->sources[1].defining_instruction.has_value());
    EXPECT_EQ(first->dest_id, std::optional<InstructionId>{0});

    const auto& second = results.at(1).info;
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(second->sources.size(), 2u);
    EXPECT_EQ(second->sources[0].kind, SourceKind::Instruction);
    ASSERT_TRUE(second->sources[0].defining_instruction.has_value());
    EXPECT_EQ(*second->sources[0].defining_instruction, 0u);
    EXPECT_EQ(second->sources[1].kind, SourceKind::Constant);

    const auto& third = results.at(2).info;
    ASSERT_TRUE(third.has_value());
    ASSERT_EQ(third->sources.size(), 2u);
    EXPECT_EQ(third->sources[0].defining_instruction, std::optional<InstructionId>{0});
    EXPECT_EQ(third->sources[1].defining_instruction, std::optional<InstructionId>{1});

    const auto& fourth = results.at(3).info;
    ASSERT_TRUE(fourth.has_value());
    ASSERT_EQ(fourth->sources.size(), 1u);
    EXPECT_FALSE(fourth->dest_id.has_value());
    EXPECT_EQ(fourth->sources[0].defining_instruction, std::optional<InstructionId>{2});

    ASSERT_EQ(first->uses.size(), 2u);
    EXPECT_EQ(first->uses[0].instruction_index, 1u);
    EXPECT_EQ(first->uses[0].operand_index, 0u);
    EXPECT_EQ(first->uses[1].instruction_index, 2u);
    EXPECT_EQ(first->uses[1].operand_index, 0u);

    ASSERT_EQ(second->uses.size(), 1u);
    EXPECT_EQ(second->uses[0].instruction_index, 2u);
    EXPECT_EQ(second->uses[0].operand_index, 1u);

    ASSERT_EQ(third->uses.size(), 1u);
    EXPECT_EQ(third->uses[0].instruction_index, 3u);
    EXPECT_EQ(third->uses[0].operand_index, 0u);

    const auto& external_uses = def_user.external_input_uses();
    ASSERT_EQ(external_uses.at("%x").size(), 1u);
    EXPECT_EQ(external_uses.at("%x")[0].instruction_index, 0u);
    EXPECT_EQ(external_uses.at("%x")[0].operand_index, 0u);
}

TEST(DefUserTest, EmptyLinesDoNotConsumeInstructionIds) {
    const auto parsed = parse_program({
        "%a = add 1, 2",
        "",
        "# comment",
        "ret %a",
    });

    DefUser def_user({});
    def_user.analyze_def_use(parsed);

    const auto& results = def_user.results();
    ASSERT_EQ(results.size(), 2u);
    ASSERT_TRUE(results.at(0).info.has_value());
    ASSERT_TRUE(results.at(1).info.has_value());
    EXPECT_EQ(results.at(1).info->inst.opcode.lexeme, "ret");
    ASSERT_EQ(results.at(0).info->uses.size(), 1u);
    EXPECT_EQ(results.at(0).info->uses[0].instruction_index, 1u);
}

TEST(DefUserTest, RecordsRepeatedUsesAndRegisterReuse) {
    const auto parsed = parse_program({
        "%a = add 1, 2",
        "%a = mul %a, %a",
        "ret %a",
    });

    DefUser def_user({});
    def_user.analyze_def_use(parsed);

    const auto& results = def_user.results();
    ASSERT_EQ(results.size(), 3u);

    const auto& first = results.at(0).info;
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(first->uses.size(), 2u);
    EXPECT_EQ(first->uses[0].instruction_index, 1u);
    EXPECT_EQ(first->uses[0].operand_index, 0u);
    EXPECT_EQ(first->uses[1].instruction_index, 1u);
    EXPECT_EQ(first->uses[1].operand_index, 1u);

    const auto& second = results.at(1).info;
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(second->sources.size(), 2u);
    EXPECT_EQ(second->sources[0].defining_instruction, std::optional<InstructionId>{0});
    EXPECT_EQ(second->sources[1].defining_instruction, std::optional<InstructionId>{0});
    ASSERT_EQ(second->uses.size(), 1u);
    EXPECT_EQ(second->uses[0].instruction_index, 2u);
    EXPECT_EQ(second->uses[0].operand_index, 0u);
    ASSERT_TRUE(results.at(2).info.has_value());
    ASSERT_EQ(results.at(2).info->sources.size(), 1u);
    EXPECT_EQ(results.at(2).info->sources[0].defining_instruction,
              std::optional<InstructionId>{1});
}

TEST(DefUserTest, ReportsUndefinedValueAtOperand) {
    const auto parsed = parse_program({
        "%a = add %missing, 1",
    });

    DefUser def_user({});
    def_user.analyze_def_use(parsed);

    const auto& result = def_user.results().at(0);
    EXPECT_FALSE(result.info.has_value());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "Undefined value: %missing");
    EXPECT_EQ(result.error->location.line, 1u);
    EXPECT_EQ(result.error->location.column, 10u);
}

TEST(DefUserTest, PreservesParserErrorLocation) {
    const auto parsed = parse_program({
        "%a = add 1,",
    });

    DefUser def_user({});
    def_user.analyze_def_use(parsed);

    const auto& result = def_user.results().at(0);
    EXPECT_FALSE(result.info.has_value());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->location.line, 1u);
    EXPECT_EQ(result.error->location.column, 12u);
    EXPECT_NE(result.error->message.find("Expected operand at end of input"),
              std::string::npos);
    ASSERT_TRUE(parsed.at(1).error.has_value());
    EXPECT_EQ(result.error->location.line, parsed.at(1).error->location.line);
    EXPECT_EQ(result.error->location.column, parsed.at(1).error->location.column);
}

TEST(DefUserTest, EmptyProgramAndOnlyCommentsProduceNoInstructions) {
    DefUser def_user({"%x"});
    for (const auto& parsed : {parse_program({}),
                               parse_program({"", "  \t", "# comment"})}) {
        def_user.analyze_def_use(parsed);
        EXPECT_TRUE(def_user.results().empty());
        EXPECT_TRUE(def_user.external_input_uses().empty());
    }
}

TEST(DefUserTest, LoadStoreAndRetTrackUsesWithoutExtraDefinitions) {
    DefUser def_user({"%ptr"});
    def_user.analyze_def_use(parse_program({
        "%a = load %ptr",
        "store %a, %ptr",
        "ret %a",
    }));

    const auto& results = def_user.results();
    ASSERT_EQ(results.size(), 3u);
    for (const auto& [id, result] : results) {
        SCOPED_TRACE(id);
        ASSERT_TRUE(result.info.has_value());
        EXPECT_FALSE(result.error.has_value());
    }
    const auto& load = *results.at(0).info;
    EXPECT_EQ(load.dest_id, std::optional<InstructionId>{0});
    ASSERT_EQ(load.uses.size(), 2u);
    EXPECT_EQ(load.uses[0].instruction_index, 1u);
    EXPECT_EQ(load.uses[0].operand_index, 0u);
    EXPECT_EQ(load.uses[1].instruction_index, 2u);
    EXPECT_EQ(load.uses[1].operand_index, 0u);

    const auto& store = *results.at(1).info;
    EXPECT_FALSE(store.dest_id.has_value());
    EXPECT_TRUE(store.uses.empty());
    ASSERT_EQ(store.sources.size(), 2u);
    EXPECT_EQ(store.sources[0].defining_instruction, std::optional<InstructionId>{0});
    EXPECT_EQ(store.sources[1].kind, SourceKind::ExternalInput);
    EXPECT_EQ(store.sources[1].operand_index, 1u);
    EXPECT_FALSE(results.at(2).info->dest_id.has_value());

    const auto& uses = def_user.external_input_uses().at("%ptr");
    ASSERT_EQ(uses.size(), 2u);
    EXPECT_EQ(uses[0].instruction_index, 0u);
    EXPECT_EQ(uses[0].operand_index, 0u);
    EXPECT_EQ(uses[1].instruction_index, 1u);
    EXPECT_EQ(uses[1].operand_index, 1u);
}

TEST(DefUserTest, RejectsSelfReferenceAndForwardReferenceWithoutPriorDefinition) {
    const std::vector<std::map<size_t, ParseResult>> programs = {
        parse_program({"%a = add %a, 1"}),
        parse_program({"%b = mul %a, 2", "%a = add 1, 2"}),
    };
    DefUser def_user({});
    for (const auto& parsed : programs) {
        def_user.analyze_def_use(parsed);
        ASSERT_EQ(def_user.results().size(), 1u);
        const auto& result = def_user.results().at(0);
        EXPECT_FALSE(result.info.has_value());
        ASSERT_TRUE(result.error.has_value());
        EXPECT_EQ(result.error->message, "Undefined value: %a");
        EXPECT_EQ(result.error->location.line, 1u);
        EXPECT_EQ(result.error->location.column, 10u);
    }
}

TEST(DefUserTest, KeepsCompletedInstructionsAndStopsAtFirstError) {
    DefUser def_user({});
    def_user.analyze_def_use(parse_program({
        "# comment",
        "%a = add 1, 2",
        "",
        "%b = mul %missing, 3",
        "ret %a",
    }));

    const auto& results = def_user.results();
    ASSERT_EQ(results.size(), 2u);
    ASSERT_TRUE(results.at(0).info.has_value());
    EXPECT_FALSE(results.at(0).error.has_value());
    EXPECT_TRUE(results.at(0).info->uses.empty());
    const auto& failed = results.at(1);
    EXPECT_FALSE(failed.info.has_value());
    ASSERT_TRUE(failed.error.has_value());
    EXPECT_EQ(failed.error->location.line, 4u);
    EXPECT_EQ(failed.error->location.column, 10u);
    EXPECT_EQ(failed.error->message, "Undefined value: %missing");
}

TEST(DefUserTest, RepeatedCallsClearDefinitionsResultsAndExternalUses) {
    DefUser def_user({"%x"});
    def_user.analyze_def_use(parse_program({"%a = add %x, 1", "ret %a"}));
    ASSERT_EQ(def_user.results().size(), 2u);
    ASSERT_EQ(def_user.external_input_uses().at("%x").size(), 1u);

    // A definition from the first call must not satisfy a use in the next call.
    def_user.analyze_def_use(parse_program({"ret %a"}));
    ASSERT_EQ(def_user.results().size(), 1u);
    ASSERT_TRUE(def_user.results().at(0).error.has_value());
    EXPECT_EQ(def_user.results().at(0).error->message, "Undefined value: %a");
    EXPECT_TRUE(def_user.external_input_uses().empty());

    // A later successful call must remove the error and start external uses anew.
    def_user.analyze_def_use(parse_program({"ret %x"}));
    ASSERT_EQ(def_user.results().size(), 1u);
    EXPECT_FALSE(def_user.results().at(0).error.has_value());
    ASSERT_TRUE(def_user.results().at(0).info.has_value());
    ASSERT_EQ(def_user.external_input_uses().at("%x").size(), 1u);
    EXPECT_EQ(def_user.external_input_uses().at("%x")[0].instruction_index, 0u);
}

TEST(DefUserTest, RejectsIllegalOperandKindWithItsOwnLocation) {
    // Parser normally rejects this kind, so construct the input to exercise
    // DefUser's defensive check directly.
    const Instruction instruction{
        std::nullopt,
        Token{TokenKind::Identifier, "ret", {7, 1}},
        {Token{TokenKind::Identifier, "bad", {7, 5}}},
    };
    const std::map<size_t, ParseResult> parsed{
        {7, ParseResult{instruction, std::nullopt}},
    };

    DefUser def_user({});
    def_user.analyze_def_use(parsed);
    ASSERT_EQ(def_user.results().size(), 1u);
    const auto& result = def_user.results().at(0);
    EXPECT_FALSE(result.info.has_value());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "Illegal source kind: bad");
    EXPECT_EQ(result.error->location.line, 7u);
    EXPECT_EQ(result.error->location.column, 5u);
}

