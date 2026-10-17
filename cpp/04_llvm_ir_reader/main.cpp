#include <memory>
#include <string>
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
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

void print_operands(llvm::Instruction& instruction){
    size_t operand_num = 0;
    for(llvm::Use& use : instruction.operands()){
        auto value = use.get();
        llvm::outs() << "    operand " << operand_num++ << ": " ;
        value->printAsOperand(llvm::outs(), false);
        llvm::outs() << ", ";
        if(llvm::dyn_cast<llvm::Instruction>(value)){
            llvm::outs() << "Instruction";
        } else if(llvm::dyn_cast<llvm::Argument>(value)){
            llvm::outs() << "Argument";
        } else if(llvm::dyn_cast<llvm::ConstantInt>(value)){
            llvm::outs() << "ConstantInt";
        } else {
            llvm::outs() << "Other";
        }
        llvm::outs() << "\n";
    }
}
void print_result_uses(llvm::Instruction& instruction){
    if( instruction.getType()->isVoidTy() ){
        llvm::outs() << "    no result (void)" << "\n" ;
    } else {
        llvm::outs() << "    ";
        instruction.printAsOperand(llvm::outs(),false);
        llvm::outs() << ":" << "\n";
        if(instruction.use_empty()){
            llvm::outs() << "      no uses" << "\n";
        } else {
            for(llvm::Use& use : instruction.uses()){
                auto value = use.getUser();
                llvm::outs() << "      used by" << *value << ", operand " << use.getOperandNo() << "\n";
            }
        }
    }
}
void inspect_module(llvm::Module& module){
    size_t definition_count = 0;
    size_t declaration_count = 0;
    size_t block_count = 0;
    size_t inst_count = 0;
    for(llvm::Function& function : module){
        llvm::outs() << "Function: " << function.getName();
        if(function.isDeclaration()){
            llvm::outs() << " (declaration)\n";
            ++declaration_count;
            continue;
        }
        llvm::outs() << "\n";
        ++definition_count;
        size_t bb_num = 0;
        for(llvm::BasicBlock& block : function){
            llvm::outs() << "BasicBlock: ";
            block.printAsOperand(llvm::outs(), false);
            llvm::outs() << " #" << bb_num++ << "\n";
            ++block_count;
            size_t inst_num = 0;
            for(llvm::Instruction& inst : block){
                llvm::outs() << "  Instruction: #" << inst_num++ << " : " << inst << " [opcode: " << inst.getOpcodeName() << "]\n";
                ++inst_count;
                print_operands(inst);
                print_result_uses(inst);
            }
        }
    }
    llvm::outs() << "Summary:\n"
                 << "  function definitions: " << definition_count << "\n"
                 << "  function declarations: " << declaration_count << "\n"
                 << "  basic blocks: " << block_count << "\n"
                 << "  instructions: " << inst_count << "\n";
}
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
    inspect_module(*module);
    return 0;
}