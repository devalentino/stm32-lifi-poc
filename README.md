# LiFi

LiFi is an STM32-based visible light communication project. It provides a small transport stack for sending bytes over an LED/photodiode-style optical link using STM32 GPIO, timers, and interrupt callbacks.

The project is split into three main layers:

- `LiFi_Transmitter`: converts bytes into timed GPIO output pulses.
- `LiFi_Receiver`: samples incoming light pulses and reconstructs bytes.
- `LiFi_Socket`: wraps the raw byte stream into protocol packets with payload, ACK, NAK, EOT, CRC checking, retries, and application callbacks.

The current firmware target is an STM32F411RE project generated for STM32CubeIDE/CMake. Host-side integration tests use Unity and fake transport components.

## Repository Layout

- `Sources/`: firmware source code and LiFi protocol implementation.
- `Tests/`: host-side protocol tests and fake LiFi transport.
- `Drivers/`: STM32 HAL and CMSIS dependencies.
- `External/`: test dependencies.
- `Startup/`: MCU startup code.

## LiFi Socket API

Include the protocol header:

```c
#include "lifi_protocol.h"
```

The socket API is defined in `Sources/lifi_protocol.h`:

```c
void LiFi_Socket_Init(
    LiFi_Socket_t *socket,
    LiFi_Transmitter_t *transmitter,
    LiFi_Receiver_t *receiver,
    LiFi_Socket_onErrorCallback on_error_callback,
    LiFi_Socket_onTransmissionSuccessfulCallback on_transmission_success_callback,
    LiFi_Socket_onReceiveSuccessfulCallback on_receive_success_callback);

bool LiFi_Socket_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length);

bool LiFi_Socket_Read(LiFi_Socket_t *socket, uint8_t *buffer);
```

`LiFi_Socket_Send` starts a payload transmission. `LiFi_Socket_Read` prepares a socket to receive the next payload into the provided buffer. Both functions return `false` if the socket is already busy.

## Callbacks

Register callbacks when initializing the socket:

```c
static void on_lifi_error(LiFi_Socket_Error_t error, LiFi_Socket_t *socket)
{
  (void)socket;

  if (error == LIFI_SOCKET_CONNECTION_ERROR) {
    /* Transmission was not acknowledged after the retry limit. */
  }
}

static void on_lifi_transmission_success(LiFi_Socket_t *socket)
{
  (void)socket;
  /* Payload was sent and acknowledged by the peer. */
}

static void on_lifi_receive_success(LiFi_Socket_t *socket)
{
  (void)socket;
  /* End-of-transmission was received. The read buffer now contains payload data. */
}
```

## Opening a Socket for Listening

Create and initialize the transmitter, receiver, and socket. Then call `LiFi_Socket_Read` with an application-owned buffer before the peer sends data.

```c
#include <stdint.h>
#include "lifi_protocol.h"

static TIM_HandleTypeDef tx_timer;
static TIM_HandleTypeDef rx_timer;

static LiFi_Transmitter_t transmitter;
static LiFi_Receiver_t receiver;
static LiFi_Socket_t socket;

static uint8_t rx_buffer[64];

static void on_receive_success(LiFi_Socket_t *socket)
{
  (void)socket;
  /*
   * rx_buffer contains the received payload.
   * Process it here or signal the main loop.
   */
}

static void on_error(LiFi_Socket_Error_t error, LiFi_Socket_t *socket)
{
  (void)socket;
  (void)error;
}

void LiFi_App_InitListener(void)
{
  LiFi_Transmitter_Init(&transmitter, &tx_timer, GPIOA, GPIO_PIN_5);
  LiFi_Receiver_Init(&receiver, &rx_timer, GPIOA, GPIO_PIN_1);

  LiFi_Socket_Init(&socket,
                   &transmitter,
                   &receiver,
                   on_error,
                   NULL,
                   on_receive_success);

  if (!LiFi_Socket_Read(&socket, rx_buffer)) {
    /* Socket is busy. Try again later. */
  }
}
```

The receive buffer must stay valid until `on_receive_success` is called.

## Opening a Socket for Sending

Create and initialize the socket, then pass a payload buffer to `LiFi_Socket_Send`.

```c
#include <stdint.h>
#include <string.h>
#include "lifi_protocol.h"

static TIM_HandleTypeDef tx_timer;
static TIM_HandleTypeDef rx_timer;

static LiFi_Transmitter_t transmitter;
static LiFi_Receiver_t receiver;
static LiFi_Socket_t socket;

static uint8_t tx_payload[] = "Hello over LiFi";

static void on_transmission_success(LiFi_Socket_t *socket)
{
  (void)socket;
  /* Payload was delivered and acknowledged. */
}

static void on_error(LiFi_Socket_Error_t error, LiFi_Socket_t *socket)
{
  (void)socket;

  if (error == LIFI_SOCKET_CONNECTION_ERROR) {
    /* Peer did not acknowledge the payload after all retries. */
  }
}

void LiFi_App_InitSender(void)
{
  LiFi_Transmitter_Init(&transmitter, &tx_timer, GPIOA, GPIO_PIN_5);
  LiFi_Receiver_Init(&receiver, &rx_timer, GPIOA, GPIO_PIN_1);

  LiFi_Socket_Init(&socket,
                   &transmitter,
                   &receiver,
                   on_error,
                   on_transmission_success,
                   NULL);
}

void LiFi_App_Send(void)
{
  if (!LiFi_Socket_Send(&socket, tx_payload, (uint8_t)strlen((char *)tx_payload))) {
    /* Socket is busy. Try again later. */
  }
}
```

The transmit buffer must stay valid until `on_transmission_success` or `on_error` is called.

## Hardware Interrupt Integration

The transmitter and receiver are driven by STM32 HAL callbacks. Your application must forward the timer and GPIO interrupts to the LiFi drivers:

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) {
    LiFi_Transmitter_TimerCallback(&transmitter);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_1) {
    LiFi_Receiver_GPIO_Callback(&receiver);
  }
}
```

## Protocol Notes

- Payload packets are acknowledged with ACK packets.
- Corrupted packets are rejected with NAK packets.
- CRC is calculated over the packet type, packet id, payload length, and payload bytes.
- Large payloads are split across protocol packets based on `LIFI_TX_BUFFER_SIZE`.
- `MAX_TRANSMIT_RETRIES_COUNT` controls the retry limit before `LIFI_SOCKET_CONNECTION_ERROR`.

## Building

Debug firmware build:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake -S ./ -B Debug -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
make -C Debug VERBOSE=1 -j
```

Release firmware build:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake -S ./ -B Release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
make -C Release VERBOSE=1 -j
```

Host-side protocol tests:

```sh
cmake -S . -B build-tests -DLIFI_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests
```
