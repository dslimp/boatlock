# Manual Control Mode

Manual control is part of the product surface for phone and future BLE remote
controllers.

## Current Command Contract

The firmware accepts only the atomic
`MANUAL_TARGET:<angleDeg>,<throttlePct>,<ttlMs>` lease command plus
`MANUAL_OFF`. `angleDeg` is the selected steering vector from `-90` to `+90`
degrees relative to bow-forward, and `throttlePct` is the brushed-motor PWM
request. Each accepted `MANUAL_TARGET` belongs to one controller source,
disables Anchor mode on entry, and refreshes that source's manual deadman TTL.
A competing source cannot replace a live manual lease until the lease expires
or `MANUAL_OFF`/`STOP` clears it.
The shipped phone UI uses `ttlMs=1000` and refreshes the selected angle/speed
while either value is non-zero.

If manual updates stop, the TTL expires, Manual mode exits, and the normal
quiet-output path stops stepper and thruster output.

## Multi-Client Ownership Design

This is the required design gate before accepting a future BLE remote as a
second real controller. Multi-central BLE transport is allowed for discovery and
telemetry, but transport support does not grant control authority.

Controller identity is assigned by firmware, not self-declared in command text.
Each connected central needs a connection-local identity made from the BLE
connection handle, sanitized peer address when available, and a monotonic
connection generation. A reconnect creates a new controller identity and must
acquire control again. If the BLE stack cannot provide a stable peer address,
the handle plus generation is still sufficient for one boot session; identities
are not persisted across boots.

Read-only telemetry clients are allowed to coexist. They may subscribe to `34cd`
live telemetry and `78ab` logs and may use transport-only `STREAM_START`,
`STREAM_STOP`, and `SNAPSHOT`. They do not own a control lease and must not send
actuation, mode, settings, OTA, simulation, or injected-sensor commands.

At most one eligible control owner may exist at a time. The control lease stores
controller identity, role (`app` or `remote`), acquire time, last refresh, and
expiry. The first eligible control command can acquire the lease only when no
live owner exists. Accepted owner
commands and owner `HEARTBEAT` refresh the lease. Commands from a different
controller while the lease is live are rejected with a stable busy reason and
have no side effects.

Takeover is conservative. A different controller can take over only after the
current owner lease expires, the owner disconnects and the runtime has gone
quiet, or emergency `STOP` clears the owner. There is no silent transfer to a
connected telemetry client. Owner disconnect during Manual or Anchor is a
control-link loss: outputs go quiet and the runtime must not resume actuation
because another client is still connected.

The phone app is the full product controller for anchor save/enable/disable,
jog, hold-heading, manual, STOP, and allowed settings and OTA flows. A
future BLE remote is constrained by default to deadman manual control, ordinary
Anchor off, owner heartbeat, and emergency STOP. It must not save or move the
anchor point, change settings, start OTA, run HIL, inject sensors, or use setup
or dev/HIL commands unless a later product decision explicitly adds and tests
those remote surfaces.

Manual control is a sublease of the control owner. The first accepted
`MANUAL_TARGET` may acquire both the control lease and manual deadman lease
atomically when no owner exists. After that, only the control owner can refresh
manual TTL with `MANUAL_TARGET` or stop manual output with `MANUAL_OFF`. Manual TTL
expiry stops Manual output; it does not grant takeover to a competing source.
Control lease expiry, owner disconnect, or `STOP` clears any active manual
lease.

STOP priority is absolute at the safety layer. Physical STOP always wins and
clears control ownership, manual lease state, Anchor, and outputs. BLE `STOP`
does not require ownership, so a secondary controller can stop a live owner.

Before accepting an implementation, tests must cover:

- two connected telemetry clients receiving live frames/logs while neither can
  control without acquiring ownership
- owner acquisition, refresh, expiry, disconnect cleanup, and busy rejection for
  Anchor, Manual, settings, OTA, and dev/HIL commands
- remote role allowlist: manual, Anchor off, heartbeat, and STOP accepted;
  anchor save/jog/settings/OTA/HIL/sensor injection rejected
- manual sublease behavior: same-owner refresh, non-owner refresh rejection,
  TTL quieting, `MANUAL_OFF`, lease expiry, and reconnect
- STOP preemption from hardware and from a non-owner BLE client
- Android or bench two-central smoke where phone telemetry stays read-only while
  a remote/harness owns manual, and the reverse busy path is observable
