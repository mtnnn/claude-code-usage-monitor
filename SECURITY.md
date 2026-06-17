# Security policy

## Threat model

Claude Code Usage Monitor is designed to be used on a **trusted local network**
(typically a home or office network). The bridge runs on your computer, and the ESP32
on the same LAN polls `/usage` every few seconds.

What we defend against:

- **Unauthorized reading of usage data by other clients on the LAN.** As of
  v0.2, every request to `/usage` and `/metrics` must present a bearer token
  generated on first run and saved in `~/.claude-code-usage/token` (permissions
  `0600`, directory `0700`).
- **Opportunistic scans** of port 8787 on the LAN: without a token we respond `401`.
- **Unauthorized re-pointing of the device.** The firmware's control endpoint
  (`POST /config` on port 80, normal mode) changes where the device
  sends its bearer token, so it requires knowledge of the current token
  (`Authorization: Bearer` header or form field, near-constant-time
  comparison). A LAN host that does not know the token receives a `401` and cannot
  redirect the device toward a hostile bridge.
- **Opportunistic access to the setup AP (captive portal).** The AP
  `ClaudeMonitor-XXYY` is WPA2-protected with a random password generated on each
  portal startup and shown only on the device's display — physical
  proximity is required to read it. The bridge bearer token is not repopulated in the
  web form (we do not reflect a secret), and a portal entered due to a WiFi failure
  automatically restarts after a timeout instead of remaining an indefinitely
  running AP.

What we do NOT defend against (out of scope in v0.2):

- **Adversaries with shell access on the PC where the bridge runs.** They can read the
  token from the filesystem or read the JSONL transcripts directly.
- **Passive interception on the LAN** (Wi-Fi without WPA2/3, sniffing on
  unmanaged switches): the traffic is plain HTTP. TLS is planned for v0.3.
- **Exposure to the Internet.** Do not expose the bridge to the Internet without a
  TLS-terminating reverse proxy in front — the bearer token does not replace HTTPS.
  Workaround: put nginx/Caddy in front of `127.0.0.1:8787` with a Let's Encrypt cert
  or an SSH/Tailscale tunnel.
- **Prompt content leakage.** The bridge exposes only aggregated data (cost, token
  counts, model breakdown). The full prompts and responses stay in the
  JSONL files and are never transmitted.

## Supported versions

| Version | Supported |
|----------|------------|
| 0.2.x    | yes        |
| 0.1.x    | no — upgrade to 0.2.x |

## Reporting a vulnerability

Email: **rootedlab@proton.me** with subject `[cc-monitor security]`.

- Acknowledgement within 7 days.
- Fix target: 30 days for medium-high severity.
- Please do not open a public issue before coordinated disclosure.

We appreciate responsible disclosure and thank anyone who helps us keep
the project secure for the community.
