#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/speaker/speaker.h"

namespace esphome {
namespace leo_realtime {

class LeoRealtime : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }
  void set_microphone_source(microphone::MicrophoneSource *source) { this->microphone_source_ = source; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_mic_port(uint16_t port) { this->mic_port_ = port; }

 protected:
  void service_speaker_();
  void service_microphone_();
  bool open_listener_(int &fd, uint16_t port);
  void close_client_();
  void close_server_();
  void close_mic_client_();
  void close_mic_server_();
  void flush_pending_();

  speaker::Speaker *speaker_{nullptr};
  microphone::MicrophoneSource *microphone_source_{nullptr};

  uint16_t port_{8769};
  uint16_t mic_port_{8770};

  int server_fd_{-1};
  int client_fd_{-1};
  int mic_server_fd_{-1};
  int mic_client_fd_{-1};

  std::vector<uint8_t> pending_{};
  size_t pending_offset_{0};

  std::vector<uint8_t> mic_pending_{};
  size_t mic_pending_offset_{0};
  static constexpr size_t MIC_PENDING_LIMIT = 32768;
};

}  // namespace leo_realtime
}  // namespace esphome
