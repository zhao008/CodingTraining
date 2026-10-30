#include <memory>
#include <string>
#include "LLVMDependencyDAG.h"
#include "llvm/IR/Argument.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Verifier.h"

int main(int argc, char** argv){
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    if(argc!=2){
        llvm::errs() << "Usage: " << argv[0] << " <file.ll>\n";
        return 1;
    }
    std::string file_name = argv[1];
    auto module = llvm::parseIRFile(file_name, diagnostic, context);
    if(!module){
        diagnostic.print(argv[0], llvm::errs());
        return 1;
    }
    if(llvm::verifyModule(*module, &llvm::errs() )){
        return 1;
    }
    LLVMDependencyDAG analyzer;
    bool has_failure = false;
    for(llvm::Function& function : *module){
        if(function.isDeclaration()){
            llvm::outs() << "Function: " << function.getName() << " (skipped declaration)\n";
            continue;
        }
        analyzer.analyze(function);
        const DAGAnalysisResult& result = analyzer.results();
        if(result.error){
            llvm::errs() << "Function: " << function.getName() << " error: " << *result.error << "\n";
            has_failure = true;
            continue;
        }
        llvm::outs() << "Function: " << function.getName() << "\n";
        analyzer.printResult();
    }
    return has_failure ? 1 : 0;
}