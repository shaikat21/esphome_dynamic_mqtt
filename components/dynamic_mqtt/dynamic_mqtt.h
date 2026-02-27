#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#endif

#ifdef USE_ESP8266
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecureBearSSL.h>
  namespace BearSSL {
    class X509List;
  }
#endif

#include <PubSubClient.h>

namespace esphome {
namespace dynamic_mqtt {

class DynamicMqttComponent;

class DynamicMqttMessageTrigger : public Trigger<std::string, std::string> {
 public:
  explicit DynamicMqttMessageTrigger(DynamicMqttComponent *parent) : parent_(parent) {}
  void set_topic(const std::string &t) { topic_ = t; }
  const std::string &topic() const { return topic_; }

 private:
  DynamicMqttComponent *parent_;
  std::string topic_;
};

class DynamicMqttComponent : public Component {
 public:
  void set_broker(const std::string &b) { broker_ = b; }
  void set_port(uint16_t p) { port_ = p; }
  void set_keepalive(uint16_t s) { keepalive_ = s; }
  void set_insecure(bool v) { insecure_ = v; }
  void set_ca_cert(const std::string &pem) { ca_cert_pem_ = pem; }

  void set_client_id(TemplateArg<std::string> v) { client_id_ = v; }
  void set_username(TemplateArg<std::string> v) { username_ = v; }
  void set_password(TemplateArg<std::string> v) { password_ = v; }

  void register_trigger(DynamicMqttMessageTrigger *t) { triggers_.push_back(t); }

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }

  // Publish helper (optional for your code/lambdas)
  bool publish(const std::string &topic, const std::string &payload, bool retain = false);

 private:
  void ensure_tls_config_();
  void ensure_connected_();
  void handle_message_(char *topic, uint8_t *payload, unsigned int length);

  std::string broker_;
  uint16_t port_{8883};
  uint16_t keepalive_{30};
  bool insecure_{false};
  std::string ca_cert_pem_;

  TemplateArg<std::string> client_id_;
  TemplateArg<std::string> username_;
  TemplateArg<std::string> password_;

  uint32_t last_connect_attempt_{0};
  uint32_t last_loop_ms_{0};

#ifdef ARDUINO_ARCH_ESP8266
  std::unique_ptr<BearSSL::X509List> trust_anchor_;
  BearSSL::WiFiClientSecure wifi_secure_;
#else
  WiFiClientSecure wifi_secure_;
#endif

  PubSubClient mqtt_{wifi_secure_};

  std::vector<DynamicMqttMessageTrigger *> triggers_;
};

}  // namespace dynamic_mqtt
}  // namespace esphome
