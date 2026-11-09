#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stream {
  // Count the complete UDP payload, including the optional encryption prefix,
  // against the 64 KiB Windows buffering limit and 64-segment Linux GSO limit.
  std::size_t video_send_batch_size(std::size_t block_size, std::size_t prefix_size, std::size_t max_batch_bytes);

  // An explicit ceiling counts complete video UDP payloads (including RTP,
  // packet/FEC headers and encryption prefixes), not encoded image bytes.
  // Batch allowance only: one datagram may take longer than a millisecond.
  std::size_t video_pacing_batch_packets(int ceiling_kbps, std::size_t datagram_bytes);
  // Cumulative bit-time, rounded upward once. Used by both admission and send
  // deadlines so fractional packets/ms never become an admission floor.
  // Invalid/overflowing positive demand saturates at signed nanoseconds max.
  std::uint64_t video_pacing_delay_ns(std::uint64_t packets, std::size_t datagram_bytes, int ceiling_kbps);

  template<class Now, class Sleep, class Cancelled>
  bool wait_for_video_pacing(std::chrono::steady_clock::time_point due, Now now, Sleep sleep, Cancelled cancelled) {
    while (!cancelled()) {
      const auto current = now();
      if (current >= due) {
        return true;
      }
      // Explicit low caps can need long packet intervals. Keep media stop
      // responsive without changing the uncapped timer's existing behavior.
      sleep(std::min(due - current, std::chrono::steady_clock::duration(std::chrono::milliseconds(1))));
    }
    return false;
  }

  struct video_pacing_demand_t {
    int encoder_kbps;
    int frames_per_second;
    std::size_t payload_bytes;
    std::size_t datagram_bytes;
    std::size_t frame_header_bytes;
    int fec_percentage;
    int min_parity_shards;
    // PyroWave may split critical and detail independently of RS capacity.
    bool independent_fec_blocks = false;
  };

  struct video_pacing_reservation_t {
    bool admissible = false;
    std::uint64_t packets_per_second = 0;
    std::uint64_t send_ns_per_second = 0;
  };

  // Conservative one-second demand at the negotiated encoder rate/cadence.
  // Includes per-frame padding and per-block parity rounding/minima. This is
  // nominal admission, not a maximum IDR size or a physical network estimate.
  video_pacing_reservation_t video_pacing_reservation(int ceiling_kbps, const video_pacing_demand_t &demand);
  bool reserve_video_pacing_capacity(std::uint64_t &used_ns_per_second, const video_pacing_reservation_t &reservation);

  /**
   * @brief Choose the FEC percentage for a session.
   *
   * @param requested Percentage the client asked for (x-ml-video.fecPercent), or a negative value if it didn't ask.
   * @param host_default The host's fec_percentage.
   * @return The client's request when it is from 0 to 255, otherwise the host's.
   */
  inline int session_fec_percentage(int requested, int host_default) {
    // The client may turn FEC off (0) for itself; the host's own setting starts at 1.
    return requested >= 0 && requested <= 255 ? requested : host_default;
  }

  /// Fixed video send rate used when a client doesn't ask for one: 80% of gigabit.
  constexpr int DEFAULT_VIDEO_SEND_RATE_MBPS = 800;

  /**
   * @brief Choose the video send (burst) rate for a session.
   *
   * @param requested Rate in Mbps the client asked for (x-ml-video.sendRateMbps), or 0 if it didn't ask.
   * @param host_default The host's rate, DEFAULT_VIDEO_SEND_RATE_MBPS.
   * @return The client's request when it is from 100 to 100000 Mbps, otherwise the host's.
   */
  inline int session_video_send_rate(int requested, int host_default) {
    return requested >= 100 && requested <= 100000 ? requested : host_default;
  }

  /**
   * @brief Packets per millisecond at a video send rate.
   *
   * @param send_rate_mbps Rate each frame's packets are sent at.
   * @param block_size Bytes per video packet, as the fixed pacer has always counted them.
   * @return At least one packet per millisecond.
   */
  inline std::size_t video_send_rate_packets_per_ms(int send_rate_mbps, std::size_t block_size) {
    if (block_size == 0) {
      return 1;
    }
    //                               Mbps -> bytes per ms                                        packet
    return std::max<std::size_t>(1, std::size_t(std::max(send_rate_mbps, 1)) * 1'000'000 / 8 / 1000 / block_size);
  }

  inline std::string canonical_codec_name(std::string_view codec) {
    if (codec.empty()) {
      return {};
    }
    std::string lowered(codec);
    for (char &ch : lowered) {
      if (ch >= 'A' && ch <= 'Z') {
        ch = static_cast<char>(ch - 'A' + 'a');
      }
    }
    if (lowered == "h264" || lowered == "h.264") return "H.264";
    if (lowered == "h265" || lowered == "hevc") return "HEVC";
    if (lowered == "av1") return "AV1";
    return std::string(codec);
  }

  struct control_packet_view_t {
    std::uint16_t type = 0;
    std::string_view payload;
  };

  std::optional<control_packet_view_t> decode_control_packet(std::string_view packet_bytes);
  std::vector<std::uint8_t> concat_and_insert(
    std::uint64_t insert_size,
    std::uint64_t slice_size,
    std::string_view data1,
    std::string_view data2
  );
}  // namespace stream
