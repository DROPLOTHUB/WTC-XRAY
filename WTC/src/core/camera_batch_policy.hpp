#pragma once
#include <cstddef>
namespace camera_batch_policy {
inline bool complete(bool executed,bool prepared,std::size_t actual,std::size_t requested) {
    return executed && prepared && requested>0 && actual==requested;
}
}
