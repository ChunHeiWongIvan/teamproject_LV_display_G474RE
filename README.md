# Custom UART Message Protocol

This project uses a custom UART packet format to send data between the HV MCU to the LV display MCU.

The protocol is designed to be:

- simple to parse
- compact
- easy to extend with new message IDs
- robust against partial UART reads by using a start byte and checksum

---

## Packet Structure

Each UART message has the following byte layout:

| Field | Size | Description |
|---|---:|---|
| `SOF` | 1 byte | Start of frame marker, always `0xA5` |
| `ID` | 2 bytes | Message ID, little-endian |
| `LEN` | 1 byte | Number of payload bytes |
| `DATA` | 0 to 8 bytes | Payload data |
| `CRC` | 1 byte | XOR checksum of all previous bytes in the frame |

### Byte layout

```text
+------+------+------+------+---------+------+
| SOF  | ID_L | ID_H | LEN  | DATA... | CRC  |
+------+------+------+------+---------+------+
   1B     1B     1B     1B    0..8B     1B