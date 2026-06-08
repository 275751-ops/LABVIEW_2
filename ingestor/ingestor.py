import json
import paho.mqtt.client as mqtt
import ssl
from config import get_connection

MQTT_HOST = "broker"
MQTT_PORT = 8883
MQTT_TOPIC = "lab/+/+/+"

# DB_HOST = "database"w
# DB_NAME = "postgres"
# DB_USER = "postgres"
# DB_PASSWORD = "postgres"
"""
UWAGA!
Sprawdz dane do logowania z plikiem database/Dockerfile i najlepiej przenies je do
osobnego pliku ktorego nie bedziesz wgrywac do repozytorium!
"""

def save_measurement(topic,data):
    conn = get_connection()
    cur=conn.cursor()
    cur.execute("""
     INSERT INTO measurements
     (group_id,device_id,sensor,value,unit,ts_ms,topic)
     VALUES (%s,%s,%s,%s,%s,%s,%s)
    """,(
     data.get("group_id"),
     data["device_id"],
     data["sensor"],
     data["value"],
     data.get("unit"),
     data["ts_ms"],
     topic
    ))
    conn.commit()
    cur.close()
    conn.close()


def is_valid(data):
     required = ["device_id", "sensor", "value", "ts_ms"]
     return all(field in data for field in required)

def on_connect(client, userdata, flags, rc):
     print("Connected with result code", rc)
     client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8")
        data = json.loads(payload)

        if not is_valid(data):
            print("Invalid payload:", data)
            return

        save_measurement(msg.topic, data)
        print("Saved message from topic:", msg.topic)
    except Exception as e:
        print("Error:", e)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.tls_set(ca_certs="/app/certs/ca.crt")

client.tls_insecure_set(True)

client.connect(MQTT_HOST, MQTT_PORT, 60)
client.loop_forever()
