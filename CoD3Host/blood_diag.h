#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <mutex>
namespace BloodDiag {
inline bool On(){static const bool on=getenv("COD3_BLOOD_DIAG")!=nullptr;return on;}
inline bool Test(){static const bool on=getenv("COD3_BLOOD_DIAG_TEST")!=nullptr;return On()&&on;}
inline void Log(const char* format,...){if(!On())return;static std::mutex m;std::lock_guard<std::mutex> lock(m);printf("BDIAG ");va_list args;va_start(args,format);vprintf(format,args);va_end(args);printf("\n");fflush(stdout);}
}
