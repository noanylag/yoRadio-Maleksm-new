#line 1 "D:\\Maleksm\\yoRadio-1\\yoRadio\\src\\async-mqtt-client\\AsyncMqttClient\\MessageProperties.hpp"
#pragma once

struct AsyncMqttClientMessageProperties {
  uint8_t qos;
  bool dup;
  bool retain;
};
