#include "lifi_host_interface.h"
#include "lifi_host_inreface.h"

void LiFi_HostInterface_Init(LiFi_HostInterface_t *host_interface, UART_HandleTypeDef *huart) {
    host_interface->huart = huart;
    host_interface->state = HOST_INTERFACE_IDLE;
}

void LiFi_HostInterface_onReceiveSuccessfulCallback(LiFi_HostInterface_t *host_interface, uint8_t length) {
    host_interface->rx_bytes_received = length;

    if (host_interface->on_buffer_received_callback != NULL && host_interface->on_buffer_received_callback_context != NULL) {
        host_interface->on_buffer_received_callback(host_interface->on_buffer_received_callback_context);
    }
}