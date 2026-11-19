#include "LoopReport.h"

#include "llvm/Config/llvm-config.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "LoopReport",
        LLVM_VERSION_STRING,
        [](llvm::PassBuilder& builder) {
            builder.registerPipelineParsingCallback(
                [](llvm::StringRef name,
                   llvm::FunctionPassManager& manager,
                   llvm::ArrayRef<
                       llvm::PassBuilder::PipelineElement>) {
                    if (name != "loop-report") {
                        return false;
                    }

                    manager.addPass(
                        training::LoopReportPass{});
                    return true;
                });
        }
    };
}
