from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import binary_sensor

domofon_ns = cg.esphome_ns.namespace('domofon')
Domofon = domofon_ns.class_('Domofon', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Domofon),
    cv.Required('call_detect_pin'): pins.gpio_input_pin_schema,
    cv.Required('sw_open_pin'): pins.gpio_output_pin_schema,
    cv.Required('sw_down_pin'): pins.gpio_output_pin_schema,
    cv.Required('amp_sw_pin'): pins.gpio_output_pin_schema,
    cv.Required('audible_call'): cv.boolean,
    cv.Required('call_accept_delay'): cv.positive_time_period_milliseconds,
    cv.Required('audio1_before_play_delay'): cv.positive_time_period_milliseconds,
    cv.Required('audio1_after_play_delay'): cv.positive_time_period_milliseconds,
    cv.Required('audio2_before_play_delay'): cv.positive_time_period_milliseconds,
    cv.Required('before_hangup_delay'): cv.positive_time_period_milliseconds,
    cv.Required('call_processing_sensor_id'): cv.use_id(binary_sensor.BinarySensor),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    call_detect_pin = await cg.gpio_pin_expression(config['call_detect_pin'])
    cg.add(var.set_call_detect_pin(call_detect_pin))
    sw_open_pin = await cg.gpio_pin_expression(config['sw_open_pin'])
    cg.add(var.set_sw_open_pin(sw_open_pin))
    sw_down_pin = await cg.gpio_pin_expression(config['sw_down_pin'])
    cg.add(var.set_sw_down_pin(sw_down_pin))
    amp_sw_pin = await cg.gpio_pin_expression(config['amp_sw_pin'])
    cg.add(var.set_amp_sw_pin(amp_sw_pin))
    cg.add(var.set_audible_call(config['audible_call']))
    cg.add(var.set_call_accept_delay(config['call_accept_delay']));
    cg.add(var.set_audio1_before_play_delay(config['audio1_before_play_delay']));
    cg.add(var.set_audio1_after_play_delay(config['audio1_after_play_delay']));
    cg.add(var.set_audio2_before_play_delay(config['audio2_before_play_delay']));
    cg.add(var.set_before_hangup_delay(config['before_hangup_delay']))
    target_sens = await cg.get_variable(config['call_processing_sensor_id'])
    cg.add(var.set_call_processing_sensor(target_sens))
    await cg.register_component(var, config)