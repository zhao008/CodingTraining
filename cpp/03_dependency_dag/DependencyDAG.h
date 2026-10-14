#pragma once

#include "DefUser.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

using Cycle = std::uint64_t;

struct DependencyNode {
    std::vector<InstructionId> predecessors;
    std::vector<InstructionId> successors;
};

struct InstructionTiming {
    Cycle start = 0;
    Cycle finish = 0;
    std::optional<InstructionId> critical_predecessor;
};

struct DAGAnalysisInfo {
    std::vector<DependencyNode> graph;
    std::vector<InstructionTiming> timings;
    std::vector<InstructionId> critical_path;
    Cycle total_cycles = 0;
};

struct DAGAnalysisResult {
    std::optional<DAGAnalysisInfo> info;
    std::optional<std::string> error;
};

class DependencyDAG {
public:
    void analyze(const DefUseAnalysisResult& def_use){
            analysis_result_.info.reset();
            analysis_result_.error.reset();
            if(def_use.error.has_value()){
                analysis_result_.error = "DefUse Error: " + def_use.error->message;
                return;
            }
            analysis_result_.info.emplace();
            for(const auto& inst : def_use.info){
                auto opcode = inst.second.inst.opcode.lexeme; 
                const auto latency_it = opcode_latencies_.find(opcode);
                if(latency_it == opcode_latencies_.end()){
                    analysis_result_.error = "Illegal Opcode!";
                    analysis_result_.info.reset();
                    return;
                };
                DependencyNode depend_node = build_node(inst.second);
                analysis_result_.info->graph.push_back(depend_node);
                InstructionTiming node_timing = comp_node_timing(inst.first, latency_it->second);
                analysis_result_.info->timings.push_back(node_timing);
            }
        
        const auto& timings = analysis_result_.info->timings;
        auto it = std::ranges::max_element(timings, {}, &InstructionTiming::finish);  
        if (it != timings.end()) {
            InstructionId critical_inst_id = static_cast<InstructionId>(it - timings.begin());
            analysis_result_.info->total_cycles = it->finish;
            build_critical_path(critical_inst_id);
        }

    }

    const DAGAnalysisResult& results() const {
        return analysis_result_;
    }

private:
    // Fixed latencies for the experimental model, not actual hardware latencies.
    inline static const std::unordered_map<std::string, Cycle> opcode_latencies_ = {
        {"add", 1},
        {"mul", 3}
    };

    DependencyNode build_node(const InstructionDefUser& inst){
        DependencyNode node;
        for(auto use : inst.uses){
            node.successors.push_back(use.instruction_index);
        }
        std::sort(node.successors.begin(), node.successors.end());
        node.successors.erase(
            std::unique(node.successors.begin(), node.successors.end()),
            node.successors.end());       
        for(auto source : inst.sources){
            if(source.defining_instruction.has_value()) 
                node.predecessors.push_back(*source.defining_instruction);
        }
        std::sort(node.predecessors.begin(), node.predecessors.end());
        node.predecessors.erase(
                                std::unique(node.predecessors.begin(), node.predecessors.end()),
                                node.predecessors.end());
        return node;
    }
    InstructionTiming comp_node_timing(size_t inst_id, Cycle latency){
        InstructionTiming node_timing;
        Cycle max_predecessor_timing = 0;
        // first inst
        if(analysis_result_.info->timings.size() == 0){
            node_timing.start = 0;
            node_timing.finish = node_timing.start + latency;
            return node_timing;
        }   
        // no depend node 
        else if(analysis_result_.info->graph[inst_id].predecessors.size()==0){
            node_timing.start = 0;
            node_timing.finish = node_timing.start + latency;
            return node_timing;
        }
        else{
            for(auto predecessor :analysis_result_.info->graph[inst_id].predecessors ){
                const auto& time =  analysis_result_.info->timings[predecessor];
                if(max_predecessor_timing < time.finish){
                    max_predecessor_timing = time.finish;
                    node_timing.start = max_predecessor_timing;
                    node_timing.finish = node_timing.start + latency;
                    node_timing.critical_predecessor = predecessor;           
                }
            }
            return node_timing;
        }
    }
    void build_critical_path(InstructionId critical_inst_id){
        analysis_result_.info->critical_path.push_back(critical_inst_id);
        while(analysis_result_.info->timings[critical_inst_id].critical_predecessor.has_value()){
            critical_inst_id = *analysis_result_.info->timings[critical_inst_id].critical_predecessor;
            analysis_result_.info->critical_path.push_back(critical_inst_id);
        }
        auto& path = analysis_result_.info->critical_path;
        std::reverse(path.begin(), path.end());
    }
    DAGAnalysisResult analysis_result_;
};
