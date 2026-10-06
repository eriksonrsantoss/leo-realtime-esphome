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
#ifdef USE_ESP32
  this->server_fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (this->server_fd_ < 0) {
    ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
    this->mark_failed();
    return;
  }

  int yes = 1;
  setsockopt(this->server_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(this->port_);

  if (::bind(this->server_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "bind() failed on port %u: errno=%d", this->port_, errno);
    this->close_server_();
    this->mark_failed();
    return;
  }

  if (::listen(this->server_fd_, 1) < 0) {
    ESP_LOGE(TAG, "listen() failed: errno=%d", errno);
    this->close_server_();
    this->mark_failed();
    return;
  }

  int flags = fcntl(this->server_fd_, F_GETFL, 0);
  if (flags >= 0)
    fcntl(this->server_fd_, F_SETFL, flags | O_NONBLOCK);

  // Diagnostic stage 3: the configured speaker pointer is present, but we do
  // not call start(), play(), finish(), stop(), or any other speaker method.
  ESP_LOGI(TAG, "Diagnostic stage 3: TCP listening on port %u; speaker reference loaded but untouched", this->port_);
#else
  ESP_LOGE(TAG, "This component currently requires ESP32");
  this->mark_failed();
#endif
}

void LeoRealtime::loop() {
#ifdef USE_ESP32
  if (this->server_fd_ < 0)
    return;

  sockaddr_in client_addr{};
  socklen_t len = sizeof(client_addr);
  int fd = ::accept(this->server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &len);
  if (fd >= 0) {
    ESP_LOGI(TAG, "Diagnostic TCP client connected; speaker remains untouched");
    ::close(fd);
  }
#endif
}

void LeoRealtime::flush_pending_() {
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
  ESP_LOGCONFIG(TAG, "  Diagnostic stage 3: TCP enabled");
  ESP_LOGCONFIG(TAG, "  Speaker reference: %s", this->speaker_ != nullptr ? "loaded (untouched)" : "missing");
  ESP_LOGCONFIG(TAG, "  TCP port: %u", this->port_);
}

}  // namespace leo_realtime
}  // namespace esphome
