#include "LoopReport.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"

namespace training {

static void printLoopReport(const llvm::Loop& loop, llvm::raw_ostream& output) {
    output << "loop header=";
    loop.getHeader()->printAsOperand(output, false);   // 打印成 %outer.header 这种形式
    output << " depth=" << loop.getLoopDepth()
           << " blocks=" << loop.getNumBlocks() << "\n";

    for (const llvm::Loop* sub : loop.getSubLoops()) {
        printLoopReport(*sub, output);                       // 再打印子循环
    }
}

static unsigned countLoops(const llvm::Loop& loop){
    unsigned count = 1;
    for(const llvm::Loop* sub : loop.getSubLoops()){
        count += countLoops(*sub);
    }
    return count;
}

llvm::PreservedAnalyses LoopReportPass::run(
    llvm::Function& function,
    llvm::FunctionAnalysisManager& analysis_manager) {
    if (function.isDeclaration()) {
        return llvm::PreservedAnalyses::all();
    }

    auto& loop_info = analysis_manager.getResult<llvm::LoopAnalysis>(function);

    unsigned total = 0;
    for(const llvm::Loop* loop : loop_info){
        total += countLoops(* loop);
    }

    llvm::errs() << "function=" << function.getName() << " loops=" << total << "\n";

    for(const llvm::Loop* loop : loop_info){
        printLoopReport(*loop, llvm::errs());
    }

    return llvm::PreservedAnalyses::all();
}

} // namespace training
