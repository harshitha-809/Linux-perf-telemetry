#pragma once

#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace lpt {

enum class MetricType { Gauge, Counter };

struct MetricFamily {
    std::string name;
    std::string help;
    MetricType type = MetricType::Gauge;
    // labels serialized as canonical {k="v",...} -> value
    std::map<std::string, double> samples;
};

class MetricsRegistry {
public:
    void set_gauge(const std::string& name, const std::string& help, double value,
                   const std::map<std::string, std::string>& labels = {});
    void set_counter(const std::string& name, const std::string& help, double value,
                     const std::map<std::string, std::string>& labels = {});

    std::string render_prometheus() const;
    void clear();

private:
    static std::string labels_to_string(const std::map<std::string, std::string>& labels);
    mutable std::mutex mu_;
    std::map<std::string, MetricFamily> families_;
};

}  // namespace lpt
