#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Instruction{
    string inst_name;
    string dest_reg;
    std::vector<string> source_reg;
    int latency;
};

std::vector<Instruction> program{
    {"IADD", "R1", {"R2", "R3"}, 4},
    {"FFMA", "R4", {"R1", "R5"}, 4},
    {"IADD", "R6", {"R7", "R8"}, 4},
    {"FFMA", "R9", {"R4", "R6"}, 4},
};

struct ScheduleResult {
    int instruction_id;
    string opcode;
    int issue_cycle;
    int stall_cycles;
    int ready_cycle;
    std::vector<std::string> blocking_registers;
};


void scoreboard_retire(std::unordered_map<std::string, int>& reg_ready, int clk){
    for (auto it = reg_ready.cbegin(); it != reg_ready.cend(); ) {
        if(it->second <= (clk+1)){
            it = reg_ready.erase(it);
        } else {
            it++;
        }
    }
}

std::vector<ScheduleResult> simulate(const std::vector<Instruction> program){
    int clk = 0;
    int pc = 0;
    std::vector<ScheduleResult> resualt_trace;
    std::unordered_map<std::string, int> reg_ready;
    ScheduleResult trace_once;
    Instruction inst;
    bool source1_ready;
    bool source2_ready;
    int earliest_issue_cycle;

    while(resualt_trace.size() < program.size() ){
        inst = program[pc];
        // find source1 in scoreboard 
        auto source1 = reg_ready.find(inst.source_reg[0]);
        if (source1 != reg_ready.end() && clk < source1->second) {
            source1_ready = false;
        }
        else {
            source1_ready = true;
            
        }
        // find source 2 in socreboard
        auto source2 = reg_ready.find(inst.source_reg[1]);
        if (source2 != reg_ready.end() && clk < source2->second) {
            source2_ready = false;
        }
        else {
            source2_ready = true;
        }        
        // get inst issu time
        if(resualt_trace.empty() ){
            earliest_issue_cycle = clk;
        }
        else {
           earliest_issue_cycle = resualt_trace.back().issue_cycle + 1;
        }
        if(earliest_issue_cycle == clk){ //update once
            trace_once.blocking_registers.clear();
            if(!source1_ready & source2_ready){
                trace_once.issue_cycle = source1->second;
                trace_once.blocking_registers.push_back(source1->first);
            }
            else if(source1_ready & !source2_ready){
                trace_once.issue_cycle = source2->second;
                trace_once.blocking_registers.push_back(source2->first);
            }
            else if(!source1_ready & !source2_ready){
                trace_once.issue_cycle = max(source1->second, source2->second);
                trace_once.blocking_registers.push_back(source1->first);
                trace_once.blocking_registers.push_back(source2->first);
            }
            else{
                trace_once.issue_cycle = earliest_issue_cycle;
            }
        }
        // issu inst
        if(trace_once.issue_cycle == clk){
            trace_once.instruction_id = pc;
            trace_once.stall_cycles = trace_once.issue_cycle - earliest_issue_cycle;
            trace_once.ready_cycle = trace_once.issue_cycle + inst.latency;
            trace_once.opcode = inst.inst_name;
            resualt_trace.push_back(trace_once);
            //regist dest in scoreboard
            reg_ready[inst.dest_reg] = clk + inst.latency;
            pc++;
        }
        //clock update
        scoreboard_retire(reg_ready, clk);
        clk++ ;      
    }

    return resualt_trace;
}

void print_trace(const std::vector<ScheduleResult>& resualt_trace){
    string block_str;
    cout << std::left;
    cout << std::setw(4)  << "ID"
         << std::setw(8)  << "Opcode"
         << std::setw(7)  << "Issue"
         << std::setw(7)  << "Stall"
         << std::setw(10) << "Blocking"
         << "Ready" << endl;
    for (auto it = resualt_trace.cbegin(); it != resualt_trace.cend(); ++it) {
        const ScheduleResult& result = *it;
        if(result.blocking_registers.size() == 0)
            block_str = "-";
        else if(result.blocking_registers.size() == 1)
            block_str = result.blocking_registers[0];
        else
            block_str = result.blocking_registers[0] + "," + result.blocking_registers[1];
        cout << std::setw(4)  << result.instruction_id
             << std::setw(8)  << result.opcode
             << std::setw(7)  << result.issue_cycle
             << std::setw(7)  << result.stall_cycles
             << std::setw(10) << block_str
             << result.ready_cycle << endl;
    }
}

int main (){
    auto resualt_trace = simulate(program);
    print_trace(resualt_trace);
}