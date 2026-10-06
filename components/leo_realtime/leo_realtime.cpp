#include "leo_realtime.h"

#include <cerrno>
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <fcntl.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <unistd.h>
#endif

namespace esphome {
namespace leo_realtime {

static const char *const TAG = "leo_realtime";

void LeoRealtime::setup() {
  // IMPORTANT: lwIP sockets cannot be created safely this early in ESPHome setup.
  // Defer TCP server creation until loop(), after networking/lwIP is ready.
  ESP_LOGI(TAG, "Deferred TCP build: socket creation will wait for network readiness");
}

void LeoRealtime::loop() {
#ifdef USE_ESP32
  if (this->server_fd_ < 0) {
    this->server_fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (this->server_fd_ < 0)
      return;

    int yes = 1;
    setsockopt(this->server_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(this->port_);

    if (::bind(this->server_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 ||
        ::listen(this->server_fd_, 1) < 0) {
      this->close_server_();
      return;
    }

    int flags = fcntl(this->server_fd_, F_GETFL, 0);
    if (flags >= 0)
      fcntl(this->server_fd_, F_SETFL, flags | O_NONBLOCK);

    ESP_LOGI(TAG, "Deferred TCP listener active on port %u", this->port_);
    return;
  }

  // Diagnostic only: accept and close. Speaker/streaming remains inactive.
  sockaddr_in client_addr{};
  socklen_t len = sizeof(client_addr);
  int fd = ::accept(this->server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &len);
  if (fd >= 0) {
    ESP_LOGI(TAG, "Diagnostic TCP client accepted and closed");
    ::close(fd);
  }
#endif
}

void LeoRealtime::flush_pending_() {
  if (this->pending_.empty() || this->speaker_ == nullptr)
    return;

  const size_t remaining = this->pending_.size() - this->pending_offset_;
  const size_t accepted = this->speaker_->play(this->pending_.data() + this->pending_offset_, remaining);
  this->pending_offset_ += accepted;

  if (this->pending_offset_ >= this->pending_.size()) {
    this->pending_.clear();
    this->pending_offset_ = 0;
  }
}

void LeoRealtime::close_client_() {
#ifdef USE_ESP32
  if (this->client_fd_ >= 0) {
    ::close(this->client_fd_);
    this->client_fd_ = -1;
  }
#endif
  this->pending_.clear();
  this->pending_offset_ = 0;
}

void LeoRealtime::close_server_() {
#ifdef USE_ESP32
  if (this->server_fd_ >= 0) {
    ::close(this->server_fd_);
    this->server_fd_ = -1;
  }
#endif
}

void LeoRealtime::dump_config() {
  ESP_LOGCONFIG(TAG, "Léo Realtime:");
  ESP_LOGCONFIG(TAG, "  Deferred TCP diagnostic; speaker/streaming inactive");
  ESP_LOGCONFIG(TAG, "  Speaker reference: %s", this->speaker_ != nullptr ? "loaded" : "missing");
  ESP_LOGCONFIG(TAG, "  TCP port: %u", this->port_);
}

}  // namespace leo_realtime
}  // namespace esphome
