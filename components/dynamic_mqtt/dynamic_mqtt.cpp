#include "dynamic_mqtt.h"

#include "esphome/core/application.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace dynamic_mqtt {

static const char *const TAG = "dynamic_mqtt";

void DynamicMqttComponent::setup() {
  this->mqtt_.setServer(this->broker_.c_str(), this->port_);
  this->mqtt_.setKeepAlive(this->keepalive_);

  this->mqtt_.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    this->handle_message_(topic, payload, length);
  });

  // triggers register happens via to_code; link them
  for (auto *t : this->triggers_) this->register_trigger(t);

  this->ensure_tls_config_();
}

void DynamicMqttComponent::ensure_tls_config_() {
#ifdef ARDUINO_ARCH_ESP8266
  if (this->insecure_) {
    this->wifi_secure_.setInsecure();
    ESP_LOGW(TAG, "TLS insecure mode enabled (no cert validation) [ESP8266]");
    return;
  }
  if (!this->ca_cert_pem_.empty()) {
    this->trust_anchor_.reset(new BearSSL::X509List(this->ca_cert_pem_.c_str()));
    this->wifi_secure_.setTrustAnchors(this->trust_anchor_.get());
    ESP_LOGI(TAG, "TLS CA cert set [ESP8266]");
  } else {
    ESP_LOGW(TAG, "No CA cert provided; TLS validation may fail [ESP8266]");
  }
#else
  if (this->insecure_) {
    this->wifi_secure_.setInsecure();
    ESP_LOGW(TAG, "TLS insecure mode enabled (no cert validation) [ESP32]");
    return;
  }
  if (!this->ca_cert_pem_.empty()) {
    this->wifi_secure_.setCACert(this->ca_cert_pem_.c_str());
    ESP_LOGI(TAG, "TLS CA cert set [ESP32]");
  } else {
    ESP_LOGW(TAG, "No CA cert provided; TLS validation may fail [ESP32]");
  }
#endif
}

void DynamicMqttComponent::loop() {
  // WiFi must be connected
  if (!wifi::global_wifi_component || !wifi::global_wifi_component->is_connected()) {
    return;
  }

  // keep mqtt loop pumping if connected
  if (this->mqtt_.connected()) {
    this->mqtt_.loop();
    return;
  }

  // if not connected, attempt every 5 seconds
  uint32_t now = millis();
  if (now - this->last_connect_attempt_ < 5000) return;
  this->last_connect_attempt_ = now;

  this->ensure_connected_();
}

void DynamicMqttComponent::ensure_connected_() {
  std::string cid = this->client_id_.value_or("");
  std::string user = this->username_.value_or("");
  std::string pass = this->password_.value_or("");

  if (cid.empty() || user.empty() || pass.empty()) {
    ESP_LOGW(TAG, "MQTT creds empty; waiting (cid=%d user=%d pass=%d)",
             (int) !cid.empty(), (int) !user.empty(), (int) !pass.empty());
    return;
  }

  ESP_LOGI(TAG, "Connecting MQTT TLS to %s:%u as user=%s", this->broker_.c_str(), this->port_, user.c_str());

  bool ok = this->mqtt_.connect(cid.c_str(), user.c_str(), pass.c_str());
  if (!ok) {
    ESP_LOGW(TAG, "MQTT connect failed, state=%d", this->mqtt_.state());
    return;
  }

  ESP_LOGI(TAG, "MQTT connected");

  // subscribe all configured topics (from triggers)
  for (auto *t : this->triggers_) {
    if (!t->topic().empty()) {
      bool s = this->mqtt_.subscribe(t->topic().c_str(), 1);
      ESP_LOGI(TAG, "Subscribe %s => %s", t->topic().c_str(), s ? "OK" : "FAIL");
    }
  }
}

bool DynamicMqttComponent::publish(const std::string &topic, const std::string &payload, bool retain) {
  if (!this->mqtt_.connected()) return false;
  return this->mqtt_.publish(topic.c_str(), payload.c_str(), retain);
}

void DynamicMqttComponent::handle_message_(char *topic_c, uint8_t *payload, unsigned int length) {
  std::string topic(topic_c ? topic_c : "");
  std::string data;
  data.reserve(length);
  for (unsigned int i = 0; i < length; i++) data.push_back((char) payload[i]);

  for (auto *t : this->triggers_) {
    if (t->topic() == topic) {
      t->trigger(topic, data);
    }
  }
}

}  // namespace dynamic_mqtt
}  // namespace esphome
