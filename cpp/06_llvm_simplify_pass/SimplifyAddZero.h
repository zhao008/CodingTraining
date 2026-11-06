#pragma once

#include "llvm/IR/PassManager.h"

namespace training {

class SimplifyAddZeroPass
    : public llvm::OptionalPassInfoMixin<SimplifyAddZeroPass> {
public:
    llvm::PreservedAnalyses run(
        llvm::Function& function,
        llvm::FunctionAnalysisManager& analysis_manager);
};

} // namespace training