#pragma once
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <string_view>
#include <iostream>

enum class TokenKind {
    Identifier,
    Value,
    Integer,
    Equal,
    Comma,
};

struct SourceLocation {
    size_t line;
    size_t column;
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    SourceLocation location;
};

struct LexError {
    std::string message;
    SourceLocation location;
};

struct TokenizeResult {
    std::vector<Token> tokens;
    std::optional<LexError> error;
};

inline std::string_view token_kind_name(TokenKind kind){
    switch (kind)
    {
        case TokenKind :: Identifier :  return "Identifier" ;
        case TokenKind :: Value :       return "Value" ;
        case TokenKind :: Integer :     return "Integer" ;
        case TokenKind :: Equal :       return "Equal" ;
        case TokenKind :: Comma :       return "Comma" ;
    }
    return "Unknown";
};

inline void print_tokens(const std::vector<Token>& tokens){
    for(const Token& token : tokens){
        std::cout << token_kind_name(token.kind)<< " : " << token.lexeme << std::endl;
    }
};

class Lexer{

public:
    void tokenize_line(std::string_view line, size_t line_number){
        size_t i = 0;
        SourceLocation token_loca;
        token_loca.column = 1;
        token_loca.line = line_number;
        std::string token_name_tmp;
        Token token;
        LexError error;
        std::string token_name;
        
        while(i < line.size()){
            //decode input
            if (line[i] == ' ') {
                i++;
                token_loca.column ++;
                continue;
            }
            else if (line[i] == '#'){
                break;
            }
            // lex value
            else if(line[i] == '%'){
                token_name.push_back(line[i]);
                i++;
                if(is_letter(line[i])){
                    while(line[i]!= ' ' && line[i]!= ',' && i < line.size()){
                        token_name.push_back(line[i]);
                        i ++ ;
                    }  
                    push_token(TokenKind::Value, token_name, token_loca, line_number);
                    token_loca.column += token_name.size();
                    token_name = "";                
                }
                else{
                    lexToken_[line_number].error = LexError{"unexpected Value", {line_number, token_loca.column}};
                    break;
                } 
            }
            else if(is_letter(line[i]) || line[i] == '_' ){
                token_name.push_back(line[i]);
                i++;
                while(line[i] != ' ' && i < line.size()){
                    token_name.push_back(line[i]);
                    i ++ ;
                }  
                push_token(TokenKind::Identifier, token_name, token_loca, line_number);
                token_loca.column += token_name.size();                   
                token_name = "";
            }
            else if(is_int(line[i])){
                auto token_is_letter = false;
                token_name.push_back(line[i]);
                i++;
                while((is_int(line[i]) || is_letter(line[i])) && i < line.size()){
                    token_name.push_back(line[i]);
                    if(is_letter(line[i]))
                        token_is_letter = true;
                    i ++ ;
                }  
                if(token_is_letter){
                    push_token(TokenKind::Identifier, token_name, token_loca, line_number);
                    token_loca.column += token_name.size();
                } 
                else{
                    push_token(TokenKind::Integer, token_name, token_loca, line_number);
                    token_loca.column += token_name.size();                   
                }
                token_name = "";                  
            }
            else if(line[i] == '='){
                push_token(TokenKind::Equal, "=", token_loca, line_number);
                token_loca.column++;    
                i++ ;       
            }   
            else if(line[i] == ','){
                push_token(TokenKind::Comma, ",", token_loca, line_number);
                token_loca.column++;     
                i++;           
            }       
            else {
                lexToken_[line_number].error = LexError{"unexpected char", {line_number, token_loca.column}};
                break;               
            }
        }
    };
    const std::map<size_t, TokenizeResult>& results() const { return lexToken_; }

private:
    bool is_letter(const char c){
        bool is_lower = (c >= 'a' && c <= 'z');
        bool is_upper = (c >= 'A' && c <= 'Z');
        bool is_letter = is_lower || is_upper;
        return is_letter;
    }

    bool is_int(const char c){
        bool is_int = (c >= '0' && c<= '9');
        return is_int;
    }

    void push_token( const TokenKind kind, const std::string& lexeme, 
                          const SourceLocation location, const size_t line_number){
        Token token;
        token.kind = kind;
        token.lexeme = lexeme;
        token.location = location;
        lexToken_[line_number].tokens.push_back(std::move(token));  
    }

    std::map <size_t, TokenizeResult> lexToken_;

};