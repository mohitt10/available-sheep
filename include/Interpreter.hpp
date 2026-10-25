#pragma once
#include <cstdint>
#include <vector>
#include "Ir.hpp"
 
enum class RunStatus { Ok, DivisionByZero, InputExhausted };
 
struct RunResult {
    RunStatus status = RunStatus::Ok;
    std::vector<std::int32_t> output;
};
 
RunResult run(const Program& program, const std::vector<std::int32_t>& input);
 