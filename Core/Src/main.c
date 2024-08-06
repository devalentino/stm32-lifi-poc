#include "main.h"


uint32_t received_value;

void setup() {
	setupReceiver();
	setupTransmitter();
	setupUtils();
}

int main() {
    setup();

    while (true) {
    	startReceiver();

    	if (isButtonPressed()) {
    		enableLedIndicator();
    		transmitOne();
    	} else {
    		disableLedIndicator();
    		transmitZero();
    	}

    	received_value = receive();
    }
}
