#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/speaker/speaker.h"

namespace esphome {
namespace leo_realtime {

class LeoRealtime : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }
  void set_port(uint16_t port) { this->port_ = port; }

 protected:
  void close_client_();
  void close_server_();
  void flush_pending_();

  speaker::Speaker *speaker_{nullptr};
  uint16_t port_{8769};
  int server_fd_{-1};
  int client_fd_{-1};
  std::vector<uint8_t> pending_{};
  size_t pending_offset_{0};
};

}  // namespace leo_realtime
}  // namespace esphome
