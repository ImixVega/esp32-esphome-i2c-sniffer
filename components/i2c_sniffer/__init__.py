# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 ImixVega and contributors
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]

CONF_SCL_PIN = "scl_pin"
CONF_SDA_PIN = "sda_pin"
CONF_RAW_EDGES = "raw_edges"
CONF_QUIET_TIME_MS = "quiet_time_ms"

ns = cg.esphome_ns.namespace("i2c_sniffer")
I2CSniffer = ns.class_("I2CSniffer", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(I2CSniffer),
        cv.Required(CONF_SCL_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_SDA_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RAW_EDGES, default=True): cv.boolean,
        cv.Optional(CONF_QUIET_TIME_MS, default=200): cv.int_range(min=50, max=2000),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    scl = await cg.gpio_pin_expression(config[CONF_SCL_PIN])
    sda = await cg.gpio_pin_expression(config[CONF_SDA_PIN])
    cg.add(var.set_scl_pin(scl))
    cg.add(var.set_sda_pin(sda))
    cg.add(var.set_raw_edges(config[CONF_RAW_EDGES]))
    cg.add(var.set_quiet_time_ms(config[CONF_QUIET_TIME_MS]))