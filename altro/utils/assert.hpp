// Copyright [2021] Optimus Ride Inc.

#pragma once
#include <string>

#ifndef NDEBUG
#define ALTRO_ASSERT(Expr, Msg) ;
#else
#define ALTRO_ASSERT(Expr, Msg) ;
#endif

namespace altro {
namespace utils {

void AssertMsg(bool expr, const std::string& msg, const char* expr_str, int line, const char* file);

constexpr bool AssertionsActive() { 
#ifndef NDEBUG
  return true; 
#else
  return false;
#endif
}

}  // namespace utils
}  // namespace altro