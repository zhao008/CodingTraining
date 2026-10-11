#pragma once

#include "Parser.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

using InstructionId = std::size_t;

// 来源分类，不是 int/float 等数值类型。
enum class SourceKind {
    Instruction,    // 前面某条指令产生的值
    ExternalInput,  // 调用方显式声明的外部输入
    Constant        // 直接写出的整数常量
};

struct SourceRef {
    SourceKind kind;
    std::size_t operand_index;
    std::optional<InstructionId> defining_instruction;
};

struct DefUseAnalysisError {
    std::string message;
    SourceLocation location;
};

struct UseSite {
    InstructionId instruction_index;
    std::size_t operand_index;
};

struct InstructionDefUser{
    Instruction inst;
    std::optional<InstructionId> dest_id;
    std::vector<UseSite> uses;
    std::vector<SourceRef> sources;
};

struct DefUseAnalysisResult {
    std::optional<InstructionDefUser> info;
    std::optional<DefUseAnalysisError> error;
};

class DefUser
{
public:
    explicit DefUser(std::unordered_set<std::string> inputs)
    : external_inputs_(std::move(inputs)) {};  

    void analyze_def_use(const std::map <size_t, ParseResult>& passer_token){
        definitions_.clear();
        analysis_result_.clear();
        external_input_uses_.clear();
        InstructionId instruction_id = 0;
        
        for (const auto& passer_line : passer_token) {
            
            InstructionDefUser inst_def_user;
           
            if(passer_line.second.error.has_value()){
                const ParseError& passer_error = *passer_line.second.error;
                set_error( instruction_id,
                           "PASSER ERROR : " + passer_error.message,
                            passer_error.location);

                return;
            }
            // 空行和注释行没有 instruction，这是合法情况，直接跳过。
            else if(passer_line.second.instruction.has_value()){
                const Instruction& inst = *passer_line.second.instruction;
                inst_def_user.inst = inst;
                for(size_t i = 0; i < inst.operands.size(); i++){
                    const auto& source = inst.operands[i];
                    SourceRef source_def_user{};
                    if(source.kind == TokenKind::Integer){
                        source_def_user.kind = SourceKind::Constant;
                        source_def_user.operand_index = i;
                    }
                    else if (source.kind == TokenKind::Value){ 
                        auto inst_id = definitions_.find(source.lexeme);
                        auto is_external_inputs = external_inputs_.find(source.lexeme);
                        if(inst_id != definitions_.end()){
                            source_def_user.kind = SourceKind::Instruction;
                            source_def_user.defining_instruction = inst_id->second;
                            source_def_user.operand_index = i;
                            analysis_result_[inst_id->second].info->uses.push_back({instruction_id, i});
                        }
                        else if(is_external_inputs!=external_inputs_.end()){
                            source_def_user.kind = SourceKind::ExternalInput;
                            source_def_user.operand_index = i;
                            external_input_uses_[source.lexeme].push_back({instruction_id, i});
                        }
                        else {
                            set_error( instruction_id,
                                       "Undefined value: " + source.lexeme ,
                                       source.location);
                         
                            return;                           
                        }                      
                    }
                    else{
                        set_error( instruction_id,
                                   "Illegal source kind: " + source.lexeme ,
                                   source.location);

                        return;
                    }

                    inst_def_user.sources.push_back(source_def_user);
                }
                if(inst.destination.has_value()){
                    const Token& dest_token = *inst.destination;
                    inst_def_user.dest_id = instruction_id;
                    definitions_.insert_or_assign(dest_token.lexeme, instruction_id);
                }
                analysis_result_[instruction_id].info = inst_def_user;  
                instruction_id ++ ;
            }

                  
        }
    };
   
    const  std::map<size_t, DefUseAnalysisResult>& results() const {
        return analysis_result_;
    }   

    const std::unordered_map<std::string, std::vector<UseSite>>& external_input_uses() const {
        return external_input_uses_;
    }   

private:
    
    void set_error(size_t line_number,
                   std::string message,
                   SourceLocation location) {
        auto& result = analysis_result_[line_number];
        result.info.reset();
        result.error = DefUseAnalysisError{std::move(message), location};
    }
    
    std::unordered_map<std::string, InstructionId> definitions_; 
    std::map<size_t, DefUseAnalysisResult> analysis_result_;
    std::unordered_set<std::string> external_inputs_;
    std::unordered_map<std::string, std::vector<UseSite>> external_input_uses_;
};  
