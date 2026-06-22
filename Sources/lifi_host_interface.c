#include "lifi_host_interface.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>

static void xon(LiFi_HostInterface_t *host_interface) {
  host_interface->huart->Instance->DR = 0x11;
  __HAL_UART_DISABLE_IT(host_interface->huart, UART_IT_TXE);
}

static void xoff(LiFi_HostInterface_t *host_interface) {
  host_interface->huart->Instance->DR = 0x13;
  __HAL_UART_DISABLE_IT(host_interface->huart, UART_IT_TXE);
}

void LiFi_HostInterface_Init(LiFi_HostInterface_t *host_interface, UART_HandleTypeDef *huart,
                             uint8_t *tx_buffer, uint8_t *rx_buffer) {
  host_interface->huart = huart;
  host_interface->state = HOST_INTERFACE_IDLE;

  host_interface->tx_buffer = tx_buffer;
  host_interface->rx_buffer = rx_buffer;
}

void LiFi_HostInterface_ConsumeData(LiFi_HostInterface_t *host_interface) {
  __HAL_UART_ENABLE_IT(host_interface->huart, UART_IT_TXE);
}

void LiFi_HostInterface_onUartTransmitterEmptyCallback(LiFi_HostInterface_t *host_interface) {
  // TODO: evaluate if we send XON or XOFF
}

void LiFi_HostInterface_onUartDataReceivedCallback(LiFi_HostInterface_t *host_interface,
                                                   uint8_t length, uint8_t buffer_address_shift) {
  xoff(host_interface);
  __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);

  if (host_interface->on_buffer_received_callback != NULL &&
      host_interface->on_buffer_received_callback_context != NULL) {
    host_interface->on_buffer_received_callback(
        host_interface->on_buffer_received_callback_context);
  }
}

void LiFi_HostInterface
