# Léo Realtime ESPHome

Experimental ESPHome external component for M5Stack Atom Echo.

It receives raw PCM over a local TCP socket and writes the bytes directly to an existing ESPHome `speaker`.

Initial stream format used by this project:
- 16 kHz
- signed 16-bit PCM
- stereo
- little-endian

Example:

```yaml
external_components:
  - source: github://eriksonrsantoss/leo-realtime-esphome@main
    components: [leo_realtime]

leo_realtime:
  speaker_id: echo_speaker
  port: 8769
```

No credentials, API keys, Wi-Fi passwords, or Home Assistant tokens belong in this repository.
