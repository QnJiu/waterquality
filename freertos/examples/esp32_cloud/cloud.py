"""Build a small Alibaba IoT property-post message from Modbus status."""
import json
import math


def property_message(status, message_id):
    properties = {
        "State": status["state"],
        "Fault": status["fault"],
        "Step": status["step"],
        "StartResult": status["start_result"],
        "ResultValid": int(status["result_valid"]),
    }
    if status["result_valid"]:
        value = status["concentration_mg_l"]
        if not math.isfinite(value):
            raise ValueError("invalid concentration")
        properties["Concentration"] = value
    return json.dumps({"id": str(message_id), "version": "1.0",
                       "params": properties,
                       "method": "thing.event.property.post"})


def publish_status(client, topic, status, message_id):
    payload = property_message(status, message_id)
    client.publish(topic.encode(), payload.encode(), qos=0)


def publish_version(client, topic, version, message_id):
    if not isinstance(version, int) or not 1 <= version <= 0xffffffff:
        raise ValueError("invalid confirmed version")
    payload = json.dumps({"id": str(message_id), "params": {"version": str(version)}})
    client.publish(topic.encode(), payload.encode(), qos=0)


def publish_progress(client, topic, step, description):
    payload = json.dumps({"id": "1", "params": {"step": str(step), "desc": description}})
    client.publish(topic.encode(), payload.encode(), qos=0)
