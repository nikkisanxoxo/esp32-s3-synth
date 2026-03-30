#!/usr/bin/env python3
"""
Callback timing benchmark for ESP32-S3.

Sends MIDI note_ons one by one, waits for the CCOUNT timing report after each,
and prints a summary table.

Usage:
    python3 bench_polyphony.py [--port /dev/ttyUSB0] [--max-voices 16] [--midi-port auto]
"""

import argparse
import re
import time

import mido
import serial

BUDGET_CYCLES  = 320000  # buffer_length / sample_rate * 240 MHz
BASE_NOTE      = 48      # C3 — voices spread chromatically by 2 semitones
STEP_SEMITONES = 2

def find_midi_port(name_hint):
    ports = mido.get_output_names()
    seen = []
    for p in ports:
        if p not in seen:
            seen.append(p)
    if name_hint:
        match = next((p for p in seen if name_hint.lower() in p.lower()), None)
        if match:
            return match
    return next((p for p in seen if 'Through' not in p), None)

def run(serial_port, baud, max_voices, midi_hint):
    bp_port = find_midi_port(midi_hint)
    if bp_port is None:
        print("No suitable MIDI output port found.")
        print("Available:", mido.get_output_names())
        return

    print(f"MIDI port : {bp_port}")
    print(f"Serial    : {serial_port}  {baud} baud")
    print(f"Budget    : {BUDGET_CYCLES} cycles\n")

    ser = serial.Serial(serial_port, baud, timeout=0.1)
    time.sleep(0.5)
    ser.reset_input_buffer()

    results = []

    with mido.open_output(bp_port) as midi_out:
        for n in range(0, max_voices + 1):
            if n > 0:
                note = BASE_NOTE + (n - 1) * STEP_SEMITONES
                midi_out.send(mido.Message('note_on', note=note, velocity=100))

            deadline = time.time() + 5.0
            buf = ''
            found = False
            while time.time() < deadline:
                buf += ser.read(256).decode('ascii', errors='ignore')
                m = re.search(
                    r'cb: min=(\d+) avg=(\d+) max=(\d+) cycles \(budget=(\d+), n=(\d+)\)',
                    buf)
                if m:
                    mn, avg, mx, budget, cnt = [int(x) for x in m.groups()]
                    pct = mx / budget * 100
                    status = 'OVER' if mx > budget else 'ok'
                    print(f"voices={n:2d}  min={mn:7d}  avg={avg:7d}  max={mx:7d} "
                          f" {pct:5.1f}%  {status if n > 0 else 'idle'}")
                    results.append((n, mn, avg, mx, budget))
                    ser.reset_input_buffer()
                    found = True
                    break

            if not found:
                print(f"voices={n:2d}  (no response — stopping)")
                break

        for n in range(1, max_voices + 1):
            midi_out.send(mido.Message('note_off',
                                       note=BASE_NOTE + (n - 1) * STEP_SEMITONES,
                                       velocity=0))

    ser.close()

    if not results:
        return

    print(f"\n{'voices':>6}  {'min':>8}  {'avg':>8}  {'max':>8}  {'%budget':>8}")
    print("-" * 50)
    for voices, mn, avg, mx, budget in results:
        print(f"{voices:>6}  {mn:>8}  {avg:>8}  {mx:>8}  {mx/budget*100:>7.1f}%")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ESP32-S3 polyphony timing benchmark')
    parser.add_argument('--port',       default='/dev/ttyUSB0', help='Serial port (ESP32 UART0)')
    parser.add_argument('--baud',       default=115200, type=int)
    parser.add_argument('--max-voices', default=16, type=int)
    parser.add_argument('--midi-port',  default='ESP32', help='Substring to match MIDI port name')
    args = parser.parse_args()

    run(args.port, args.baud, args.max_voices, args.midi_port)
