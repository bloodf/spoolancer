# Security

Spoolancer is prototype firmware and hardware. Automated tests and builds do not establish security of an assembled device. There is no independent security audit, guaranteed response time or maintained older-release support schedule. Security fixes are developed against the current main branch; reports should identify the exact version or commit.

## Reporting a vulnerability

Use the repository’s **Security → Advisories → Report a vulnerability** option when GitHub private vulnerability reporting is enabled. If that option is unavailable, contact the repository maintainer through a private channel listed on their GitHub profile. Do not send secrets or exploit details in a public issue.

If no private channel is available, open an issue asking the maintainer to enable private reporting, without including reproduction details or sensitive data. Include affected versions, impact, a minimal reproduction and sanitized evidence only after a private channel is established. Never include real Wi-Fi credentials, pairing PINs, session tokens or unredacted flash dumps.

## Current boundaries

- The screenless and integrated-touch setup access point is temporary and open. PIN enrollment and Wi-Fi configuration use local HTTP; nearby observers may see that traffic. Physical BOOT arming and time limits do not encrypt it.
- The local device interface and Spoolman connection use HTTP. The current Spoolman client accepts a numeric IPv4 address and does not support HTTPS or server authentication. Use a trusted local network; do not expose the device’s HTTP service directly to the internet.
- Flash storage is not encrypted. Someone with physical access may read stored data or replace firmware. The eight-second BOOT action resets Wi-Fi and pairing while keeping inventory, calibration and Spoolman configuration; it is not a secure erase operation.
- The public status endpoint intentionally exposes device and connection status. Browser sessions use bearer tokens held in tab memory. These mechanisms do not protect against interception on an untrusted network.
- Writable NTAG216 inventory records contain spool metadata without encryption or a cryptographic signature. NFC identity and write checks are not protection against deliberate tag impersonation. Original printer tags remain read-only in this implementation.

For the complete operating limits and remaining checks, see the [firmware guide](firmware/README.md) and [status report](docs/STATUS.md).
