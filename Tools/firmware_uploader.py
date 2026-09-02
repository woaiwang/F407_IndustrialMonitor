#!/usr/bin/env python3
"""Upload a raw STM32 application image through the Stage 7.2 UART IAP."""

import argparse
import binascii
import pathlib
import struct
import sys
import time

try:
    import serial
except ModuleNotFoundError:
    serial = None


APP_ADDRESS = 0x08010000
APP_END = 0x08100000
APP_MAX_SIZE = APP_END - APP_ADDRESS

HEADER = b"\xA5\x5A"
MAX_WRITE_DATA_SIZE = 256
MAX_PAYLOAD_SIZE = 4 + MAX_WRITE_DATA_SIZE
MAX_ATTEMPTS = 3
HANDSHAKE_ACK_WINDOW_SECONDS = 0.04
HELLO_ACK_TIMEOUT_SECONDS = 0.2
SERIAL_SETTLE_SECONDS = 0.15

CMD_HELLO = 0x01
CMD_BEGIN_UPDATE = 0x02
CMD_ERASE_APP = 0x03
CMD_WRITE_DATA = 0x04
CMD_VERIFY = 0x05
CMD_RUN_APP = 0x06

CMD_ACK = 0x79
CMD_NACK = 0x1F


class UploadError(RuntimeError):
    """Raised when the target does not complete an IAP protocol operation."""


class FrameError(UploadError):
    """Raised when a received UART response is not a valid IAP frame."""

    def __init__(self, message, raw_bytes=b""):
        super().__init__(message)
        self.raw_bytes = bytes(raw_bytes)


class ResponseTimeout(TimeoutError):
    """Raised when a response frame is incomplete or absent."""

    def __init__(self, raw_bytes):
        super().__init__("response timeout")
        self.raw_bytes = bytes(raw_bytes)


def calculate_crc32(data):
    """Return standard CRC-32 (poly EDB88320, init/xorout FFFFFFFF)."""
    return binascii.crc32(data) & 0xFFFFFFFF


def build_frame(command, sequence, payload=b""):
    """Build A5 5A CMD SEQ LEN_LE PAYLOAD CRC32_LE."""
    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ValueError("payload is larger than the protocol limit")

    body = struct.pack("<BBH", command, sequence, len(payload)) + payload
    return HEADER + body + struct.pack("<I", calculate_crc32(body))


def load_firmware(path):
    """Load and validate a V1 raw application image."""
    if path.suffix.lower() != ".bin":
        raise UploadError("V1 supports raw .bin firmware only")

    firmware = path.read_bytes()
    if not firmware:
        raise UploadError("firmware file is empty")
    if len(firmware) > APP_MAX_SIZE:
        raise UploadError(
            f"firmware is {len(firmware)} bytes; APP limit is {APP_MAX_SIZE} bytes"
        )

    return firmware


def open_serial_port(arguments):
    """Open the UART without letting CH340 control lines request ISP mode."""
    port = serial.Serial()
    port.port = arguments.port
    port.baudrate = arguments.baud
    port.bytesize = serial.EIGHTBITS
    port.parity = serial.PARITY_NONE
    port.stopbits = serial.STOPBITS_ONE
    port.timeout = 0.1
    port.write_timeout = 1.0
    port.rtscts = False
    port.dsrdtr = False
    port.rts = False
    port.dtr = False
    port.open()
    port.rts = False
    port.dtr = False
    return port


class IapTransport:
    def __init__(self, port, verbose=False):
        self.port = port
        self.sequence = 0
        self.verbose = verbose

    def _trace(self, direction, data):
        if self.verbose:
            hex_data = bytes(data).hex(" ").upper() if data else "<none>"
            print(f"{direction}: {hex_data}")

    def wait_for_handshake(self, reset_timeout):
        """Validate the raw handshake with a framed HELLO ACK."""
        deadline = time.monotonic() + reset_timeout

        while time.monotonic() < deadline:
            self.port.reset_input_buffer()
            self._trace("TX", b"\x7F")
            self.port.write(b"\x7F")
            self.port.flush()

            raw_response = self._read_handshake_window(deadline)
            self._trace("RX", raw_response)
            if raw_response != b"\x79":
                continue

            try:
                self.request(CMD_HELLO, response_timeout=HELLO_ACK_TIMEOUT_SECONDS)
            except UploadError as error:
                if self.verbose:
                    print(f"Handshake candidate rejected: {error}")
                continue

            return

        raise UploadError("handshake timed out; reset the board and try again")

    def _read_handshake_window(self, overall_deadline):
        deadline = min(
            overall_deadline, time.monotonic() + HANDSHAKE_ACK_WINDOW_SECONDS
        )
        raw_response = bytearray()

        while time.monotonic() < deadline:
            self.port.timeout = max(0.001, deadline - time.monotonic())
            chunk = self.port.read(64)
            if chunk:
                raw_response.extend(chunk)

        return bytes(raw_response)

    def request(self, command, payload=b"", response_timeout=1.0):
        """Send one frame and require its matching ACK, retrying at most 3 times."""
        sequence = self.sequence
        frame = build_frame(command, sequence, payload)
        last_error = "no response"

        for attempt in range(1, MAX_ATTEMPTS + 1):
            self.port.reset_input_buffer()
            self._trace("TX", frame)
            self.port.write(frame)
            self.port.flush()

            try:
                (
                    response_command,
                    response_sequence,
                    response_payload,
                    raw_response,
                ) = self._read_frame(response_timeout)
            except (FrameError, ResponseTimeout) as error:
                last_error = str(error)
                self._trace("RX", error.raw_bytes)
                continue

            self._trace("RX", raw_response)

            if response_sequence != sequence:
                last_error = (
                    f"response sequence 0x{response_sequence:02X} does not match "
                    f"0x{sequence:02X}"
                )
                continue

            if response_command == CMD_ACK and not response_payload:
                self.sequence = (self.sequence + 1) & 0xFF
                return

            if response_command == CMD_NACK:
                reason = response_payload[0] if response_payload else 0
                last_error = f"target NACK reason 0x{reason:02X}"
                continue

            last_error = f"unexpected response command 0x{response_command:02X}"

        raise UploadError(
            f"command 0x{command:02X} failed after {MAX_ATTEMPTS} attempts: {last_error}"
        )

    def _read_frame(self, response_timeout):
        deadline = time.monotonic() + response_timeout
        raw_response = bytearray()

        try:
            while True:
                if self._read_exact(1, deadline, raw_response) != HEADER[:1]:
                    continue
                if self._read_exact(1, deadline, raw_response) == HEADER[1:]:
                    break

            metadata = self._read_exact(4, deadline, raw_response)
            command, sequence, payload_length = struct.unpack("<BBH", metadata)
            if payload_length > MAX_PAYLOAD_SIZE:
                raise FrameError(
                    f"response payload length {payload_length} is invalid", raw_response
                )

            payload = self._read_exact(payload_length, deadline, raw_response)
            received_crc32 = struct.unpack(
                "<I", self._read_exact(4, deadline, raw_response)
            )[0]
            calculated_crc32 = calculate_crc32(metadata + payload)
            if received_crc32 != calculated_crc32:
                raise FrameError("response CRC32 mismatch", raw_response)
        except TimeoutError as error:
            raise ResponseTimeout(raw_response) from error

        return command, sequence, payload, bytes(raw_response)

    def _read_exact(self, length, deadline, raw_response):
        data = bytearray()

        while len(data) < length:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("response timeout")

            self.port.timeout = max(0.01, remaining)
            chunk = self.port.read(length - len(data))
            if not chunk:
                continue
            data.extend(chunk)
            raw_response.extend(chunk)

        return bytes(data)


def upload_firmware(
    transport, firmware, erase_timeout, verify_timeout, hello_confirmed=False
):
    firmware_crc32 = calculate_crc32(firmware)
    print(f"Firmware: {len(firmware)} bytes, CRC32: 0x{firmware_crc32:08X}")

    if not hello_confirmed:
        transport.request(CMD_HELLO)
    transport.request(CMD_BEGIN_UPDATE, struct.pack("<II", len(firmware), firmware_crc32))
    transport.request(CMD_ERASE_APP, response_timeout=erase_timeout)

    for offset in range(0, len(firmware), MAX_WRITE_DATA_SIZE):
        chunk = firmware[offset : offset + MAX_WRITE_DATA_SIZE]
        transport.request(CMD_WRITE_DATA, struct.pack("<I", offset) + chunk)
        print(f"\rWriting: {offset + len(chunk):6d}/{len(firmware):6d} bytes", end="", flush=True)

    print()
    transport.request(CMD_VERIFY, response_timeout=verify_timeout)
    transport.request(CMD_RUN_APP)


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="serial port, e.g. COM5")
    parser.add_argument("--file", required=True, type=pathlib.Path, help="application .bin file")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate (default: 115200)")
    parser.add_argument(
        "--reset-timeout",
        type=float,
        default=10.0,
        help="seconds to wait while repeatedly sending the handshake (default: 10)",
    )
    parser.add_argument(
        "--erase-timeout",
        type=float,
        default=30.0,
        help="seconds to wait for APP-sector erasure (default: 30)",
    )
    parser.add_argument(
        "--verify-timeout",
        type=float,
        default=30.0,
        help="seconds to wait for target-side CRC verification (default: 30)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="print UART TX/RX bytes in hexadecimal",
    )
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    if serial is None:
        raise UploadError("pyserial is required: install it with 'python -m pip install pyserial'")

    firmware = load_firmware(arguments.file)

    with open_serial_port(arguments) as port:
        if arguments.verbose:
            print(f"Serial control: DTR={int(port.dtr)} RTS={int(port.rts)}")

        time.sleep(SERIAL_SETTLE_SECONDS)
        transport = IapTransport(port, verbose=arguments.verbose)
        print("Reset the target now; waiting for 0x7F / 0x79 handshake...")
        transport.wait_for_handshake(arguments.reset_timeout)
        print("Handshake complete. Starting update...")
        upload_firmware(
            transport,
            firmware,
            arguments.erase_timeout,
            arguments.verify_timeout,
            hello_confirmed=True,
        )

    print("Update verified. RUN_APP ACK received; application jump requested.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, UploadError) as error:
        print(f"Upload failed: {error}", file=sys.stderr)
        sys.exit(1)
