#pragma once

#include "lpt/collectors.hpp"
#include "lpt/prometheus.hpp"
#include "lpt/types.hpp"

#include <vector>

namespace lpt {

class SnapshotExporter {
public:
    explicit SnapshotExporter(std::size_t process_limit = 64);

    void update(const SystemSnapshot& snap);
    std::string render() const;

private:
    std::size_t process_limit_;
    MetricsRegistry registry_;
    CpuSnapshot prev_cpu_{};
    bool have_prev_cpu_ = false;
};

}  // namespace lpt
