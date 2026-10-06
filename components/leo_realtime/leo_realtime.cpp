#include "leo_realtime.h"

#include "esphome/core/log.h"

namespace esphome {
namespace leo_realtime {

static const char *const TAG = "leo_realtime";

void LeoRealtime::setup() {
  // Diagnostic build: intentionally do not open the TCP socket yet.
  // This isolates boot stability from the networking/audio path.
  ESP_LOGI(TAG, "Diagnostic build loaded; TCP listener disabled");
}

void LeoRealtime::loop() {
  // Intentionally empty in the diagnostic build.
}

void LeoRealtime::flush_pending_() {
}

void LeoRealtime::close_client_() {
  this->pending_.clear();
  this->pending_offset_ = 0;
  this->client_fd_ = -1;
}

void LeoRealtime::close_server_() {
  this->server_fd_ = -1;
}

void LeoRealtime::dump_config() {
  ESP_LOGCONFIG(TAG, "Léo Realtime:");
  ESP_LOGCONFIG(TAG, "  Diagnostic build: TCP listener disabled");
  ESP_LOGCONFIG(TAG, "  Configured TCP port: %u", this->port_);
}

}  // namespace leo_realtime
}  // namespace esphome
