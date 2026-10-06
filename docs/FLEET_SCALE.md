# Fleet-Scale Telemetry Architecture (Design Proposal)

*Note: This document is a conceptual systems design proposal. It illustrates how the local `lpt-agent` could be scaled to monitor thousands of nodes in a distributed fleet. It does not reflect an actively deployed production cluster in this repository.*

## 1. Architecture Overview
At scale, telemetry shifts from single-node diagnosis to a distributed pipeline:
**Host (Target) → `lpt-agent` → Prometheus (Regional Scraping) → Global Aggregation (Thanos/Cortex/Mimir) → Visualization/Alerting (Grafana).**
The agent remains responsible for efficiently reading `/proc` and exposing raw metrics, while the infrastructure handles centralization and durability.

## 2. Service Discovery
A static `prometheus.yml` configuration cannot scale to thousands of ephemeral machines. Instead, Prometheus relies on **Service Discovery** (e.g., Consul, Kubernetes endpoints, or AWS EC2 discovery). As new hosts spin up, they register with the discovery service, and Prometheus dynamically begins routing HTTP GET requests to their `lpt-agent` endpoints without manual intervention.

## 3. Metrics Collection (Pull Model)
The `lpt-agent` utilizes Prometheus's standard **pull model**. This is highly advantageous for host telemetry because:
* It prevents the host from wasting resources trying to push data to an overwhelmed central server.
* It inherently bounds the agent's memory footprint—metrics are generated on demand and dropped, leaving buffering to the aggregator.
* Failure is isolated; if Prometheus goes down, the agent continues functioning safely.

## 4. Global Aggregation
Individual Prometheus servers scale vertically until they reach storage or memory limits. To query metrics across multiple regions or thousands of instances, a global aggregator like **Thanos, Cortex, or Mimir** is introduced. These systems deduplicate metrics from multiple Prometheus replicas and store long-term historical data in cheap object storage (like S3), providing a unified global query view.

## 5. Cardinality Control
Fleet-scale telemetry generates massive time-series databases. High-cardinality labels (e.g., embedding a dynamic PID or transient thread ID as a label in `lpt_process_cpu_usage`) will quickly exhaust Prometheus memory (OOM). Aggregation design dictates that highly transient dimensions must be aggregated or dropped at the agent or scraper level, preserving only stable dimensions (e.g., `hostname`, `service_name`, or `datacenter`).

## 6. Reliability and Failure Modes
If a single `lpt-agent` fails, Prometheus records an `up == 0` metric, triggering a host-down alert while the rest of the fleet is unaffected. If a regional Prometheus instance fails, redundancy (e.g., running pairs of Prometheus servers scraping the same targets) ensures data continuity, with global aggregators deduplicating the overlapping streams.

## 7. Capacity and Scaling Considerations
* **Scrape Intervals:** Tuned based on necessity (e.g., 15s for critical metrics, 60s for generic nodes) to balance network bandwidth and storage costs.
* **Retention:** Local Prometheus storage is kept brief (e.g., 2–14 days), while long-term object storage retains downsampled data for months.

## 8. Production Considerations
A true production rollout requires rigorous operational constraints:
* **Security:** Implementing mTLS or reverse proxies in front of `lpt-agent` to ensure only authorized scrapers can access the `/metrics` endpoint.
* **Rate Limiting:** Capping concurrent scrapes to prevent accidental CPU saturation on the host.
* **Version Management:** Using configuration management (Ansible/Chef) or container orchestration (DaemonSets) for safe, phased binary rollouts.
