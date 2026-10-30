#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"

namespace llvm {
class Function;
class Instruction;
}

using InstructionId = std::size_t;

struct LLVMDependencyNode {
    const llvm::Instruction* instruction = nullptr;

    std::vector<InstructionId> predecessors;
    std::vector<InstructionId> successors;
};

struct DAGAnalysisInfo {
    std::vector<LLVMDependencyNode> graph;
};

struct DAGAnalysisResult {
    std::optional<DAGAnalysisInfo> info;
    std::optional<std::string> error;
};

class LLVMDependencyDAG {
public:
    void analyze(const llvm::Function& function);

    const DAGAnalysisResult& results() const {
        return result_;
    }

    void printResult();
    
private:
    DAGAnalysisResult result_;
};