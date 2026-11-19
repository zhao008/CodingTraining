#pragma once

#include "llvm/IR/PassManager.h"

namespace training {

class LoopReportPass
    : public llvm::OptionalPassInfoMixin<LoopReportPass> {
public:
    llvm::PreservedAnalyses run(
        llvm::Function& function,
        llvm::FunctionAnalysisManager& analysis_manager);
};

} // namespace training