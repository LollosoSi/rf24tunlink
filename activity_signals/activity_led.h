/*
 * activity_led.h
 *
 *  Created on: 27 Jun 2024
 *      Author: Andrea Roccaccino & AI
 */

#pragma once

#include "ActivitySignalInterface.h"

#include <gpiod.hpp>
#include <optional>

class ActivityLed : public ActivitySignalInterface {
    int led_gpio = 10;
    std::string chip_path;
	std::optional<::gpiod::line_request> request;

public:
    ActivityLed(const Settings s) : ActivitySignalInterface(s) {
        led_gpio = s.activity_led_gpio;
        find_gpiochip();

		auto chip = ::gpiod::chip(chip_path);     

        // Configuration v2
        ::gpiod::line_settings settings;
        settings.set_direction(::gpiod::line::direction::OUTPUT);

        ::gpiod::line_config line_cfg;
        line_cfg.add_line_settings(led_gpio, settings);

        ::gpiod::request_config req_cfg;
        req_cfg.set_consumer("rf24tunlink_activity_led");

        // Assegnazione tramite move
        request = chip.prepare_request()
                      .set_request_config(req_cfg)
                      .set_line_config(line_cfg)
                      .do_request();
    }

    void stop() {
		request.reset(); // release
	}

    void set_state(bool on_off) {
        if (request) {
            request->set_value(led_gpio, on_off ? ::gpiod::line::value::ACTIVE : ::gpiod::line::value::INACTIVE);
        }
    }

    void find_gpiochip() {
        for (const auto &entry : std::filesystem::directory_iterator("/dev/")) {
            if (entry.path().string().find("gpiochip") != std::string::npos) {
                try {
                    ::gpiod::chip c(entry.path().string());
                    if (led_gpio < (int)c.get_info().num_lines()) {
                        chip_path = entry.path().string();
                        break;
                    }
                } catch (...) { continue; }
            }
        }
        if (chip_path.empty()) throw std::runtime_error("GPIO chip not found");
    }
};