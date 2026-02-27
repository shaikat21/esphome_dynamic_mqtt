import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_BROKER,
    CONF_PORT,
    CONF_CLIENT_ID,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_TOPIC,
)
from esphome import automation
from esphome.core import CORE

CODEOWNERS = ["@you"]
DEPENDENCIES = ["wifi"]
AUTO_LOAD = []

CONF_CA_CERT = "ca_cert"
CONF_INSECURE = "insecure"
CONF_KEEPALIVE = "keepalive"
CONF_ON_MESSAGE = "on_message"
CONF_THEN = "then"

def _add_lib_deps():
    # Pull PubSubClient automatically for this component
    CORE.add_platformio_option("lib_deps", ["knolleary/PubSubClient@^2.8"])


dynamic_mqtt_ns = cg.esphome_ns.namespace("dynamic_mqtt")
DynamicMqttComponent = dynamic_mqtt_ns.class_("DynamicMqttComponent", cg.Component)

DynamicMqttMessageTrigger = dynamic_mqtt_ns.class_(
    "DynamicMqttMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DynamicMqttComponent),

        cv.Required(CONF_BROKER): cv.string,
        cv.Optional(CONF_PORT, default=8883): cv.port,

        cv.Optional(CONF_CLIENT_ID, default=""): cv.templatable(cv.string),
        cv.Optional(CONF_USERNAME, default=""): cv.templatable(cv.string),
        cv.Optional(CONF_PASSWORD, default=""): cv.templatable(cv.string),

        cv.Optional(CONF_CA_CERT, default=""): cv.string,
        cv.Optional(CONF_INSECURE, default=False): cv.boolean,
        cv.Optional(CONF_KEEPALIVE, default=30): cv.int_range(min=10, max=120),

        cv.Optional(CONF_ON_MESSAGE, default=[]): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_TOPIC): cv.string,
                    cv.Required(CONF_THEN): automation.validate_automation(
                        {
                            cv.GenerateID(): cv.declare_id(DynamicMqttMessageTrigger),
                        }
                    ),
                }
            )
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    _add_lib_deps()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_broker(config[CONF_BROKER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_keepalive(config[CONF_KEEPALIVE]))
    cg.add(var.set_insecure(config[CONF_INSECURE]))
    cg.add(var.set_ca_cert(config[CONF_CA_CERT]))

    client_id = await cg.templatable(config[CONF_CLIENT_ID], [], cg.std_string)
    username  = await cg.templatable(config[CONF_USERNAME],  [], cg.std_string)
    password  = await cg.templatable(config[CONF_PASSWORD],  [], cg.std_string)

    cg.add(var.set_client_id(client_id))
    cg.add(var.set_username(username))
    cg.add(var.set_password(password))

    for m in config.get(CONF_ON_MESSAGE, []):
        trig = cg.new_Pvariable(m[CONF_THEN][CONF_ID], var)
        cg.add(trig.set_topic(m[CONF_TOPIC]))
        cg.add(var.register_trigger(trig))
        await automation.build_automation(
            trig,
            [(cg.std_string, "topic"), (cg.std_string, "payload")],
            m[CONF_THEN],
        )
