# Manual Control Mode

Manual control is part of the product surface for phone and future BLE remote
controllers.

The firmware accepts only the atomic `MANUAL_SET:<steer>,<throttlePct>,<ttlMs>`
lease command plus `MANUAL_OFF`. Each accepted command belongs to one controller
source and refreshes that source's deadman TTL. A competing source cannot replace
a live lease until the lease expires or `MANUAL_OFF`/`STOP` clears it.

If manual updates stop, the TTL expires, Manual mode exits, and the normal
quiet-output path stops stepper and thruster output.
