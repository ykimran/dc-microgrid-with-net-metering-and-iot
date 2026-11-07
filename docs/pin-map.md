# Pin Map

Source of truth is `firmware/microgrid_controller/microgrid_controller.ino`.

## Digital pins

| Pin | Function | Notes |
|---|---|---|
| D2 | Relay-1 | Charge vs export. NC = charging, NO = send back to grid |
| D3 | Relay-2 | Grid vs battery. NC = grid, NO = battery |
| D4 | Relay-3 | Day/night profile. NC = day, NO = night |
| D5 | Relay-4 | Non-essential loads |
| D6 | Grid sense | Input: 5 V = grid online, 0 V = offline. Add a ~100 kΩ pulldown to GND if it can float |
| D7 | Buzzer | Active buzzer or simple piezo |

Relay module is 4-channel, **active LOW** (LOW energises the coil). All relays are driven HIGH (off) at boot.

## Analog pins

| Pin | Sensor | Use |
|---|---|---|
| A0 | ACS-1 | Net metering (grid current) |
| A1 | VS | Battery pack voltage |
| A2 | ACS-2 | Load current |
| A3 | LDR | Day/night detection |

## I²C (A4 = SDA, A5 = SCL)

| Address | Display | Content |
|---|---|---|
| 0x27 | Display-1 | Grid status, battery level (%), battery voltage |
| 0x26 | Display-2 | Load power (ACS-2 current × 12 V), load current, DAY/NIGHT |
| 0x25 | Display-3 | Net-metering power (ACS-1 current × 12 V) and current |

## Serial

`Serial` at 115200 baud, JSON lines to the NodeMCU ESP8266 (a logic level shifter is in the parts list).

## Traffic-light demo (separate board)

| Set | Green | Yellow | Red |
|---|---|---|---|
| 1 | D2 | D3 | D4 |
| 2 | D5 | D6 | D7 |
| 3 | D8 | D9 | D10 |
| 4 | D11 | D12 | D13 |

Timing: 5 s green, 2 s yellow, then red while the next set cycles.
