// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ImixVega and contributors

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace esphome {
namespace i2c_sniffer {

class I2CSniffer : public Component {
 public:
  void set_scl_pin(InternalGPIOPin *pin) { scl_pin_ = pin; }
  void set_sda_pin(InternalGPIOPin *pin) { sda_pin_ = pin; }
  void set_raw_edges(bool value) { raw_edges_ = value; }
  void set_quiet_time_ms(uint16_t value) { quiet_time_ms_ = value; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  // Raw events are stored in DRAM, not in the logging code executed by an ISR.
  static constexpr uint16_t RING_SIZE = 2048;
  static constexpr uint16_t BURST_SIZE = 2048;
  static constexpr uint8_t MAX_FRAME_BYTES = 32;

  // levels: bit0=SCL, bit1=SDA. 1 means logic HIGH.
  struct Edge {
    uint32_t time_us{0};
    uint8_t levels{0};
  };

  struct FrameState {
    bool active{false};
    uint32_t start_us{0};
    uint16_t clocks{0};
    uint8_t bit_count{0};
    uint8_t shift{0};
    uint8_t count{0};
    bool overflow{false};
    uint8_t bytes[MAX_FRAME_BYTES]{};
    uint8_t ack[MAX_FRAME_BYTES]{};
  };

  InternalGPIOPin *scl_pin_{nullptr};
  InternalGPIOPin *sda_pin_{nullptr};
  uint32_t scl_mask_{0};
  uint32_t sda_mask_{0};
  bool raw_edges_{true};
  uint16_t quiet_time_ms_{200};

  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  Edge ring_[RING_SIZE]{};
  volatile uint16_t head_{0};
  volatile uint16_t tail_{0};
  volatile uint8_t last_isr_levels_{0};
  volatile uint32_t scl_edges_{0};
  volatile uint32_t sda_edges_{0};
  volatile uint32_t irq_duplicates_{0};
  volatile uint32_t ring_dropped_{0};

  Edge burst_[BURST_SIZE]{};
  uint16_t burst_size_{0};
  uint8_t burst_initial_levels_{0};
  uint8_t last_loop_levels_{0};
  uint32_t last_burst_us_{0};
  uint32_t burst_number_{0};
  uint32_t frames_total_{0};
  uint32_t partial_total_{0};
  uint32_t last_diag_ms_{0};
  uint32_t last_diag_scl_{0};
  uint32_t last_diag_sda_{0};
  uint32_t last_diag_dropped_{0};

  static void IRAM_ATTR scl_interrupt_(I2CSniffer *self);
  static void IRAM_ATTR sda_interrupt_(I2CSniffer *self);
  void IRAM_ATTR record_edge_();

  void process_event_(const Edge &event);
  void dump_burst_(bool truncated);
  void dump_frame_(const FrameState &frame, const char *terminator, uint32_t end_us, uint32_t &frame_number);
  void dump_command_hint_(const FrameState &frame);
  void dump_raw_edges_();
  void print_diagnostics_();
};

}  // namespace i2c_sniffer
}  // namespace esphome