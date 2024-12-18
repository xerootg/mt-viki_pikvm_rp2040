// copyright 2024 by xerootg
// licensed: GPL-3.0
// vagly based on https://gist.github.com/mharsch/52b0a99a945711385fb0120020d8226d
// with timings for 4 port model: https://www.reddit.com/r/pikvm/comments/yt0lne/comment/l1osgfe/
// ws2812.pio is directly from waveshare's example code which is based on the pico-sdk example code
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

// control the ws2812
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

// yes, im going to use strings on a microcontroller. its idling anyway.
#include <iostream>
#include <sstream>
#include <string>

#define MAX_PORT 4 // set to the number of ports on the KVM switch, i have 4 but the "internet" says 8 are a thing. who trusts the internet? i sure don't.
#define INVALID_PORT 0 // just a value to indicate that the port is invalid or serve as a default value

// connected to 'D-' pin on mini USB
#define OUTPUT_PIN 15

// connected to 'D+' pin on mini USB
#define INPUT_PIN 14

#define LED_PIN 16

#define LOW 0
#define HIGH 1

PIO pio;
uint sm;
uint offset;

// each color is {r, g, b}
int colors[9][3] = {
  {255, 0, 0}, // red
  {0, 255, 0}, // green
  {0, 0, 255}, // blue
  {255, 255, 0}, // yellow
  {255, 0, 255}, // purple
  {0, 255, 255}, // cyan
  {255, 255, 255}, // white
  {0, 128, 0}, // dark green
  {0, 0, 128} // dark blue
};

enum MessageType {
  to_kvm,
  from_kvm,
  from_ipkvm_host,
  to_ipkvm_host,
  watchdog
};

enum MessageStatus {
  message_sending, // Used to indicate that a message is being sent
  message_sent, // Used when a message is sent
  message_received, // Used when a message is received and valid
  invalid_value, // Used when junk is received
  receive_timeout, // Used when a message is not received within a certain time
  ok
};

const char UNKNOWN[] = "unknown";
const char TO_KVM[] = "to_kvm";
const char FROM_KVM[] = "from_kvm";
const char FROM_IPKVM_HOST[] = "from_ipkvm_host";
const char TO_IPKVM_HOST[] = "to_ipkvm_host";
const char WATCHDOG[] = "watchdog";

const char* MessageTypeToString(MessageType type) {
    switch (type) {
        case to_kvm: return TO_KVM;
        case from_kvm: return FROM_KVM;
        case from_ipkvm_host: return FROM_IPKVM_HOST;
        case to_ipkvm_host: return TO_IPKVM_HOST;
        case watchdog: return WATCHDOG;
        default: return UNKNOWN;
    }
}

const char MESSAGE_SENDING[] = "message_sending";
const char MESSAGE_SENT[] = "message_sent";
const char MESSAGE_RECEIVED[] = "message_received";
const char INVALID_VALUE[] = "invalid_value";
const char RECEIVE_TIMEOUT[] = "receive_timeout";
const char OK[] = "ok";

const char* MessageStatusToString(MessageStatus status) {
    switch (status) {
        case message_sending: return MESSAGE_SENDING;
        case message_sent: return MESSAGE_SENT;
        case message_received: return MESSAGE_RECEIVED;
        case invalid_value: return INVALID_VALUE;
        case receive_timeout: return RECEIVE_TIMEOUT;
        case ok: return OK;
        default: return UNKNOWN;
    }
}

void send_status_json(MessageType type, MessageStatus status, int port)
{
  const char* type_str = MessageTypeToString(type);
  const char* status_str = MessageStatusToString(status);

  std::ostringstream oss;
  oss << "{\"type\": \"" << type_str << "\", \"status\": \"" << status_str << "\", \"port\": " << port << "}";

  std::string json_str = oss.str();
  std::cout << json_str << std::endl;
}


void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

void put_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t mask = (green << 16) | (red << 8) | (blue << 0);
    put_pixel(mask);
}

void set_led(int state) {
  // there's a ws2812 on the board, gpio 16.
  // we can use this to set the color of the led
  put_rgb(colors[state][0], colors[state][1], colors[state][2]);
}

void setup() {
  // digital I/O
  gpio_init(OUTPUT_PIN);
  gpio_set_dir(OUTPUT_PIN, GPIO_OUT);
  // set output high
  gpio_put(OUTPUT_PIN, HIGH);

  gpio_init(INPUT_PIN);
  gpio_set_dir(INPUT_PIN, GPIO_IN);
  stdio_init_all();
}

int get_port() {
  // wait for input to go low. if it's already low, wait for it to go high.
  while (gpio_get(INPUT_PIN) == LOW) {
    sleep_ms(1);
  }

  // wait for input to go high
  while (gpio_get(INPUT_PIN) == HIGH) {
    sleep_ms(1);
  }

  // start timing
  absolute_time_t start = get_absolute_time();

  // wait for input to go low again
  while (gpio_get(INPUT_PIN) == LOW) {
    sleep_ms(1);
  }

  // stop timing
  absolute_time_t stop = get_absolute_time();

  // calculate the duration in milliseconds
  double duration_ms = absolute_time_diff_us(start, stop) / 1000.0;

  // calculate the port number
  int current_port = (int) round(duration_ms / 6);
  
  // then, check if it's a valid port number

  if (current_port >= 1 && current_port <= MAX_PORT) {
    send_status_json(from_kvm, message_received, current_port);
    return current_port;
  } else {
    send_status_json(from_kvm, invalid_value, current_port);
    return INVALID_PORT;
  }
}

void set_port(int p) {
  send_status_json(to_kvm, message_sending, p);
  gpio_put(OUTPUT_PIN, LOW);
  sleep_ms(12);
  gpio_put(OUTPUT_PIN, HIGH);
  sleep_ms(12);
  gpio_put(OUTPUT_PIN, LOW);
  sleep_ms(9 + (4 * p));
  gpio_put(OUTPUT_PIN, HIGH);
  send_status_json(to_kvm, message_sent, p);
}

void loop() {
  int temp_port = INVALID_PORT;
  absolute_time_t timeout = make_timeout_time_ms(1000); // 1 second timeout
  while (absolute_time_diff_us(get_absolute_time(), timeout) > 0 && temp_port == INVALID_PORT) {
    int c = getchar_timeout_us(0);
    watchdog_update();

    // if it's a timeout, return so we can check for a valid port
    if(c == PICO_ERROR_TIMEOUT) {
      continue;
    }
    // determine if c is a valid port
    if(c >= '1' && c <= '0' + MAX_PORT) {
      temp_port = c - '0';
      send_status_json(from_ipkvm_host, message_received, temp_port);
      break; // exit loop so we can check for success
    } else {
      int c = 0;
      while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {} // clear the buffer

      temp_port = INVALID_PORT;
      send_status_json(from_ipkvm_host, invalid_value, INVALID_PORT);
    }
  }

  watchdog_update();

  // if somehow we get a valid port, set it. RUN.
  if(temp_port != INVALID_PORT) {
    set_port(temp_port);
  }

  watchdog_update(); // port setting is done, update the watchdog so getting the port doesn't trigger it
  int current_port = get_port();
  watchdog_update();

  if(current_port != INVALID_PORT) // return is invalid port if not valid or timeout. the host will have json to indicate this.
  {
    set_led(current_port);
    send_status_json(to_ipkvm_host, message_sent, current_port);
  } else {
    set_led(INVALID_PORT);
  }
}

int main()
{
  setup();

  if(watchdog_caused_reboot())
  {
    send_status_json(watchdog, receive_timeout, INVALID_PORT);
  } else {
    send_status_json(watchdog, ok, INVALID_PORT);
  }

  pio = pio0;
  sm = 0;
  offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, sm, offset, 16, 800000, true);

  send_status_json(from_ipkvm_host, ok, INVALID_PORT);
  watchdog_enable(5000, 1);

  while (true) {
    loop();
    watchdog_update();
  }
  return -1; // should never reach here
}
