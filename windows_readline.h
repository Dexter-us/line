#pragma once

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

// The Windows CLI uses the operating system's console input instead of
// depending on GNU Readline, which is not available in a standard MSVC setup.
inline char* readline(const char* prompt) {
  std::cout << (prompt ? prompt : "") << std::flush;

  std::string input;
  if (!std::getline(std::cin, input))
    return nullptr;

  auto* result = static_cast<char*>(std::malloc(input.size() + 1));
  if (!result)
    return nullptr;

  std::memcpy(result, input.c_str(), input.size() + 1);
  return result;
}

inline void add_history(const char*) {}
