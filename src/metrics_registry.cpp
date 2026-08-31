#include "lpt/metrics_registry.hpp"

#include <iomanip>

namespace lpt {
namespace {

std::string escape_label(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '"' || c == '\n') {
            o.push_back('\\');
            if (c == '\n') {
                o.push_back('n');
            } else {
                o.push_back(c);
            }
        } else {
            o.push_back(c);
        }
    }
    return o;
}

}  // namespace

std::string MetricsRegistry::labels_to_string(const std::map<std::string, std::string>& labels) {
    if (labels.empty()) {
        return "";
    }
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& kv : labels) {
        if (!first) {
            ss << ",";
        }
        first = false;
        ss << kv.first << "=\"" << escape_label(kv.second) << "\"";
    }
    ss << "}";
    return ss.str();
}

void MetricsRegistry::set_gauge(const std::string& name, const std::string& help, double value,
                                const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& fam = families_[name];
    fam.name = name;
    fam.help = help;
    fam.type = MetricType::Gauge;
    fam.samples[labels_to_string(labels)] = value;
}

void MetricsRegistry::set_counter(const std::string& name, const std::string& help, double value,
                                  const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& fam = families_[name];
    fam.name = name;
    fam.help = help;
    fam.type = MetricType::Counter;
    fam.samples[labels_to_string(labels)] = value;
}

void MetricsRegistry::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    families_.clear();
}

std::string MetricsRegistry::render_prometheus() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::ostringstream ss;
    ss << std::setprecision(17);
    for (const auto& kv : families_) {
        const auto& fam = kv.second;
        ss << "# HELP " << fam.name << " " << fam.help << "\n";
        ss << "# TYPE " << fam.name << " "
           << (fam.type == MetricType::Counter ? "counter" : "gauge") << "\n";
        for (const auto& sample : fam.samples) {
            ss << fam.name << sample.first << " " << sample.second << "\n";
        }
    }
    return ss.str();
}

}  // namespace lpt
