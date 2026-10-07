#include "leo_realtime.h"
#include "esphome/components/audio/audio.h"

#include <cerrno>
#include <algorithm>
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
  // lwIP sockets are deliberately created later from loop(), after networking is ready.
  ESP_LOGI(TAG, "Stage 14: deferred speaker + microphone TCP listeners");

  if (this->microphone_source_ != nullptr) {
    this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
      if (this->mic_client_fd_ < 0 || data.empty())
        return;

      // Keep the callback non-blocking. loop() owns socket transmission.
      const size_t queued = this->mic_pending_.size() - this->mic_pending_offset_;
      if (queued + data.size() > MIC_PENDING_LIMIT) {
        ESP_LOGW(TAG, "Stage 14: microphone queue full; dropping %u bytes",
                 static_cast<unsigned>(data.size()));
        return;
      }

      if (this->mic_pending_offset_ > 0) {
        this->mic_pending_.erase(this->mic_pending_.begin(),
                                 this->mic_pending_.begin() + this->mic_pending_offset_);
        this->mic_pending_offset_ = 0;
      }
      this->mic_pending_.insert(this->mic_pending_.end(), data.begin(), data.end());
    });
  }
}

bool LeoRealtime::open_listener_(int &fd, uint16_t port) {
#ifdef USE_ESP32
  if (fd >= 0)
    return true;

  fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (fd < 0)
    return false;

  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 ||
      ::listen(fd, 1) < 0) {
    ::close(fd);
    fd = -1;
    return false;
  }

  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0)
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  ESP_LOGI(TAG, "Stage 14: TCP listener active on port %u", port);
  return true;
#else
  return false;
#endif
}

void LeoRealtime::loop() {
#ifdef USE_ESP32
  this->open_listener_(this->server_fd_, this->port_);
  this->open_listener_(this->mic_server_fd_, this->mic_port_);

  this->service_speaker_();
  this->service_microphone_();
#endif
}

void LeoRealtime::service_speaker_() {
#ifdef USE_ESP32
  if (this->server_fd_ < 0)
    return;

  if (this->client_fd_ < 0) {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int fd = ::accept(this->server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &len);
    if (fd >= 0) {
      int flags = fcntl(fd, F_GETFL, 0);
      if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      this->client_fd_ = fd;
      ESP_LOGI(TAG, "Stage 14: speaker client connected; 16-bit/2ch/16000 Hz");
      this->speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, 2, 16000));
      this->speaker_->set_mute_state(false);
      this->speaker_->set_volume(1.0f);
      this->speaker_->start();
    }
    return;
  }

  this->flush_pending_();
  if (!this->pending_.empty())
    return;

  uint8_t buffer[1024];
  const int received = ::recv(this->client_fd_, buffer, sizeof(buffer), 0);

  if (received > 0) {
    const size_t accepted = this->speaker_->play(buffer, static_cast<size_t>(received));
    if (accepted < static_cast<size_t>(received)) {
      this->pending_.assign(buffer + accepted, buffer + received);
      this->pending_offset_ = 0;
    }
    return;
  }

  if (received == 0) {
    this->flush_pending_();
    ESP_LOGI(TAG, "Stage 14: speaker client disconnected");
    this->close_client_();
    return;
  }

  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGW(TAG, "Stage 14: speaker socket error errno=%d", errno);
    this->close_client_();
  }
#endif
}

void LeoRealtime::service_microphone_() {
#ifdef USE_ESP32
  if (this->mic_server_fd_ < 0 || this->microphone_source_ == nullptr)
    return;

  if (this->mic_client_fd_ < 0) {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int fd = ::accept(this->mic_server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &len);
    if (fd >= 0) {
      int flags = fcntl(fd, F_GETFL, 0);
      if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      this->mic_client_fd_ = fd;
      this->mic_pending_.clear();
      this->mic_pending_offset_ = 0;
      ESP_LOGI(TAG, "Stage 14: microphone client connected; starting 16-bit mono capture");
      this->microphone_source_->start();
    }
    return;
  }

  // Detect a client-side close without consuming application data.
  uint8_t probe;
  const int peeked = ::recv(this->mic_client_fd_, &probe, 1, MSG_PEEK);
  if (peeked == 0) {
    ESP_LOGI(TAG, "Stage 14: microphone client disconnected; stopping capture");
    this->close_mic_client_();
    return;
  }
  if (peeked < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGW(TAG, "Stage 14: microphone socket error errno=%d", errno);
    this->close_mic_client_();
    return;
  }

  if (this->mic_pending_offset_ >= this->mic_pending_.size())
    return;

  const uint8_t *ptr = this->mic_pending_.data() + this->mic_pending_offset_;
  const size_t remaining = this->mic_pending_.size() - this->mic_pending_offset_;
  const int sent = ::send(this->mic_client_fd_, ptr, remaining, MSG_DONTWAIT);

  if (sent > 0) {
    this->mic_pending_offset_ += static_cast<size_t>(sent);
    if (this->mic_pending_offset_ >= this->mic_pending_.size()) {
      this->mic_pending_.clear();
      this->mic_pending_offset_ = 0;
    }
  } else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGW(TAG, "Stage 14: microphone send error errno=%d", errno);
    this->close_mic_client_();
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

void LeoRealtime::close_mic_client_() {
  if (this->microphone_source_ != nullptr)
    this->microphone_source_->stop();
#ifdef USE_ESP32
  if (this->mic_client_fd_ >= 0) {
    ::close(this->mic_client_fd_);
    this->mic_client_fd_ = -1;
  }
#endif
  this->mic_pending_.clear();
  this->mic_pending_offset_ = 0;
}

void LeoRealtime::close_mic_server_() {
#ifdef USE_ESP32
  if (this->mic_server_fd_ >= 0) {
    ::close(this->mic_server_fd_);
    this->mic_server_fd_ = -1;
  }
#endif
}

void LeoRealtime::dump_config() {
  ESP_LOGCONFIG(TAG, "Léo Realtime:");
  ESP_LOGCONFIG(TAG, "  Stage 14: speaker output + on-demand microphone capture");
  ESP_LOGCONFIG(TAG, "  Speaker PCM: signed 16-bit little-endian, 16 kHz, stereo");
  ESP_LOGCONFIG(TAG, "  Microphone PCM: signed 16-bit little-endian, 16 kHz, mono");
  ESP_LOGCONFIG(TAG, "  Speaker reference: %s", this->speaker_ != nullptr ? "loaded" : "missing");
  ESP_LOGCONFIG(TAG, "  Microphone source: %s", this->microphone_source_ != nullptr ? "loaded" : "missing");
  ESP_LOGCONFIG(TAG, "  Speaker TCP port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Microphone TCP port: %u", this->mic_port_);
}

}  // namespace leo_realtime
}  // namespace esphome
