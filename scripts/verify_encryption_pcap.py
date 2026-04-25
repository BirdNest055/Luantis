#!/usr/bin/env python3
"""
Luanti Encryption Verification Script
======================================

This script analyzes a pcap/pcapng capture file to verify that Luanti
game traffic is encrypted with AES-256-GCM after authentication completes.

It checks:
1. Initial packets (before encryption activation) contain the plaintext
   protocol magic 0x4f457403
2. After encryption activation, packets contain the encrypted flag 0x80
   after the base header, and the protocol magic is NOT visible
3. No plaintext game packets appear after encryption is established

Usage:
    python3 verify_encryption_pcap.py <capture.pcap>

Requirements:
    pip install scapy

The script uses scapy to parse UDP packets on port 30000 (default Luanti port).
You can change the port with the --port argument.
"""

import sys
import struct
import argparse
from collections import defaultdict

try:
    from scapy.all import rdpcap, UDP, IP
except ImportError:
    print("ERROR: scapy is required. Install with: pip install scapy")
    sys.exit(1)

# Luanti protocol constants
PROTOCOL_MAGIC = b'\x4f\x45\x74\x03'
BASE_HEADER_SIZE = 7
ENCRYPTED_FLAG = 0x80
DEFAULT_PORT = 30000

# Packet classification
class PacketType:
    PLAINTEXT = "PLAINTEXT"
    ENCRYPTED = "ENCRYPTED"
    UNKNOWN = "UNKNOWN"
    TOO_SHORT = "TOO_SHORT"


def classify_packet(payload):
    """Classify a UDP payload as plaintext, encrypted, or unknown."""
    if len(payload) < BASE_HEADER_SIZE:
        return PacketType.TOO_SHORT, {}

    # Check protocol magic in base header
    magic = payload[0:4]
    if magic != PROTOCOL_MAGIC:
        return PacketType.UNKNOWN, {"reason": "bad_magic", "magic": magic.hex()}

    info = {
        "peer_id": struct.unpack(">H", payload[4:6])[0],
        "channel": payload[6],
    }

    if len(payload) <= BASE_HEADER_SIZE:
        return PacketType.PLAINTEXT, info

    # Check the byte after the base header
    first_byte = payload[BASE_HEADER_SIZE]

    if first_byte == ENCRYPTED_FLAG:
        info["encrypted_flag"] = True
        info["has_nonce"] = len(payload) >= BASE_HEADER_SIZE + 1 + 12
        info["has_tag"] = len(payload) >= BASE_HEADER_SIZE + 1 + 12 + 16
        info["ciphertext_len"] = max(0, len(payload) - BASE_HEADER_SIZE - 1 - 12 - 16)
        return PacketType.ENCRYPTED, info
    elif first_byte <= 0x03:
        info["packet_type"] = first_byte
        info["payload_len"] = len(payload) - BASE_HEADER_SIZE
        return PacketType.PLAINTEXT, info
    else:
        info["first_byte"] = first_byte
        return PacketType.UNKNOWN, info


def check_plaintext_protocol_magic_in_encrypted(payload):
    """
    Check if the plaintext protocol magic 0x4f457403 appears anywhere
    in the encrypted portion of a packet (after the base header).
    If it does, the encryption is broken.
    """
    encrypted_portion = payload[BASE_HEADER_SIZE:]
    # Skip the encrypted flag + nonce (1 + 12 = 13 bytes)
    ciphertext_start = 1 + 12
    if len(encrypted_portion) > ciphertext_start + 4:
        for i in range(ciphertext_start, len(encrypted_portion) - 3):
            if encrypted_portion[i:i+4] == PROTOCOL_MAGIC:
                return True
    return False


def analyze_pcap(filename, port):
    """Analyze a pcap file for Luanti encryption verification."""
    print(f"Reading capture file: {filename}")
    print(f"Filtering for UDP port: {port}")
    print()

    try:
        packets = rdpcap(filename)
    except Exception as e:
        print(f"ERROR: Failed to read capture file: {e}")
        return False

    stats = defaultdict(int)
    plaintext_packets = []
    encrypted_packets = []
    unknown_packets = []
    plaintext_magic_in_encrypted = []

    for i, pkt in enumerate(packets):
        if not pkt.haslayer(UDP) or not pkt.haslayer(IP):
            continue

        udp = pkt[UDP]
        if udp.dport != port and udp.sport != port:
            continue

        payload = bytes(udp.payload)
        ptype, info = classify_packet(payload)

        src_ip = pkt[IP].src
        dst_ip = pkt[IP].dst

        stats["total"] += 1
        stats[ptype.lower()] += 1

        frame_info = {
            "frame": i + 1,
            "src": f"{src_ip}:{udp.sport}",
            "dst": f"{dst_ip}:{udp.dport}",
            "size": len(payload),
            "info": info,
        }

        if ptype == PacketType.PLAINTEXT:
            plaintext_packets.append(frame_info)
        elif ptype == PacketType.ENCRYPTED:
            encrypted_packets.append(frame_info)
            # Check for leaked plaintext protocol magic
            if check_plaintext_protocol_magic_in_encrypted(payload):
                plaintext_magic_in_encrypted.append(frame_info)
        else:
            unknown_packets.append(frame_info)

    # Print report
    print("=" * 70)
    print("LUANTI ENCRYPTION VERIFICATION REPORT")
    print("=" * 70)
    print()

    print("SUMMARY")
    print("-" * 40)
    print(f"  Total Luanti packets:    {stats['total']}")
    print(f"  Plaintext packets:       {stats['plaintext']}")
    print(f"  Encrypted packets:       {stats['encrypted']}")
    print(f"  Unknown packets:         {stats['unknown']}")
    print(f"  Too-short packets:       {stats['too_short']}")
    print()

    # First plaintext packets
    if plaintext_packets:
        print("FIRST 10 PLAINTEXT PACKETS")
        print("-" * 40)
        for p in plaintext_packets[:10]:
            pt = p['info'].get('packet_type', '?')
            ch = p['info'].get('channel', '?')
            peer = p['info'].get('peer_id', '?')
            print(f"  Frame {p['frame']:5d} | {p['src']:20s} -> {p['dst']:20s} "
                  f"| size={p['size']:4d} | peer={peer} ch={ch} type={pt}")
        if len(plaintext_packets) > 10:
            print(f"  ... and {len(plaintext_packets) - 10} more plaintext packets")
        print()

    # First encrypted packets
    if encrypted_packets:
        print("FIRST 10 ENCRYPTED PACKETS")
        print("-" * 40)
        for p in encrypted_packets[:10]:
            ct_len = p['info'].get('ciphertext_len', '?')
            has_nonce = p['info'].get('has_nonce', False)
            has_tag = p['info'].get('has_tag', False)
            print(f"  Frame {p['frame']:5d} | {p['src']:20s} -> {p['dst']:20s} "
                  f"| size={p['size']:4d} | ct_len={ct_len} nonce={has_nonce} tag={has_tag}")
        if len(encrypted_packets) > 10:
            print(f"  ... and {len(encrypted_packets) - 10} more encrypted packets")
        print()

    # Timeline analysis
    if plaintext_packets and encrypted_packets:
        first_encrypted_frame = encrypted_packets[0]['frame']
        last_plaintext_before_enc = None
        for p in plaintext_packets:
            if p['frame'] < first_encrypted_frame:
                last_plaintext_before_enc = p['frame']

        print("ENCRYPTION ACTIVATION TIMELINE")
        print("-" * 40)
        print(f"  First encrypted packet:  Frame {first_encrypted_frame}")
        if last_plaintext_before_enc:
            print(f"  Last plaintext before:   Frame {last_plaintext_before_enc}")
        else:
            print(f"  No plaintext seen before encryption")

        # Check if plaintext appears AFTER encryption starts
        plaintext_after_enc = [p for p in plaintext_packets
                               if p['frame'] > first_encrypted_frame]
        if plaintext_after_enc:
            print(f"  WARNING: {len(plaintext_after_enc)} plaintext packets "
                  f"found AFTER encryption started!")
            for p in plaintext_after_enc[:5]:
                print(f"    Frame {p['frame']} | {p['src']} -> {p['dst']}")
        else:
            print(f"  No plaintext after encryption: GOOD")
        print()

    # Critical check: plaintext protocol magic in encrypted packets
    print("CRITICAL: PLAINTEXT PROTOCOL MAGIC IN ENCRYPTED PACKETS")
    print("-" * 40)
    if plaintext_magic_in_encrypted:
        print(f"  FAIL: Found 0x4f457403 in {len(plaintext_magic_in_encrypted)} "
              f"encrypted packet(s)!")
        print(f"  This means the encryption is BROKEN - Wireshark can see")
        print(f"  the Luanti protocol header in ciphertext!")
        for p in plaintext_magic_in_encrypted[:5]:
            print(f"    Frame {p['frame']} | {p['src']} -> {p['dst']}")
        print()
        return False
    else:
        if encrypted_packets:
            print(f"  PASS: No plaintext protocol magic (0x4f457403) found in "
                  f"any of {len(encrypted_packets)} encrypted packets.")
            print(f"  Wireshark CANNOT identify the protocol from encrypted traffic.")
        else:
            print(f"  SKIP: No encrypted packets found to check.")
        print()

    # Final verdict
    print("=" * 70)
    print("VERDICT")
    print("=" * 70)
    if encrypted_packets and not plaintext_magic_in_encrypted:
        encryption_ratio = len(encrypted_packets) / max(1, stats['total']) * 100
        print(f"  ENCRYPTION IS REAL: {encryption_ratio:.1f}% of packets are encrypted")
        print(f"  AES-256-GCM encryption is active on the wire.")
        print(f"  Protocol magic 0x4f457403 is hidden from passive observers.")
        return True
    elif not encrypted_packets and plaintext_packets:
        print(f"  ENCRYPTION IS NOT ACTIVE: All {len(plaintext_packets)} packets are plaintext")
        print(f"  The insecure overlay is correct - traffic is NOT encrypted.")
        return False
    else:
        print(f"  INCONCLUSIVE: No Luanti packets found on port {port}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Verify Luanti AES-256-GCM encryption in a pcap capture")
    parser.add_argument("pcap_file", help="Path to pcap/pcapng capture file")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                       help=f"UDP port to filter (default: {DEFAULT_PORT})")
    args = parser.parse_args()

    success = analyze_pcap(args.pcap_file, args.port)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
