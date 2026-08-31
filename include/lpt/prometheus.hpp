#pragma once

#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace lpt {

using Labels = std::map<std::string, std::string>;

class MetricsRegistry {
public:
    void set_gauge(const std::string& name, const std::string& help, double value,
                   Labels labels = {});
    void set_counter(const std::string& name, const std::string& help, double value,
                     Labels labels = {});

    std::string render() const;
    void clear();

private:
    struct Sample {
        std::string name;
        std::string help;
        std::string type;  // "gauge" or "counter"
        Labels labels;
        double value = 0.0;
    };

    mutable std::mutex mu_;
    std::vector<Sample> samples_;
};

std::string escape_label_value(const std::string& v);
std::string format_labels(const Labels& labels);
std::string format_prometheus_line(const std::string& name, const Labels& labels, double value);

}  // namespace lpt
