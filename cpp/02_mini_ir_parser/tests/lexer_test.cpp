#include <gtest/gtest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "Lexer.h"

// 让 gtest 失败时打印 "Identifier" 这样的名字，而不是一串字节
std::ostream& operator<<(std::ostream& os, TokenKind kind) {
    return os << token_kind_name(kind);
}

namespace {

// 对单独一行做词法分析，返回这一行的结果。
// 如果 Lexer 没给这一行留下任何记录（比如空行），就当作"没有 token、没有错误"。
TokenizeResult lex(std::string_view line, int line_number = 1) {
    Lexer lexer;
    lexer.tokenize_line(line, line_number);
    auto it = lexer.results().find(line_number);
    if (it == lexer.results().end()) {
        return TokenizeResult{};
    }
    return it->second;
}

struct ExpectedToken {
    TokenKind kind;
    std::string lexeme;
    int column;
};

// 逐个比较 token 的种类、文本、行号、列号
void expect_tokens(const TokenizeResult& result,
                   const std::vector<ExpectedToken>& expected,
                   int line_number = 1) {
    // print_tokens(result.tokens);
    ASSERT_EQ(result.tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        SCOPED_TRACE("token #" + std::to_string(i));
        const Token& token = result.tokens[i];
        EXPECT_EQ(token.kind, expected[i].kind);
        EXPECT_EQ(token.lexeme, expected[i].lexeme);
        EXPECT_EQ(token.location.line, line_number);
        EXPECT_EQ(token.location.column, expected[i].column);
    }
}

std::vector<TokenKind> kinds_of(const TokenizeResult& result) {
    std::vector<TokenKind> kinds;
    for (const Token& token : result.tokens) {
        kinds.push_back(token.kind);
    }
    return kinds;
}

}  // namespace

// Test 1：普通二元指令
TEST(LexerTest, BinaryInstruction) {
    auto result = lex("%r1 = add %r2, %r3");

    EXPECT_FALSE(result.error.has_value());
    expect_tokens(result, {
        {TokenKind::Value,      "%r1", 1},
        {TokenKind::Equal,      "=",   5},
        {TokenKind::Identifier, "add", 7},
        {TokenKind::Value,      "%r2", 11},
        {TokenKind::Comma,      ",",   14},
        {TokenKind::Value,      "%r3", 16},
    });
}

// Test 2：无目的寄存器指令
TEST(LexerTest, InstructionWithoutDestination) {
    auto result = lex("store %r4, %ptr");

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(kinds_of(result), (std::vector<TokenKind>{
        TokenKind::Identifier,
        TokenKind::Value,
        TokenKind::Comma,
        TokenKind::Value,
    }));
}

// Test 3：一个操作数
TEST(LexerTest, SingleOperand) {
    auto result = lex("ret %r4");

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.tokens.size(), 2u);
}

// Test 4：整数字面量
TEST(LexerTest, IntegerLiteral) {
    auto result = lex("%r7 = add %r4, 42");
    // print_tokens(result.tokens);
    EXPECT_FALSE(result.error.has_value());
    ASSERT_FALSE(result.tokens.empty());
    const Token& last = result.tokens.back();
    EXPECT_EQ(last.kind, TokenKind::Integer);
    EXPECT_EQ(last.lexeme, "42");
}

// Test 5：空白和注释
TEST(LexerTest, WhitespaceAndComment) {
    auto result = lex("   ret %r4   # return value");

    EXPECT_FALSE(result.error.has_value());
    ASSERT_EQ(result.tokens.size(), 2u);
    EXPECT_EQ(result.tokens[0].location.column, 4);
}

// Test 6：空行
TEST(LexerTest, EmptyLine) {
    auto result = lex("");

    EXPECT_TRUE(result.tokens.empty());
    EXPECT_FALSE(result.error.has_value());
}

// Test 7：非法字符
TEST(LexerTest, IllegalCharacter) {
    auto result = lex("@r1 = add %r2, %r3");

    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->location.line, 1);
    EXPECT_EQ(result.error->location.column, 1);
}

// Test 8：不完整 Value
TEST(LexerTest, IncompleteValue) {
    auto result = lex("% = add %r1, %r2");
    print_tokens(result.tokens);
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->location.line, 1);
    EXPECT_EQ(result.error->location.column, 1);
}
