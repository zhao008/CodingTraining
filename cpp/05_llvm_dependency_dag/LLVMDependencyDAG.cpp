#include "LLVMDependencyDAG.h"

void LLVMDependencyDAG::analyze(const llvm::Function& function){
    std::unordered_map<const llvm::Instruction*, InstructionId> inst_id_map;
    DAGAnalysisInfo dag;
    size_t inst_id = 0;
    result_.info.reset();
    result_.error.reset();
    if(function.isDeclaration()){
        result_.error = std::string("Declaration cannot be analyzed: ") + function.getName().str();
        return;
    }
    if(function.isVarArg()){
        result_.error = "Variadic functions are not supported";
        return;
    }
    if(!function.getReturnType()->isIntegerTy(32)){
        result_.error = "Return type must be i32";
        return;
    }
    for(const llvm::Argument& arg : function.args()){
        if(!arg.getType()->isIntegerTy(32)){
            result_.error = "Argument type must be i32, argument index " + std::to_string(arg.getArgNo());
            return;
        }
    }
    if(function.size() != 1){
    result_.error = std::string("Only one basic block is supported, but found ") + std::to_string(function.size());
    return;
    }
    for(const llvm::BasicBlock& block : function){

        for(const llvm::Instruction& inst : block){
            LLVMDependencyNode dag_node;
            switch(inst.getOpcode()){
                case llvm::Instruction::Add :
                case llvm::Instruction::Mul :
                    if(!inst.getType()->isIntegerTy(32)){
                        result_.error = std::string("Arithmetic result must be i32: ") + inst.getOpcodeName();
                        return;
                    }
                    inst_id_map.emplace(&inst, inst_id);
                    break;
                case llvm::Instruction::Ret :
                    continue;
                    break;
                default:
                    result_.error = std::string("Unsupported instruction: ") + inst.getOpcodeName();
                    return;
            }
            for(const llvm::Use& operand : inst.operands()){
                auto value = operand.get();
                if(const llvm::Instruction* producer = llvm::dyn_cast<llvm::Instruction>(value)){
                    auto it = inst_id_map.find(producer);
                    if(it == inst_id_map.end()){
                        result_.error = std::string("Operand is not a numbered add/mul instruction: ")
                                    + producer->getOpcodeName();
                        return;
                    }
                    dag_node.predecessors.push_back(it->second);
                }
                else if(llvm::isa<llvm::Argument>(value) || llvm::isa<llvm::ConstantInt>(value)){
                    // 函数参数或整数常量：外部输入，不建边
                }
                else{
                    std::string text;
                    llvm::raw_string_ostream os(text);
                    value->printAsOperand(os, false);
                    result_.error = "Unsupported operand: " + os.str();
                    return;
                }
            }
            std::sort(dag_node.predecessors.begin(), dag_node.predecessors.end());
            dag_node.predecessors.erase(std::unique(dag_node.predecessors.begin(), dag_node.predecessors.end()), dag_node.predecessors.end());
            dag_node.instruction = &inst;
            for(auto opearand_used_id : dag_node.predecessors){
                dag.graph[opearand_used_id].successors.push_back(inst_id) ;
            }
            dag.graph.push_back(dag_node);
            inst_id++;
        }
    }
    result_.info = std::move(dag);
}

namespace {
void print_ids(const std::vector<InstructionId>& ids){
    llvm::outs() << "[";
    for(size_t i = 0; i < ids.size(); i++){
        if(i != 0){
            llvm::outs() << ", ";
        }
        llvm::outs() << ids[i];
    }
    llvm::outs() << "]";
}
}

void LLVMDependencyDAG::printResult(){
    if(!result_.info){
        return;
    }
    const std::vector<LLVMDependencyNode>& graph = result_.info->graph;
    size_t edge_count = 0;
    for(const LLVMDependencyNode& node : graph){
        edge_count += node.successors.size();
    }
    llvm::outs() << "  nodes: " << graph.size() << ", edges: " << edge_count << "\n";
    for(size_t i = 0; i < graph.size(); i++){
        llvm::outs() << "  [" << i << "]" << *graph[i].instruction << "\n";
        llvm::outs() << "      predecessors: ";
        print_ids(graph[i].predecessors);
        llvm::outs() << "\n      successors:   ";
        print_ids(graph[i].successors);
        llvm::outs() << "\n";
    }
}
