#include "autoconf.h"

// Exported variables
int autoCalibrate   = 0;
int fixMeasurements = 0;
FILE* calibfd       = NULL;

// Imported variables
extern int doloop;
extern struct pktLatencyStat* latencyStats;

void app_autoconf_init (void) {
	// check if autoconf is enabled
	if (autoCalibrate) {
		printf ("auto-calibration not yet implemented\n");
		exit (-1);
	}

	if (fixMeasurements) {
		printf ("Cant fix the fix measurements, not implemented yet\n");
		exit (-1);
	}
}

void app_autoconf (void) {
	// check if autoconf is enabled
	if (autoCalibrate) {
		doloop = 0;
	}
}