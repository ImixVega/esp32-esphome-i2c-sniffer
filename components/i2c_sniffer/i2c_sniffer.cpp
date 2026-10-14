// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ImixVega and contributors

#include "i2c_sniffer.h"

#include "esphome/core/log.h"
#include "soc/gpio_struct.h"
#include "esp_timer.h"

#include <cstdio>

namespace esphome {
namespace i2c_sniffer {

static const char *const TAG = "i2c_sniffer";

void I2CSniffer::setup() {
  if (scl_pin_ == nullptr || sda_pin_ == nullptr) {
    ESP_LOGE(TAG, "Missing SCL/SDA pins");
    mark_failed();
    return;
  }
  const uint8_t scl = scl_pin_->get_pin();
  const uint8_t sda = sda_pin_->get_pin();
  if (scl == sda || scl >= 32 || sda >= 32) {
    ESP_LOGE(TAG, "Classic ESP32: choose two distinct GPIO 0..31");
    mark_failed();
    return;
  }
  scl_mask_ = 1UL << scl;
  sda_mask_ = 1UL << sda;

  // IMPORTANT: setup() preserves pull-up flags from YAML.
  // Do not call pin_mode(FLAG_INPUT) again; doing so clears configured pull-ups.
  scl_pin_->setup();
  sda_pin_->setup();

  const uint32_t gpio = GPIO.in;
  last_isr_levels_ = ((gpio & scl_mask_) ? 1 : 0) | ((gpio & sda_mask_) ? 2 : 0);
  last_loop_levels_ = last_isr_levels_;

  scl_pin_->attach_interrupt(&I2CSniffer::scl_interrupt_, this, gpio::INTERRUPT_ANY_EDGE);
  sda_pin_->attach_interrupt(&I2CSniffer::sda_interrupt_, this, gpio::INTERRUPT_ANY_EDGE);
  last_diag_ms_ = millis();
  ESP_LOGI(TAG, "RAW v1.0.0 READY GPIO%u=SCL GPIO%u=SDA idle=%u%u; raw=%s quiet=%ums",
           scl, sda, last_isr_levels_ & 1, (last_isr_levels_ >> 1) & 1,
           raw_edges_ ? "ON" : "OFF", quiet_time_ms_);
}

void I2CSniffer::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 passive I2C RAW sniffer v1.0.0");
  LOG_PIN("  SCL: ", scl_pin_);
  LOG_PIN("  SDA: ", sda_pin_);
}

void IRAM_ATTR I2CSniffer::scl_interrupt_(I2CSniffer *self) { self->record_edge_(); }
void IRAM_ATTR I2CSniffer::sda_interrupt_(I2CSniffer *self) { self->record_edge_(); }

void IRAM_ATTR I2CSniffer::record_edge_() {
  // Both pins sampled from one hardware register to avoid inconsistent reads.
  const uint32_t value = GPIO.in;
  const uint8_t levels = ((value & scl_mask_) ? 1 : 0) | ((value & sda_mask_) ? 2 : 0);
  // esp_timer_get_time() is lock-free and usable in ESP-IDF ISR context.
  const uint32_t now = static_cast<uint32_t>(esp_timer_get_time());

  portENTER_CRITICAL_ISR(&mux_);
  const uint8_t changed = levels ^ last_isr_levels_;
  if (changed == 0) {
    ++irq_duplicates_;
  } else {
    if (changed & 1) ++scl_edges_;
    if (changed & 2) ++sda_edges_;
    const uint16_t next = (head_ + 1) % RING_SIZE;
    if (next == tail_) {
      ++ring_dropped_;
    } else {
      ring_[head_].time_us = now;
      ring_[head_].levels = levels;
      head_ = next;
    }
    last_isr_levels_ = levels;
  }
  portEXIT_CRITICAL_ISR(&mux_);
}

void I2CSniffer::loop() {
  // Drain only a bounded number per loop to keep ESPHome/WiFi responsive.
  for (uint16_t n = 0; n < 512; ++n) {
    Edge edge;
    bool has_event = false;
    portENTER_CRITICAL(&mux_);
    if (tail_ != head_) {
      edge = ring_[tail_];
      tail_ = (tail_ + 1) % RING_SIZE;
      has_event = true;
    }
    portEXIT_CRITICAL(&mux_);
    if (!has_event) break;
    process_event_(edge);
  }

  // Don't flush while another part of the burst is queued.
  bool queue_empty;
  portENTER_CRITICAL(&mux_);
  queue_empty = (head_ == tail_);
  portEXIT_CRITICAL(&mux_);
  const uint32_t now_us = static_cast<uint32_t>(esp_timer_get_time());
  if (queue_empty && burst_size_ > 0 &&
      static_cast<uint32_t>(now_us - last_burst_us_) > uint32_t(quiet_time_ms_) * 1000U) {
    dump_burst_(false);
  }

  if (static_cast<uint32_t>(millis() - last_diag_ms_) >= 5000U) {
    last_diag_ms_ = millis();
    print_diagnostics_();
  }
}

void I2CSniffer::process_event_(const Edge &edge) {
  // Split the capture into bursts separated by idle time.
  if (burst_size_ && static_cast<uint32_t>(edge.time_us - last_burst_us_) > uint32_t(quiet_time_ms_) * 1000U)
    dump_burst_(false);
  if (burst_size_ == 0) burst_initial_levels_ = last_loop_levels_;
  if (burst_size_ >= BURST_SIZE) dump_burst_(true);
  if (burst_size_ == 0) burst_initial_levels_ = last_loop_levels_;
  burst_[burst_size_++] = edge;
  last_burst_us_ = edge.time_us;
  last_loop_levels_ = edge.levels;
}

void I2CSniffer::dump_frame_(const FrameState &f, const char *end_type,
                            uint32_t end_us, uint32_t &frame_number) {
  ++frame_number;
  ++frames_total_;
  const bool partial = (f.bit_count != 0) || (f.count == 0) || f.overflow;
  if (partial) ++partial_total_;
  if (f.count != 0) {
    const uint8_t addr = f.bytes[0] >> 1;
    ESP_LOGI(TAG, "  FRAME %lu: ADDR=0x%02X %s %s | bytes=%u clocks=%u duration=%luus | %s%s",
             static_cast<unsigned long>(frame_number), addr,
             (f.bytes[0] & 1) ? "READ" : "WRITE", f.ack[0] ? "NACK" : "ACK",
             f.count, f.clocks, static_cast<unsigned long>(end_us - f.start_us),
             end_type, partial ? " [PARTIAL]" : "");
    // Every captured byte is preserved, including 0x00 and NACK'ed bytes.
    char payload[280];
    int pos = 0;
    for (uint8_t i = 0; i < f.count && pos < 250; ++i) {
      const int count = snprintf(payload + pos, sizeof(payload) - pos, " %02X[%c]", f.bytes[i], f.ack[i] ? 'N' : 'A');
      if (count < 0) break;
      pos += count;
    }
    payload[pos] = 0;
    ESP_LOGI(TAG, "    BYTES:%s", payload);
    if (addr == 0x00) ESP_LOGW(TAG, "    ADDR=0x00: possible general-call OR unreliable capture; not discarded");
    dump_command_hint_(f);
  } else {
    ESP_LOGW(TAG, "  FRAME %lu: NO COMPLETE BYTE | clocks=%u duration=%luus | %s [PARTIAL]",
             static_cast<unsigned long>(frame_number), f.clocks,
             static_cast<unsigned long>(end_us - f.start_us), end_type);
  }
  if (f.bit_count) {
    char bits[10];
    for (uint8_t i = 0; i < f.bit_count; ++i)
      bits[i] = (f.shift & (1U << (f.bit_count - 1 - i))) ? '1' : '0';
    bits[f.bit_count] = 0;
    ESP_LOGW(TAG, "    TAIL: %u/9 bits (%s), missing remainder of current byte/ACK", f.bit_count, bits);
  }
  if (f.overflow) ESP_LOGW(TAG, "    FRAME exceeded %u bytes", MAX_FRAME_BYTES);
}

void I2CSniffer::dump_command_hint_(const FrameState &f) {
  if (f.count < 2 || (f.bytes[0] & 1)) return;
  const uint8_t addr = f.bytes[0] >> 1;
  if ((addr == 0x44 || addr == 0x45) && f.count >= 3) {
    const uint16_t cmd = (uint16_t(f.bytes[1]) << 8) | f.bytes[2];
    if (cmd == 0x2400) ESP_LOGI(TAG, "    HINT: SHT3x single-shot high repeatability (no clock stretch)");
    else if (cmd == 0x2C06) ESP_LOGI(TAG, "    HINT: SHT3x single-shot high repeatability (clock stretch)");
  }
  if ((addr == 0x44 || addr == 0x45) && f.count == 2) {
    if (f.bytes[1] == 0xFD) ESP_LOGI(TAG, "    HINT: SHT4x high precision measurement");
    if (f.bytes[1] == 0xF6) ESP_LOGI(TAG, "    HINT: SHT4x medium precision measurement");
    if (f.bytes[1] == 0xE0) ESP_LOGI(TAG, "    HINT: SHT4x low precision measurement");
  }
  if (addr == 0x70 && f.count >= 3) {
    const uint16_t cmd = (uint16_t(f.bytes[1]) << 8) | f.bytes[2];
    if (cmd == 0x3517) ESP_LOGI(TAG, "    HINT: SHTC3 wake-up");
    else if (cmd == 0xB098) ESP_LOGI(TAG, "    HINT: SHTC3 sleep");
    else if (cmd == 0xEFC8) ESP_LOGI(TAG, "    HINT: SHTC3 read ID");
    else if (cmd == 0x7866) ESP_LOGI(TAG, "    HINT: SHTC3 measure temperature first");
    else if (cmd == 0x58E0) ESP_LOGI(TAG, "    HINT: SHTC3 measure humidity first");
  }
  if (addr == 0x38 && f.bytes[1] == 0xAC) ESP_LOGI(TAG, "    HINT: AHT20-style measurement command");
}

void I2CSniffer::dump_burst_(bool truncated) {
  if (!burst_size_) return;
  ++burst_number_;
  const uint32_t started = burst_[0].time_us;
  const uint32_t last = burst_[burst_size_ - 1].time_us;
  ESP_LOGI(TAG, "==== BURST #%lu events=%u duration=%luus start(SCL,SDA)=%u%u%s ====",
           static_cast<unsigned long>(burst_number_), burst_size_,
           static_cast<unsigned long>(last - started),
           burst_initial_levels_ & 1, (burst_initial_levels_ >> 1) & 1,
           truncated ? " [TRUNCATED]" : "");

  uint8_t previous = burst_initial_levels_;
  FrameState f;
  uint32_t frame_number = 0;
  uint32_t starts = 0, stops = 0, unframed_clocks = 0, simultaneous = 0;

  for (uint16_t i = 0; i < burst_size_; ++i) {
    const Edge &edge = burst_[i];
    const uint8_t changed = previous ^ edge.levels;
    const bool prev_scl = (previous & 1) != 0;
    const bool scl = (edge.levels & 1) != 0;
    const bool prev_sda = (previous & 2) != 0;
    const bool sda = (edge.levels & 2) != 0;

    if (changed == 3) ++simultaneous;
    // A START/STOP requires SCL staying HIGH while SDA changes.
    if (changed == 2 && prev_scl && scl) {
      if (prev_sda && !sda) {
        ++starts;
        if (f.active) dump_frame_(f, "REPEATED_START", edge.time_us, frame_number);
        f = FrameState{};
        f.active = true;
        f.start_us = edge.time_us;
      } else if (!prev_sda && sda) {
        ++stops;
        if (f.active) {
          dump_frame_(f, "STOP", edge.time_us, frame_number);
          f = FrameState{};
        } else {
          ESP_LOGW(TAG, "  STOP without START at +%luus", static_cast<unsigned long>(edge.time_us - started));
        }
      }
    }

    // Sample data ONLY on SCL rising edges.
    if (!prev_scl && scl) {
      if (!f.active) {
        ++unframed_clocks;
      } else {
        ++f.clocks;
        if (f.bit_count < 8) {
          f.shift = uint8_t((f.shift << 1) | (sda ? 1 : 0));
          ++f.bit_count;
        } else {
          if (f.count < MAX_FRAME_BYTES) {
            f.bytes[f.count] = f.shift;
            f.ack[f.count] = sda ? 1 : 0;
            ++f.count;
          } else {
            f.overflow = true;
          }
          f.bit_count = 0;
          f.shift = 0;
        }
      }
    }
    previous = edge.levels;
  }

  if (f.active) dump_frame_(f, "NO_STOP/END_OF_BURST", last, frame_number);
  ESP_LOGI(TAG, "  SUMMARY frames=%lu START=%lu STOP=%lu outside_clocks=%lu simultaneous=%lu",
           static_cast<unsigned long>(frame_number), static_cast<unsigned long>(starts),
           static_cast<unsigned long>(stops), static_cast<unsigned long>(unframed_clocks),
           static_cast<unsigned long>(simultaneous));
  if (simultaneous) ESP_LOGW(TAG, "  Simultaneous pin changes: ISR may have missed edges; check shorter wires / bus speed");
  if (raw_edges_) dump_raw_edges_();
  burst_size_ = 0;
}

void I2CSniffer::dump_raw_edges_() {
  // Raw snapshot: +time_us:SCL,SDA. Shows ALL captured transitions, even outside START/STOP.
  // 12 tokens per log line to avoid filling the ESPHome logger buffer.
  char line[260];
  for (uint16_t i = 0; i < burst_size_; i += 12) {
    int pos = 0;
    for (uint16_t j = i; j < burst_size_ && j < i + 12; ++j) {
      const Edge &e = burst_[j];
      const int n = snprintf(line + pos, sizeof(line) - pos, " +%lu:%u%u",
                             static_cast<unsigned long>(e.time_us - burst_[0].time_us),
                             e.levels & 1, (e.levels >> 1) & 1);
      if (n < 0 || pos + n >= static_cast<int>(sizeof(line))) break;
      pos += n;
    }
    line[pos] = 0;
    ESP_LOGI(TAG, "  RAW %04u-%04u:%s", i, (i + 11 < burst_size_ ? i + 11 : burst_size_ - 1), line);
  }
}

void I2CSniffer::print_diagnostics_() {
  uint32_t scl_count, sda_count, drop_count, dupe_count;
  uint16_t queued;
  portENTER_CRITICAL(&mux_);
  scl_count = scl_edges_;
  sda_count = sda_edges_;
  drop_count = ring_dropped_;
  dupe_count = irq_duplicates_;
  queued = (head_ + RING_SIZE - tail_) % RING_SIZE;
  portEXIT_CRITICAL(&mux_);
  const uint32_t value = GPIO.in;
  const unsigned scl = !!(value & scl_mask_);
  const unsigned sda = !!(value & sda_mask_);
  ESP_LOGI(TAG, "DIAG SCL=%u SDA=%u | SCLedges=%lu (+%lu) SDAedges=%lu (+%lu) | queue=%u lost=%lu (+%lu)",
           scl, sda, static_cast<unsigned long>(scl_count), static_cast<unsigned long>(scl_count - last_diag_scl_),
           static_cast<unsigned long>(sda_count), static_cast<unsigned long>(sda_count - last_diag_sda_),
           queued, static_cast<unsigned long>(drop_count), static_cast<unsigned long>(drop_count - last_diag_dropped_));
  ESP_LOGI(TAG, "DIAG bursts=%lu frames=%lu partial=%lu duplicate_ISRs=%lu",
           static_cast<unsigned long>(burst_number_), static_cast<unsigned long>(frames_total_),
           static_cast<unsigned long>(partial_total_), static_cast<unsigned long>(dupe_count));
  if (drop_count > last_diag_dropped_) ESP_LOGW(TAG, "RAW ring overflowed: capture may be incomplete");
  last_diag_scl_ = scl_count;
  last_diag_sda_ = sda_count;
  last_diag_dropped_ = drop_count;
}

}  // namespace i2c_sniffer
}  // namespace esphome