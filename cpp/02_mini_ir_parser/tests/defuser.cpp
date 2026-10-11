#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <string_view>

#include "DefUser.h"

namespace {

std::map<size_t, ParseResult> parse_program(
    std::initializer_list<std::string_view> lines) {
    Lexer lexer;
    size_t line_number = 1;
    for (const auto line : lines) {
        lexer.tokenize_line(line, line_number++);
    }

    Parser parser;
    parser.parser_tokenize_line(lexer.results());
    return parser.results();
}

}  // namespace

TEST(DefUserTest, BuildsSourcesAndUses) {
    DefUser def_user({"%x"});
    def_user.analyze_def_use(parse_program({
        "%a = add %x, 1", "%b = mul %a, 2",
        "%c = add %a, %b", "ret %c",
    }));

    const auto& result = def_user.results();
    EXPECT_FALSE(result.error.has_value());
    ASSERT_EQ(result.info.size(), 4u);

    const auto& first = result.info.at(0);
    ASSERT_EQ(first.sources.size(), 2u);
    EXPECT_EQ(first.sources[0].kind, SourceKind::ExternalInput);
    EXPECT_EQ(first.sources[0].operand_index, 0u);
    EXPECT_EQ(first.sources[1].kind, SourceKind::Constant);
    EXPECT_EQ(first.sources[1].operand_index, 1u);
    EXPECT_EQ(first.dest_id, std::optional<InstructionId>{0});

    const auto& second = result.info.at(1);
    ASSERT_EQ(second.sources.size(), 2u);
    EXPECT_EQ(second.sources[0].kind, SourceKind::Instruction);
    EXPECT_EQ(second.sources[0].defining_instruction,
              std::optional<InstructionId>{0});
    EXPECT_EQ(second.sources[1].kind, SourceKind::Constant);

    EXPECT_EQ(result.info.at(2).sources[0].defining_instruction,
              std::optional<InstructionId>{0});
    EXPECT_EQ(result.info.at(2).sources[1].defining_instruction,
              std::optional<InstructionId>{1});
    EXPECT_FALSE(result.info.at(3).dest_id.has_value());
    EXPECT_EQ(result.info.at(3).sources[0].defining_instruction,
              std::optional<InstructionId>{2});

    ASSERT_EQ(first.uses.size(), 2u);
    EXPECT_EQ(first.uses[0].instruction_index, 1u);
    EXPECT_EQ(first.uses[0].operand_index, 0u);
    EXPECT_EQ(first.uses[1].instruction_index, 2u);
    EXPECT_EQ(first.uses[1].operand_index, 0u);
    ASSERT_EQ(second.uses.size(), 1u);
    EXPECT_EQ(second.uses[0].instruction_index, 2u);
    EXPECT_EQ(second.uses[0].operand_index, 1u);
    ASSERT_EQ(result.info.at(2).uses.size(), 1u);
    EXPECT_EQ(result.info.at(2).uses[0].instruction_index, 3u);

    ASSERT_EQ(def_user.external_input_uses().at("%x").size(), 1u);
    EXPECT_EQ(def_user.external_input_uses().at("%x")[0].instruction_index, 0u);
}

TEST(DefUserTest, EmptyLinesDoNotConsumeInstructionIds) {
    DefUser def_user({});
    def_user.analyze_def_use(parse_program({
        "%a = add 1, 2", "", "# comment", "ret %a",
    }));

    const auto& result = def_user.results();
    EXPECT_FALSE(result.error.has_value());
    ASSERT_EQ(result.info.size(), 2u);
    EXPECT_EQ(result.info.at(1).inst.opcode.lexeme, "ret");
    ASSERT_EQ(result.info.at(0).uses.size(), 1u);
    EXPECT_EQ(result.info.at(0).uses[0].instruction_index, 1u);
}

TEST(DefUserTest, RecordsRepeatedUsesAndRegisterReuse) {
    DefUser def_user({});
    def_user.analyze_def_use(parse_program({
        "%a = add 1, 2", "%a = mul %a, %a", "ret %a",
    }));

    const auto& result = def_user.results();
    EXPECT_FALSE(result.error.has_value());
    ASSERT_EQ(result.info.size(), 3u);
    const auto& first = result.info.at(0);
    ASSERT_EQ(first.uses.size(), 2u);
    EXPECT_EQ(first.uses[0].instruction_index, 1u);
    EXPECT_EQ(first.uses[0].operand_index, 0u);
    EXPECT_EQ(first.uses[1].instruction_index, 1u);
    EXPECT_EQ(first.uses[1].operand_index, 1u);
    const auto& second = result.info.at(1);
    EXPECT_EQ(second.sources[0].defining_instruction,
              std::optional<InstructionId>{0});
    EXPECT_EQ(second.sources[1].defining_instruction,
              std::optional<InstructionId>{0});
    ASSERT_EQ(second.uses.size(), 1u);
    EXPECT_EQ(second.uses[0].instruction_index, 2u);
    EXPECT_EQ(result.info.at(2).sources[0].defining_instruction,
              std::optional<InstructionId>{1});
}

TEST(DefUserTest, ReportsUndefinedValueAndClearsInstructionInfo) {
    DefUser def_user({});
    def_user.analyze_def_use(parse_program({
        "%a = add 1, 2", "%b = mul %missing, 3",
    }));

    const auto& result = def_user.results();
    EXPECT_TRUE(result.info.empty());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "Undefined value: %missing");
    EXPECT_EQ(result.error->location.line, 2u);
    EXPECT_EQ(result.error->location.column, 10u);
}

TEST(DefUserTest, RejectsDestinationThatConflictsWithExternalInput) {
    DefUser def_user({"%x"});
    def_user.analyze_def_use(parse_program({"%x = add 1, 2"}));

    const auto& result = def_user.results();
    EXPECT_TRUE(result.info.empty());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message,
              "Destination conflicts with external input: %x");
    EXPECT_EQ(result.error->location.line, 1u);
    EXPECT_EQ(result.error->location.column, 1u);
}

TEST(DefUserTest, PreservesParserErrorLocation) {
    DefUser def_user({});
    def_user.analyze_def_use(parse_program({"%a = add 1,"}));

    const auto& result = def_user.results();
    EXPECT_TRUE(result.info.empty());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->location.line, 1u);
    EXPECT_EQ(result.error->location.column, 12u);
    EXPECT_NE(result.error->message.find("Expected operand at end of input"),
              std::string::npos);
}

TEST(DefUserTest, TracksLoadStoreRetAndExternalInputUses) {
    DefUser def_user({"%ptr"});
    def_user.analyze_def_use(parse_program({
        "%a = load %ptr", "store %a, %ptr", "ret %a",
    }));

    const auto& result = def_user.results();
    EXPECT_FALSE(result.error.has_value());
    ASSERT_EQ(result.info.size(), 3u);
    EXPECT_TRUE(result.info.at(1).uses.empty());
    EXPECT_FALSE(result.info.at(1).dest_id.has_value());
    ASSERT_EQ(result.info.at(0).uses.size(), 2u);
    EXPECT_EQ(result.info.at(0).uses[0].instruction_index, 1u);
    EXPECT_EQ(result.info.at(0).uses[1].instruction_index, 2u);
    EXPECT_EQ(result.info.at(1).sources[1].kind, SourceKind::ExternalInput);
    ASSERT_EQ(def_user.external_input_uses().at("%ptr").size(), 2u);
}

TEST(DefUserTest, RejectsSelfAndForwardReferences) {
    for (const auto& parsed : {
             parse_program({"%a = add %a, 1"}),
             parse_program({"%b = mul %a, 2", "%a = add 1, 2"})}) {
        DefUser def_user({});
        def_user.analyze_def_use(parsed);
        const auto& result = def_user.results();
        EXPECT_TRUE(result.info.empty());
        ASSERT_TRUE(result.error.has_value());
        EXPECT_EQ(result.error->message, "Undefined value: %a");
    }
}

TEST(DefUserTest, ClearsStateBetweenCalls) {
    DefUser def_user({"%x"});
    def_user.analyze_def_use(parse_program({"%a = add %x, 1", "ret %a"}));
    ASSERT_EQ(def_user.results().info.size(), 2u);
    ASSERT_EQ(def_user.external_input_uses().at("%x").size(), 1u);

    def_user.analyze_def_use(parse_program({"ret %a"}));
    EXPECT_TRUE(def_user.results().info.empty());
    ASSERT_TRUE(def_user.results().error.has_value());
    EXPECT_TRUE(def_user.external_input_uses().empty());

    def_user.analyze_def_use(parse_program({"ret %x"}));
    EXPECT_FALSE(def_user.results().error.has_value());
    ASSERT_EQ(def_user.results().info.size(), 1u);
    ASSERT_EQ(def_user.external_input_uses().at("%x").size(), 1u);
}

TEST(DefUserTest, RejectsIllegalOperandKindWithItsOwnLocation) {
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
    const auto& result = def_user.results();
    EXPECT_TRUE(result.info.empty());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "Illegal source kind: bad");
    EXPECT_EQ(result.error->location.line, 7u);
    EXPECT_EQ(result.error->location.column, 5u);
}
