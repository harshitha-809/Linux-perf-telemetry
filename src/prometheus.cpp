#include "lpt/prometheus.hpp"

#include "lpt/string_util.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace lpt {

std::string escape_label_value(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    for (char c : v) {
        if (c == '\\' || c == '"' || c == '\n') {
            out.push_back('\\');
            if (c == '\n') {
                out.push_back('n');
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

std::string format_labels(const Labels& labels) {
    if (labels.empty()) {
        return {};
    }
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (const auto& kv : labels) {
        if (!first) {
            os << ',';
        }
        first = false;
        os << kv.first << "=\"" << escape_label_value(kv.second) << '"';
    }
    os << '}';
    return os.str();
}

std::string format_prometheus_line(const std::string& name, const Labels& labels, double value) {
    std::ostringstream os;
    os << name << format_labels(labels) << ' ' << std::setprecision(15) << value << '\n';
    return os.str();
}

void MetricsRegistry::set_gauge(const std::string& name, const std::string& help, double value,
                                Labels labels) {
    std::lock_guard<std::mutex> lock(mu_);
    samples_.push_back(Sample{name, help, "gauge", std::move(labels), value});
}

void MetricsRegistry::set_counter(const std::string& name, const std::string& help, double value,
                                  Labels labels) {
    std::lock_guard<std::mutex> lock(mu_);
    samples_.push_back(Sample{name, help, "counter", std::move(labels), value});
}

void MetricsRegistry::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    samples_.clear();
}

std::string MetricsRegistry::render() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::ostringstream os;
    std::string last_name;
    for (const auto& s : samples_) {
        if (s.name != last_name) {
            os << "# HELP " << s.name << ' ' << s.help << '\n';
            os << "# TYPE " << s.name << ' ' << s.type << '\n';
            last_name = s.name;
        }
        os << format_prometheus_line(s.name, s.labels, s.value);
    }
    return os.str();
}

}  // namespace lpt
