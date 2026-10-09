#pragma once

#include "Lexer.h"

#include <optional>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <iostream>

struct Instruction {
    std::optional<Token> destination;
    Token opcode;
    std::vector<Token> operands;
};

struct ParseError {
    std::string message;
    SourceLocation location;
};

struct ParseResult {
    std::optional<Instruction> instruction;
    std::optional<ParseError> error;
};

enum class Opcode { 
    Add, 
    Mul, 
    Load, 
    Store, 
    Ret, 
    Unknown 
};

inline Opcode to_opcode(const std::string& name){
    if (name == "add")   return Opcode::Add;
    if (name == "mul")   return Opcode::Mul;
    if (name == "load")  return Opcode::Load;
    if (name == "store") return Opcode::Store;
    if (name == "ret")   return Opcode::Ret;
    return Opcode::Unknown;
}

class Parser{
public:

    void parser_tokenize_line(const std::map <size_t, TokenizeResult>& lexTokens){
        for( const auto& lexTokenLine : lexTokens ){
            const size_t line_number = lexTokenLine.first;
            passerToken_[line_number] = ParseResult{};

            // check lex token no error
            if (lexTokenLine.second.error.has_value()) {
                const LexError& lex_error = *lexTokenLine.second.error;
                set_error(line_number,
                          "Lex Error: " + lex_error.message,
                          lex_error.location);
                continue;
            }

            const auto& tokens = lexTokenLine.second.tokens;
            if (tokens.empty()) {
                continue;
            }

            switch (tokens[0].kind) {
                case TokenKind::Value:
                    parse_dest_first_instruction(lexTokenLine);
                    break;
                case TokenKind::Identifier:
                    parse_opcode_first_instruction(lexTokenLine);
                    break;
                default:
                    set_error(line_number,
                              "Expected value or opcode at start of line",
                              tokens[0].location);
            }
        }
    };

    const std::map<size_t, ParseResult>& results() const {
        return passerToken_;
    }

    // Debug helper for callers that need to inspect a parsed instruction.
    static void print_debug_instruction(const Instruction& instruction) {
        std::cout << "Instruction:\n";

        if (instruction.destination.has_value()) {
            std::cout << "  destination:\n";
            print_tokens(std::vector<Token>{*instruction.destination});
        } else {
            std::cout << "  destination: <none>\n";
        }

        std::cout << "  opcode:\n";
        print_tokens(std::vector<Token>{instruction.opcode});

        std::cout << "  operands:\n";
        print_tokens(instruction.operands);
    }

private:
    static bool is_operand_token(const Token& token) {
        return token.kind == TokenKind::Value ||
               token.kind == TokenKind::Integer;
    }

    static SourceLocation end_location(const std::vector<Token>& tokens) {
        const Token& last = tokens.back();
        return {
            last.location.line,
            last.location.column + last.lexeme.size()
        };
    }

    void set_error(size_t line_number,
                   std::string message,
                   SourceLocation location) {
        auto& result = passerToken_[line_number];
        result.instruction.reset();
        result.error = ParseError{std::move(message), location};
    }

    bool parse_operands(size_t line_number,
                        const std::vector<Token>& tokens,
                        size_t start,
                        size_t expected_count,
                        std::vector<Token>& operands) {
        size_t pos = start;

        for (size_t index = 0; index < expected_count; ++index) {
            if (pos >= tokens.size()) {
                set_error(line_number,
                          "Expected operand at end of input",
                          end_location(tokens));
                return false;
            }

            if (!is_operand_token(tokens[pos])) {
                set_error(line_number,
                          "Expected Integer or Value",
                          tokens[pos].location);
                return false;
            }

            operands.push_back(tokens[pos]);
            ++pos;

            if (index + 1 < expected_count) {
                if (pos >= tokens.size()) {
                    set_error(line_number,
                              "Expected ',' and another operand at end of input",
                              end_location(tokens));
                    return false;
                }

                if (tokens[pos].kind != TokenKind::Comma) {
                    set_error(line_number,
                              "Expected ',' between operands",
                              tokens[pos].location);
                    return false;
                }

                ++pos;
            }
        }

        if (pos < tokens.size()) {
            set_error(line_number,
                      "Unexpected trailing token '" + tokens[pos].lexeme + "'",
                      tokens[pos].location);
            return false;
        }

        return true;
    }

    void parse_dest_first_instruction(const std::pair<const size_t, TokenizeResult> &lexTokenLine){
        const size_t line_number = lexTokenLine.first;
        const auto& tokens = lexTokenLine.second.tokens;

        if(tokens.size() == 1){
            set_error(line_number,
                "Expected '=' after destination",
                end_location(tokens));
            return;
        }
        if(tokens[1].kind != TokenKind::Equal){
            set_error(line_number, "Expected '=' after destination", tokens[1].location);
            return;
        }
        if(tokens.size() == 2){
            set_error(line_number, "Expected opcode after '='", end_location(tokens));
            return;
        }
        if(tokens[2].kind != TokenKind::Identifier){
            set_error(line_number, "Expected Identifier for opcode", tokens[2].location);
            return;
        }

        switch (to_opcode(tokens[2].lexeme)) {
            case Opcode::Add:
            case Opcode::Mul:
                parse_with_dest_two_operands(lexTokenLine);
                break;
            case Opcode::Load:
                parse_with_dest_one_operands(lexTokenLine);
                break;
            case Opcode::Store:
            case Opcode::Ret:
                set_error(line_number,
                          "'" + tokens[2].lexeme + "' does not allow a destination",
                          tokens[0].location);
                break;
            case Opcode::Unknown:
                set_error(line_number,
                          "Unsupported opcode '" + tokens[2].lexeme + "'",
                          tokens[2].location);
                break;
        }
    };

    void parse_opcode_first_instruction(const std::pair<const size_t, TokenizeResult> &lexTokenLine){
        const size_t line_number = lexTokenLine.first;
        const auto& tokens = lexTokenLine.second.tokens;

        switch (to_opcode(tokens[0].lexeme)) {
            case Opcode::Store:
                parse_without_dest_two_operands(lexTokenLine);
                break;
            case Opcode::Ret:
                parse_without_dest_one_operands(lexTokenLine);
                break;
            case Opcode::Add:
            case Opcode::Mul:
            case Opcode::Load:
                set_error(line_number,
                          "'" + tokens[0].lexeme + "' requires a destination",
                          tokens[0].location);
                break;
            case Opcode::Unknown:
                set_error(line_number,
                          "Unsupported opcode '" + tokens[0].lexeme + "'",
                          tokens[0].location);
                break;
        }
    };

    void parse_with_dest_one_operands(const std::pair<const size_t, TokenizeResult> &lexTokenLine){
        const size_t line_number = lexTokenLine.first;
        const auto& tokens = lexTokenLine.second.tokens; 

        std::vector<Token> operands;
        if (!parse_operands(line_number, tokens, 3, 1, operands)) {
            return;
        }

        passerToken_[line_number].instruction =
            Instruction{tokens[0], tokens[2], std::move(operands)};
    };

    void parse_with_dest_two_operands(const std::pair<const size_t, TokenizeResult> &lexTokenLine){
        const size_t line_number = lexTokenLine.first;
        const auto& tokens = lexTokenLine.second.tokens; 

        std::vector<Token> operands;
        if (!parse_operands(line_number, tokens, 3, 2, operands)) {
            return;
        }

        passerToken_[line_number].instruction =
            Instruction{tokens[0], tokens[2], std::move(operands)};
    };

    void parse_without_dest_one_operands(const std::pair<const size_t, TokenizeResult> &lexTokenLine){
        const size_t line_number = lexTokenLine.first;
        const auto& tokens = lexTokenLine.second.tokens; 

        std::vector<Token> operands;
        if (!parse_operands(line_number, tokens, 1, 1, operands)) {
            return;
        }

        passerToken_[line_number].instruction =
            Instruction{std::nullopt, tokens[0], std::move(operands)};
    };

    void parse_without_dest_two_operands(const std::pair<const size_t, TokenizeResult> &lexTokenLine){
        const size_t line_number = lexTokenLine.first;
        const auto& tokens = lexTokenLine.second.tokens; 

        std::vector<Token> operands;
        if (!parse_operands(line_number, tokens, 1, 2, operands)) {
            return;
        }

        passerToken_[line_number].instruction =
            Instruction{std::nullopt, tokens[0], std::move(operands)};
    };
    std::map <size_t, ParseResult> passerToken_;
};
