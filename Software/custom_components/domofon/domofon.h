#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "driver/dac_continuous.h"

namespace esphome {
  namespace domofon {
    class Domofon : public Component {
    public:
      static void IRAM_ATTR incoming_call(Domofon *arg);
      void setup() override;
      float get_setup_priority() const override;
      void loop() override;
      void dump_config() override;
      
      void set_call_detect_pin(InternalGPIOPin *pin) { call_detect_pin_ = pin; }
      void set_sw_open_pin(InternalGPIOPin *pin) { sw_open_pin_ = pin; }
      void set_sw_down_pin(InternalGPIOPin *pin) { sw_down_pin_ = pin; }
      void set_amp_sw_pin(InternalGPIOPin *pin) { amp_sw_pin_ = pin; }

      void set_audible_call(bool val) { audible_call_ = val; }
      bool get_audible_call() { return audible_call_; }
      void set_call_accept_delay(int val) { call_accept_delay = val; }
      int get_call_accept_delay() { return call_accept_delay; }
      void set_audio1_before_play_delay(int val) { audio1_before_play_delay = val; }
      int get_audio1_before_play_delay() { return audio1_before_play_delay; }
      void set_audio1_after_play_delay(int val) { audio1_after_play_delay = val; }
      int get_audio1_after_play_delay() { return audio1_after_play_delay; }
      void set_audio2_before_play_delay(int val) { audio2_before_play_delay = val; }
      int get_audio2_before_play_delay() { return audio2_before_play_delay; }
      void set_before_hangup_delay(int val) { before_hangup_delay = val; }
      int get_before_hangup_delay() { return before_hangup_delay; }

      bool get_call_processing() { return should_process_incoming; }
      void set_call_processing_sensor(esphome::binary_sensor::BinarySensor* sensor) { call_processing_sensor_ = sensor; }

      void set_audio_raw(int audio_num, int resp_code, std::string content_type, std::string content);

      void emulate_call();
    private:
      void play_audio(size_t offset);
      int rand_min_max(int min, int max);
      void run_sequence();
    protected:
      InternalGPIOPin *call_detect_pin_;
      InternalGPIOPin *sw_open_pin_;
      InternalGPIOPin *sw_down_pin_;
      InternalGPIOPin *amp_sw_pin_;
      volatile bool should_process_incoming = false;
      volatile int current_step = -2;
      bool audible_call_ = false;
      int call_accept_delay = 1200;
      int audio1_before_play_delay = 200;
      int audio1_after_play_delay = 100;
      int audio2_before_play_delay = 500;
      int before_hangup_delay = 500;

      const int DAC_SAMPLE_RATE = 16000;
      dac_continuous_handle_t dac_handle_{nullptr};
      static const size_t CHUNK_SIZE = 512; 
      std::vector<uint8_t> audio1vec;
      std::vector<uint8_t> audio2vec;

      esphome::binary_sensor::BinarySensor* call_processing_sensor_;
    };
  }
}