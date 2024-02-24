/*
 * TelemetryPrinter.cpp
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "TelemetryPrinter.h"

TelemetryPrinter::TelemetryPrinter(const char *filename) {
	this->filename = filename;

	if (this->filename) {
		printf("Output will be printed at %s\n", this->filename);

		outfile = new std::ofstream(filename);
		if (!outfile->good()) {
			printf("Couldn't open output file, can't log data\n");
			outfile->close();
			delete outfile;
			outfile = nullptr;
		}

	} else {
		printf("This session won't save CSV output\n");
	}

}

TelemetryPrinter::~TelemetryPrinter() {
	if (outfile) {
		outfile->close();
		delete outfile;
		outfile = nullptr;
	}
}

void TelemetryPrinter::tick() {

	static bool first_tick = true;

	std::vector<Telemetry*>::iterator it;

	if (first_tick && outfile) {
		first_tick = false;

		it = elements.begin();

		while (it != elements.end()) {

			std::string *telemetrydata = (*it)->telemetry_collect(1000);
			const std::string *elementnames = (*it)->get_element_names();

			(*outfile) << (*it)->get_name().c_str() << Settings::csv_divider;

			for (int i = 0; i < (*it)->size()-1; i++) {
				(*outfile) << Settings::csv_divider;
			}

			++it;
		}
		(*outfile) << "\n";

		it = elements.begin();

		while (it != elements.end()) {

			std::string *telemetrydata = (*it)->telemetry_collect(1000);
			const std::string *elementnames = (*it)->get_element_names();

			//(*outfile) << Settings::csv_divider;

			for (int i = 0; i < (*it)->size(); i++) {
				(*outfile) << elementnames[i].c_str() << Settings::csv_divider;
			}

			++it;
		}

		(*outfile) << "\n";

	}

	it = elements.begin();

	std::system("clear");

	while (it != elements.end()) {

		//if (outfile)
		//	(*outfile) << Settings::csv_divider;

		std::string *telemetrydata = (*it)->telemetry_collect(1000);
		const std::string *elementnames = (*it)->get_element_names();
		printf("\t\t%s Telemetry entries\n", (*it)->get_name().c_str());
		for (int i = 0; i < (*it)->size(); i++) {

			if (outfile)
				(*outfile) << telemetrydata[i].c_str() << Settings::csv_divider;

			printf("%s: %s\t", elementnames[i].c_str(),
					telemetrydata[i].c_str());
		}
		printf("\n\n");

		++it;
	}

	if (outfile)
		(*outfile) << "\n";

}

