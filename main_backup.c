#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "tusb.h"

#include "pico/mutex.h"
#include "hardware/uart.h"

#define FLAG_VALUE 123

auto_init_mutex( my_mutex);

void trylock(int core) {
	uint32_t owner_out;
	if (mutex_try_enter(&my_mutex, &owner_out)) {
		printf("from core%d: in Mutex!!\n", core);
		sleep_ms(10);
		mutex_exit(&my_mutex);
		printf("from core%d: Out Mutex!!\n", core);

		sleep_ms(10);
	} else {
		printf("from core%d: locked %d\n", core, owner_out);
		sleep_ms(10);
	}
}

char buffer[34];
int counter = 0;
char *rc = "\r\n";

void core1_entry() {

	buffer[32] = '\r';
	buffer[33] = '\n';

	// multicore_fifo_push_blocking(FLAG_VALUE);

	//uint32_t g = multicore_fifo_pop_blocking();
	uint32_t g = 1;

	// if (g != FLAG_VALUE)
	//    printf("Hmm, that's not right on core 1!\n");
	//else
	//   printf("Its all gone well on core 1!");

	while (1) {

		while (tud_cdc_available()) {

//Read the next available byte in the serial receive buffer
			buffer[counter] = getchar();
//Message coming in (check not terminating character) and guard for over message size
			if ((++counter) == 32) {
				fwrite(buffer, 1, counter, stdout);
				//fwrite(rc, 1, 1, stdout);
				//fflush(stdout);

				counter = 0;
			}
		}

		tight_loop_contents();
	}
}

int main() {
	stdio_init_all();

	while (!tud_cdc_connected()) {
		sleep_ms(100);
	}

	multicore_launch_core1(core1_entry);

	char *h = "hello";

	while (true) {
		//fwrite(h, 1, 6, stdout);
		//fwrite(rc, 1, 2, stdout);
		//fflush(stdout);

		sleep_ms(10000);
		tight_loop_contents();
	}
}
