"""EE470 Node_3: HiveMQ -> Raspberry Pi -> existing Hostinger insert.php.
Run only one instance. Ctrl+C stops forwarding; unsent queued data is discarded.
"""
import argparse
import json
import queue
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from datetime import datetime
from zoneinfo import ZoneInfo
import paho.mqtt.client as mqtt

BASE = "IoT/potent_35317d28"
BROKER = "broker.hivemq.com"
PORT = 8883
TLS = True
ENDPOINT = "https://taineengineering.com/insert.php"
STORE_ZONE_NAME = "America/New_York"
STORE_ZONE = ZoneInfo(STORE_ZONE_NAME)


def run(listen_only=False, stop=None):
    stop = stop if stop is not None else threading.Event()
    pending = queue.Queue(maxsize=100)
    last_ms = 0
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                        client_id="pi_node3_" + uuid.uuid4().hex[:10],
                        clean_session=True, protocol=mqtt.MQTTv311)
    if TLS:
        client.tls_set()  # Use trusted certificates and verify the broker hostname.
    client.reconnect_delay_set(1, 30)

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            client.subscribe(BASE + "/telemetry", qos=0)
            print("Connected; listening on", BASE + "/telemetry", flush=True)
        else:
            print("Connection rejected:", reason_code, flush=True)

    def on_message(client, userdata, message):
        nonlocal last_ms
        if stop.is_set() or message.retain or len(message.payload) > 128:
            return
        try:
            data = json.loads(message.payload)
            if not isinstance(data, dict) or set(data) != {"pot", "led", "switch"}:
                raise ValueError("Use pot, led, switch fields.")
            for key, maximum in [("pot", 1023), ("led", 1), ("switch", 1)]:
                if type(data[key]) is not int or not 0 <= data[key] <= maximum:
                    raise ValueError("Invalid sensor/state value.")
        except (ValueError, UnicodeDecodeError):
            print("Ignored malformed telemetry.", flush=True)
            return
        # Unique receipt times, converted to the named zone accepted by the database.
        last_ms = max(time.time_ns() // 1_000_000, last_ms + 1)
        stamp = datetime.fromtimestamp(last_ms / 1000, STORE_ZONE).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        record = {"node_number": "Node_3", "time": stamp, "timezone": STORE_ZONE_NAME,
                  "sensor3": str(data["pot"]), "led_state": str(data["led"]),
                  "switch_state": str(data["switch"])}
        print(stamp, STORE_ZONE_NAME, data, flush=True)
        if not listen_only:
            try:
                pending.put_nowait(record)
            except queue.Full:
                print("Queue full: reading dropped. Check Hostinger connectivity.", flush=True)

    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = lambda c,u,f,r,p: print("MQTT disconnected:", r, flush=True)
    client.connect_async(BROKER, PORT, 60)
    client.loop_start()
    print("LISTEN ONLY" if listen_only else "DATABASE FORWARDING ENABLED", flush=True)
    print("Keep this terminal running. Press Ctrl+C to stop.", flush=True)
    try:
        while not stop.is_set():
            try:
                record = pending.get(timeout=0.25)
            except queue.Empty:
                continue
            # The same record/time is reused on retry: the PHP duplicate check owns it.
            delay = 1
            while not stop.is_set():
                request = urllib.request.Request(ENDPOINT,
                    data=urllib.parse.urlencode(record).encode(), method="POST",
                    headers={"Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json"})
                try:
                    with urllib.request.urlopen(request, timeout=8) as response:
                        result = json.loads(response.read(8192))
                    if result.get("status") not in ("stored", "duplicate") or result.get("record") != record:
                        raise ValueError("Server did not confirm this exact record.")
                    print("Hostinger:", result["status"], record["time"], flush=True)
                    break
                except urllib.error.HTTPError as error:
                    if 400 <= error.code < 500 and error.code not in (408, 429):
                        print("Server rejected this reading: HTTP", error.code,
                              "Check SQL, PHP and Node_3 registration.", flush=True)
                        stop.set()
                    else:
                        print("Temporary HTTP error:", error.code, flush=True)
                        stop.wait(delay)
                        delay = min(delay * 2, 15)
                except (OSError, ValueError):
                    print("Storage not confirmed; retrying the same reading.", flush=True)
                    stop.wait(delay)
                    delay = min(delay * 2, 15)
            pending.task_done()
    except KeyboardInterrupt:
        print("Stop requested.", flush=True)
    finally:
        stop.set(); client.disconnect(); client.loop_stop()
        print("Relay stopped. Unsent queued readings were discarded.", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--listen-only", action="store_true")
    run(listen_only=parser.parse_args().listen_only)
