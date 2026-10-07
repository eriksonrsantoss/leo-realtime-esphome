import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import microphone, speaker
from esphome.const import CONF_ID

DEPENDENCIES = ["speaker", "microphone"]
AUTO_LOAD = ["socket", "audio"]

CONF_SPEAKER_ID = "speaker_id"
CONF_MICROPHONE = "microphone"
CONF_PORT = "port"
CONF_MIC_PORT = "mic_port"

leo_realtime_ns = cg.esphome_ns.namespace("leo_realtime")
LeoRealtime = leo_realtime_ns.class_("LeoRealtime", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LeoRealtime),
        cv.Required(CONF_SPEAKER_ID): cv.use_id(speaker.Speaker),
        cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
            min_bits_per_sample=16,
            max_bits_per_sample=16,
            min_channels=1,
            max_channels=1,
        ),
        cv.Optional(CONF_PORT, default=8769): cv.port,
        cv.Optional(CONF_MIC_PORT, default=8770): cv.port,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    spk = await cg.get_variable(config[CONF_SPEAKER_ID])
    cg.add(var.set_speaker(spk))

    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    cg.add(var.set_microphone_source(mic_source))

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_mic_port(config[CONF_MIC_PORT]))
