/**
 * @file src/nvenc/nvenc_encoded_frame.h
 * @brief Declarations for NVENC encoded frame.
 */
#pragma once

// standard includes
#include <cstdint>
#include <functional>
#include <vector>

namespace nvenc {

  /**
   * @brief Encoded frame.
   */
  struct nvenc_encoded_frame {
    std::vector<uint8_t> data;
    uint64_t frame_index = 0;
    bool idr = false;
    bool after_ref_frame_invalidation = false;
  };

  /**
   * @brief Called during sub-frame readback each time more slices of a frame are encoded.
   * @param data The frame's bitstream so far: every finished slice, in order.
   * @param slices_done Slices encoded so far.
   * @param slices_total Slices in the frame.
   * @param idr Whether the frame is IDR.
   * @param after_ref_frame_invalidation Whether the frame is the first after reference frame invalidation.
   */
  using nvenc_subframe_callback = std::function<void(const std::vector<uint8_t> &data, uint32_t slices_done, uint32_t slices_total, bool idr, bool after_ref_frame_invalidation)>;

}  // namespace nvenc
