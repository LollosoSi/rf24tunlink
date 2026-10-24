#include <gpiod.hpp>
#include <memory>
#include <filesystem>
#include <poll.h>

// Namespace per isolare libgpiod dalle macro di RF24
namespace RadioHardware {

    struct GpioHandler {
        int fd_irq = -1;
        std::unique_ptr<::gpiod::line_request> request;
        ::gpiod::edge_event_buffer buffer;

        bool setup(int pin_num) {
            try {
                std::string chip_path = "/dev/gpiochip4";
                if (!std::filesystem::exists(chip_path)) chip_path = "/dev/gpiochip0";
                
                auto chip = ::gpiod::chip(chip_path);

                // Configurazione esplicita per evitare conflitti con macro globali
                auto settings = ::gpiod::line_settings()
                    .set_direction(::gpiod::line::direction::INPUT)
                    .set_edge_detection(::gpiod::line::edge::FALLING);

                auto line_cfg = ::gpiod::line_config();
                line_cfg.add_line_settings(static_cast<unsigned int>(pin_num), settings);

                // Creazione della richiesta tramite move-assignment
                auto req = chip.prepare_request()
                    .set_line_config(line_cfg)
                    .set_consumer("DUALRF24_IRQ")
                    .do_request();
                
                request = std::make_unique<::gpiod::line_request>(std::move(req));
                
                buffer = ::gpiod::edge_event_buffer(1);
                fd_irq = request->fd();
                
                return fd_irq != -1;
            } catch (...) {
                return false;
            }
        }

        void clear() {
            if (request) request->read_edge_events(buffer);
        }
    };
}