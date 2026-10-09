#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Lexer.h"
#include "Parser.h"

namespace {

struct InstructionCase {
    const char* source;
    const char* opcode;
    const char* destination;
    std::vector<std::pair<TokenKind, std::string>> operands;
};

struct ErrorCase {
    const char* source;
    const char* expected_message;
};

}  // namespace

TEST(ParserTest, ParsesSupportedSingleInstructions) {
    const std::vector<InstructionCase> cases = {
        {"%r1 = add %r2, 42", "add", "%r1", {
            {TokenKind::Value, "%r2"},
            {TokenKind::Integer, "42"},
        }},
        {"%r1 = mul %r2, 3", "mul", "%r1", {
            {TokenKind::Value, "%r2"},
            {TokenKind::Integer, "3"},
        }},
        {"%r1 = load %ptr", "load", "%r1", {
            {TokenKind::Value, "%ptr"},
        }},
        {"store %r1, %ptr", "store", nullptr, {
            {TokenKind::Value, "%r1"},
            {TokenKind::Value, "%ptr"},
        }},
        {"ret %r1", "ret", nullptr, {
            {TokenKind::Value, "%r1"},
        }},
    };

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.source);

        Lexer lexer;
        lexer.tokenize_line(test_case.source, 1);
        const auto& lex_result = lexer.results().at(1);

        ASSERT_FALSE(lex_result.error.has_value());

        Parser parser;
        parser.parser_tokenize_line(lexer.results());
        const auto& parse_result = parser.results().at(1);

        ASSERT_TRUE(parse_result.instruction.has_value());
        EXPECT_FALSE(parse_result.error.has_value());

        const Instruction& instruction = *parse_result.instruction;
        EXPECT_EQ(instruction.opcode.lexeme, test_case.opcode);

        if (test_case.destination == nullptr) {
            EXPECT_FALSE(instruction.destination.has_value());
        } else {
            ASSERT_TRUE(instruction.destination.has_value());
            EXPECT_EQ(instruction.destination->kind, TokenKind::Value);
            EXPECT_EQ(instruction.destination->lexeme, test_case.destination);
        }

        ASSERT_EQ(instruction.operands.size(), test_case.operands.size());
        for (size_t i = 0; i < test_case.operands.size(); ++i) {
            EXPECT_EQ(instruction.operands[i].kind, test_case.operands[i].first);
            EXPECT_EQ(instruction.operands[i].lexeme, test_case.operands[i].second);
        }
    }
}

TEST(ParserTest, ParsesMultipleInstructions) {
    Lexer lexer;
    lexer.tokenize_line("%r1 = add %r2, 42", 1);
    lexer.tokenize_line("%r3 = load %ptr", 2);
    lexer.tokenize_line("ret %r3", 3);

    for (const auto& [line_number, lex_result] : lexer.results()) {
        SCOPED_TRACE(line_number);
        ASSERT_FALSE(lex_result.error.has_value());
    }

    Parser parser;
    parser.parser_tokenize_line(lexer.results());

    ASSERT_EQ(parser.results().size(), 3u);

    const auto& add_result = parser.results().at(1);
    ASSERT_TRUE(add_result.instruction.has_value());
    EXPECT_FALSE(add_result.error.has_value());
    EXPECT_EQ(add_result.instruction->opcode.lexeme, "add");
    ASSERT_TRUE(add_result.instruction->destination.has_value());
    EXPECT_EQ(add_result.instruction->destination->lexeme, "%r1");
    ASSERT_EQ(add_result.instruction->operands.size(), 2u);
    EXPECT_EQ(add_result.instruction->operands[0].lexeme, "%r2");
    EXPECT_EQ(add_result.instruction->operands[1].lexeme, "42");

    const auto& load_result = parser.results().at(2);
    ASSERT_TRUE(load_result.instruction.has_value());
    EXPECT_FALSE(load_result.error.has_value());
    EXPECT_EQ(load_result.instruction->opcode.lexeme, "load");
    ASSERT_TRUE(load_result.instruction->destination.has_value());
    EXPECT_EQ(load_result.instruction->destination->lexeme, "%r3");
    ASSERT_EQ(load_result.instruction->operands.size(), 1u);
    EXPECT_EQ(load_result.instruction->operands[0].lexeme, "%ptr");

    const auto& ret_result = parser.results().at(3);
    ASSERT_TRUE(ret_result.instruction.has_value());
    EXPECT_FALSE(ret_result.error.has_value());
    EXPECT_EQ(ret_result.instruction->opcode.lexeme, "ret");
    EXPECT_FALSE(ret_result.instruction->destination.has_value());
    ASSERT_EQ(ret_result.instruction->operands.size(), 1u);
    EXPECT_EQ(ret_result.instruction->operands[0].lexeme, "%r3");
}

TEST(ParserTest, ReportsInvalidInstructions) {
    const std::vector<ErrorCase> cases = {
        {"%r1 add %r2, 42", "Expected '=' after destination"},
        {"%r1 = add %r2 42", "Expected ',' between operands"},
        {"%r1 = add %r2,", "Expected operand at end of input"},
        {"%r1 = add %r2", "Expected ',' and another operand at end of input"},
        {"add %r2, 42", "'add' requires a destination"},
        {"%r1 = ret %r2", "'ret' does not allow a destination"},
        {"%r1 = unknown %r2", "Unsupported opcode 'unknown'"},
        {"ret %r1, 42", "Unexpected trailing token ','"},
    };

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.source);

        Lexer lexer;
        lexer.tokenize_line(test_case.source, 1);
        const auto& lex_result = lexer.results().at(1);

        ASSERT_FALSE(lex_result.error.has_value());

        Parser parser;
        parser.parser_tokenize_line(lexer.results());
        const auto& parse_result = parser.results().at(1);

        EXPECT_FALSE(parse_result.instruction.has_value());
        ASSERT_TRUE(parse_result.error.has_value());
        EXPECT_EQ(parse_result.error->message, test_case.expected_message);
    }
}
