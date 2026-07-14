#include "lifi_protocol_internal.h"

#include <string.h>

static uint8_t calculate_crc(const uint8_t *buffer, uint8_t length) {
  uint8_t crc = 0;

  while (length--) {
    crc ^= *buffer++;
    for (uint8_t i = 0; i < 8; ++i) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }

  return crc;
}

void LiFi_Protocol_BuildPackage(uint8_t *destination, PackageType_t type, uint8_t id,
                                const uint8_t *payload, uint8_t payload_length) {
  destination[TX_PACKAGE_PREMBULE_INDEX] = PREAMBULE;
  destination[TX_PACKAGE_START_INDEX] = START_BYTE;
  destination[TX_PACKAGE_PACKAGE_TYPE_INDEX] = (uint8_t)type;
  destination[TX_PACKAGE_ID_INDEX] = id;
  destination[TX_PACKAGE_LENGTH_INDEX] = payload_length;

  if (payload_length > 0) {
    memcpy(destination + TX_PACKAGE_HEADER_BYTES, payload, payload_length);
  }

  uint8_t crc_index = TX_PACKAGE_HEADER_BYTES + payload_length;
  destination[crc_index] = calculate_crc(destination + TX_PACKAGE_PACKAGE_TYPE_INDEX,
                                         payload_length + TX_PACKAGE_HEADER_BYTES - 2);
}

bool LiFi_Protocol_ReceivedPackageCrcIsValid(const LiFi_Socket_t *socket) {
  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  uint8_t crc_index = RX_PACKAGE_HEADER_BYTES + payload_length;

  return socket->rx_package[crc_index] ==
         calculate_crc(socket->rx_package + RX_PACKAGE_PACKAGE_TYPE_INDEX,
                       payload_length + RX_PACKAGE_HEADER_BYTES - 1);
}
