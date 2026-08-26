#include "esphome/core/log.h"
#include "domofon.h"
#include "before_audio.h"
#include "after_audio.h"

namespace esphome {
    namespace domofon {
        static const char *TAG = "domofon.component";

        void IRAM_ATTR Domofon::incoming_call(Domofon *arg) {
            if (!arg->should_process_incoming) {
                arg->current_step = -1;
                arg->should_process_incoming = true;
            }
        }

        void Domofon::emulate_call() {
            if (!should_process_incoming) {
                current_step = -1;
                should_process_incoming = true;
            }
        }

        void Domofon::setup() {
            // Flag
            this->should_process_incoming = false;
            this->current_step = -2;

            // Other pins
            this->sw_open_pin_->setup();
            this->sw_open_pin_->pin_mode(gpio::FLAG_OUTPUT);
            this->sw_open_pin_->digital_write(false);
            this->sw_down_pin_->setup();
            this->sw_down_pin_->pin_mode(gpio::FLAG_OUTPUT);
            this->sw_down_pin_->digital_write(true);
            this->amp_sw_pin_->setup();
            this->amp_sw_pin_->pin_mode(gpio::FLAG_OUTPUT);
            this->amp_sw_pin_->digital_write(false);

            // Call detect
            this->call_detect_pin_->setup();
            this->call_detect_pin_->pin_mode(gpio::FLAG_INPUT);
            this->call_detect_pin_->attach_interrupt(&Domofon::incoming_call, this, gpio::INTERRUPT_FALLING_EDGE);

            // Load audio to RAM
            audio1vec.clear();
            for (int i = 0; i < before_raw_len; i++) {
                audio1vec.push_back(before_raw[i]);
            }
            audio2vec.clear();
            for (int i = 0; i < after_raw_len; i++) {
                audio2vec.push_back(after_raw[i]);
            }

            // DAC
            dac_continuous_config_t cont_cfg = {
                .chan_mask = DAC_CHANNEL_MASK_CH0,
                .desc_num = 4,
                .buf_size = 2048,
                .freq_hz = DAC_SAMPLE_RATE,
                .offset = 0,
                .clk_src = DAC_DIGI_CLK_SRC_APLL,
                .chan_mode = DAC_CHANNEL_MODE_SIMUL
            };
            if (dac_continuous_new_channels(&cont_cfg, &dac_handle_) == ESP_OK) {
                dac_continuous_enable(dac_handle_);
                ESP_LOGI(TAG, "DAC continuous DMA stream started successfully!");
            } else {
                ESP_LOGE(TAG, "Failed to allocate continuous DAC channels.");
            }
        }

        void Domofon::play_audio(size_t offset) {
            if (dac_handle_ == nullptr) return;
            int which = (this->current_step == 2) ? 0 : 1;
            size_t wav_length = (which <= 0) ? audio1vec.size() : audio2vec.size();
            int bytes_to_write = std::min(wav_length - offset, CHUNK_SIZE);
            if (bytes_to_write <= 0 || offset >= wav_length) {
                // Next
                int ms_to_next = (which == 0) ? rand_min_max(audio1_after_play_delay * 0.9, audio1_after_play_delay * 1.1) : rand_min_max(before_hangup_delay * 0.9, before_hangup_delay * 1.1);
                this->set_timeout("play_finish", ms_to_next, [this, which]() {
                    if (which == 0) {
                        this->current_step = 3;
                    } else {
                        this->current_step = 8;
                    }
                    this->run_sequence();
                });

                // Return
                return;
            }
            size_t bytes_written = 0;
            esp_err_t err = dac_continuous_write(
                dac_handle_,
                (which <= 0) ? (audio1vec.data() + offset) : (audio2vec.data() + offset),
                bytes_to_write, 
                &bytes_written, 
                pdMS_TO_TICKS(40)
            );
            int ms_to_next = (int)(((float)(CHUNK_SIZE * 1000.0f)) / ((float)DAC_SAMPLE_RATE));
            this->set_timeout("play_chunk", ms_to_next, [this, offset]() {
                this->play_audio(offset + CHUNK_SIZE);
            });
        }
        
        int Domofon::rand_min_max(int min, int max) {
            return (rand() % max) + min;
        }

        void Domofon::set_audio_raw(int audio_num, int resp_code, std::string content_type, std::string content) {
            if (resp_code < 0 || content.length() <= 0 || resp_code != 200 || content_type.length() <= 0 || this->current_step > -2) {
                // Error
                if (this->current_step > -2) {
                    ESP_LOGE(TAG, "URL Request - cannot do it now as current state is playing!");
                } else {
                    audio1vec.clear();
                    for (int i = 0; i < before_raw_len; i++) {
                        audio1vec.push_back(before_raw[i]);
                    }
                    audio2vec.clear();
                    for (int i = 0; i < after_raw_len; i++) {
                        audio2vec.push_back(after_raw[i]);
                    }
                    if (resp_code < 0) {
                        ESP_LOGE(TAG, "URL Request - request failed!");
                    } else if (content.length() <= 0) {
                        ESP_LOGE(TAG, "URL Request - response body empty!");
                    } else if (content_type.length() <= 0) {
                        ESP_LOGE(TAG, "URL Request - wrong MIME type!");
                    } else {
                        ESP_LOGE(TAG, "URL Request - incorrect response code!");
                    }
                }
            } else {
                if (audio_num <= 0) {
                    audio1vec.assign(content.begin(), content.end());
                } else {
                    audio2vec.assign(content.begin(), content.end());
                }
                ESP_LOGI(TAG, "URL Request - successfully assigned new 16KS audio%d, size %d samples!", audio_num + 1, (audio_num <= 0) ? audio1vec.size() : audio2vec.size());
            }
        }

        void Domofon::run_sequence() {
            ESP_LOGI(TAG, "Handling step %d of call", current_step);
            if (current_step <= 0) {
                if (audible_call_) {
                    this->amp_sw_pin_->digital_write(true);
                }
                this->set_timeout("step1", rand_min_max(call_accept_delay * 0.9, call_accept_delay * 1.1), [this]() {
                    this->current_step = 1;
                    this->run_sequence();
                });
            } else if (current_step == 1) {
                this->sw_down_pin_->digital_write(false);
                this->set_timeout("step2", rand_min_max(audio1_before_play_delay * 0.9, audio1_before_play_delay * 1.1), [this]() {
                    this->current_step = 2;
                    this->run_sequence();
                });
            } else if (current_step == 2) {
                this->play_audio(0);
            } else if (current_step == 3) {
                this->sw_open_pin_->digital_write(true);
                this->set_timeout("step4", rand_min_max(300, 800), [this]() {
                    this->current_step = 4;
                    this->run_sequence();
                });
            } else if (current_step == 4) {
                this->sw_open_pin_->digital_write(false);
                this->set_timeout("step5", rand_min_max(800, 1800), [this]() {
                    this->current_step = 5;
                    this->run_sequence();
                });
            } else if (current_step == 5) {
                this->sw_open_pin_->digital_write(true);
                this->set_timeout("step6", rand_min_max(300, 800), [this]() {
                    this->current_step = 6;
                    this->run_sequence();
                });
            } else if (current_step == 6) {
                this->sw_open_pin_->digital_write(false);
                this->set_timeout("step7", rand_min_max(audio2_before_play_delay * 0.9, audio2_before_play_delay * 1.1), [this]() {
                    this->current_step = 7;
                    this->run_sequence();
                });
            } else if (current_step == 7) {
                this->play_audio(0);
            } else {
                this->sw_down_pin_->digital_write(true);
                this->amp_sw_pin_->digital_write(false);
                this->set_timeout("final", 800, [this]() {
                    this->current_step = -2;
                    this->should_process_incoming = false;
                    if (call_processing_sensor_ != nullptr) call_processing_sensor_->publish_state(false);
                });
            }
        }

        void Domofon::loop() {
            // Step
            if (this->should_process_incoming && this->current_step == -1) {
                this->current_step = 0;
                if (call_processing_sensor_ != nullptr) call_processing_sensor_->publish_state(true);
                run_sequence();
            }
        }

        void Domofon::dump_config(){
            ESP_LOGCONFIG(TAG, "Domofon custom component");
        }

        float Domofon::get_setup_priority() const {
            return setup_priority::HARDWARE;
        }
    }
}