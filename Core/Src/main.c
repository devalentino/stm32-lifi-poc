#include "main.h"



void setup() {
	setupReceiver();
	setupTransmitter();
	setupUtils();
}

int main() {
    setup();

    while (true) {
    	if (isButtonPressed()) {
    		enableLedIndicator();
    		transmitOne();
    	} else {
    		disableLedIndicator();
    		transmitZero();
    	}
    }
}
