# TODO

import json
import paho.mqtt.client as mqtt
import psycopg2

MQTT_HOST = "broker"
MQTT_PORT = 1883
MQTT_TOPIC = "lab/+/+/+"

DB_HOST = "database"
DB_NAME = "postgres"
DB_USER = "postgres"
DB_PASSWORD = "postgres"

"""
UWAGA!
Sprawdz dane do logowania z plikiem database/Dockerfile i najlepiej przenies je do
osobnego pliku ktorego nie bedziesz wgrywac do repozytorium!
"""

def save_measurement(topic, data):
    conn = psycopg2.connect(
        host=DB_HOST,
        dbname=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD
    )
    cur = conn.cursor()
    cur.execute("""
        INSERT INTO measurements 
        (group_id, device_id, sensor, value, unit, ts_ms, seq, topic)
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
    """, (
        data.get("group_id"),
        data["device_id"],
        data["sensor"],
        data["value"],
        data.get("unit"),
        data["ts_ms"],
        data.get("seq"),
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

client.connect(MQTT_HOST, MQTT_PORT, 60)
client.loop_forever()