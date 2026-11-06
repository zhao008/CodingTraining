#include "SimplifyAddZero.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"

llvm::PreservedAnalyses training::SimplifyAddZeroPass::run(
    llvm::Function& function,
    llvm::FunctionAnalysisManager& /*analysis_manager*/) {
    bool changed = false;
    for (auto& bb : function) {
        for (auto inst_ptr = bb.begin(); inst_ptr != bb.end();) {
            if (inst_ptr->getOpcode() == llvm::Instruction::Add &&
                inst_ptr->getType()->isIntegerTy(32)) {
                bool delete_inst = false;
                llvm::Value* lhs = inst_ptr->getOperand(0);  // 左边，第 0 个操作数
                llvm::Value* rhs = inst_ptr->getOperand(1);  // 右边，第 1 个操作数
                if (auto* constant = llvm::dyn_cast<llvm::ConstantInt>(lhs)) {
                    if (constant->isZero()) {
                        inst_ptr->replaceAllUsesWith(rhs);
                        changed = true;
                        delete_inst = true;
                    }
                }
                if (auto* constant = llvm::dyn_cast<llvm::ConstantInt>(rhs)) {
                    if (constant->isZero()) {
                        inst_ptr->replaceAllUsesWith(lhs);
                        changed = true;
                        delete_inst = true;
                    }
                }
                if (delete_inst) {
                    // eraseFromParent 返回被删指令的下一条；continue 跳过下面的 ++，避免多走一步。
                    inst_ptr = inst_ptr->eraseFromParent();
                    continue;
                }
            }
            ++inst_ptr;
        }
    }
    return changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
}
