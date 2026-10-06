import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import speaker
from esphome.const import CONF_ID

DEPENDENCIES = ["speaker"]
AUTO_LOAD = ["socket"]

CONF_SPEAKER_ID = "speaker_id"
CONF_PORT = "port"

leo_realtime_ns = cg.esphome_ns.namespace("leo_realtime")
LeoRealtime = leo_realtime_ns.class_("LeoRealtime", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LeoRealtime),
        cv.Required(CONF_SPEAKER_ID): cv.use_id(speaker.Speaker),
        cv.Optional(CONF_PORT, default=8769): cv.port,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    spk = await cg.get_variable(config[CONF_SPEAKER_ID])
    cg.add(var.set_speaker(spk))
    cg.add(var.set_port(config[CONF_PORT]))
