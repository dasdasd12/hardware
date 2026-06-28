# Profile Configuration Skeleton

This note is a firmware-side skeleton for the keyboard profile model. The
current source of truth is the teammate documentation in:

```text
Docs-For-AI-Keyboard/docs/architecture/keyboard_config_site/
```

The files most relevant to firmware are `profile.html`,
`runtime_pipeline.html`, `runtime_contract.html`, and
`ch585_half_scan_runtime.html`.

## Boundary

`Profile` answers only one question:

```text
When a recognized control produces a press/release or value signal, what
behavior should the keyboard execute?
```

It does not contain:

- ADC sampling details
- MUX wiring or CH585 pin mapping
- magnetic raw calibration blobs
- BLE pairing records
- current USB/BLE connection state
- screen media or lighting effect data
- firmware images

Those belong to `CalibrationData`, `DeviceSettings`, `DeviceState`,
`ScreenConfig`, `LightingConfig`, or hardware facts.

## Current Runtime Split

The profile should not be split into "H417 profile" and "CH585 profile".

```text
CH585
  ADS7948 + MUX scan
  settle / discard / oversample / filtering
  per-key calibration and trigger decision
  output half key bitset

H417
  poll latest half-state from left/right CH585
  merge key bitsets and local controls
  convert 0/1 changes to profile-neutral control events
  execute active Profile / RuntimeTable semantics
  produce report intent

Output adapter
  USB HID when USB is available
  CH585 BLE / 2.4G path when wireless output is active
```

For the current no-USB PCB bring-up, the output adapter is temporarily the left
CH585 BLE HID firmware.

## Minimum Profile Source Shape

This is a small source-form example. It is not the final schema, but it follows
the same boundaries as the teammate docs.

```json
{
  "identity": {
    "profile_id": "debug_right_half_ble_v0",
    "name": "Debug Right Half BLE",
    "revision": 1,
    "schema_version": "profile.source.v1"
  },
  "compatibility": {
    "keyboard_model_id": "ak_h417_ch585_pcb_v1",
    "control_map_hash": "sha256:0000000000000000000000000000000000000000000000000000000000000000",
    "required_control_ids": [
      "key_r_000"
    ]
  },
  "defaults": {
    "controls": {
      "akey": {
        "normal": {
          "press_threshold_pm": 500,
          "release_threshold_pm": 420
        },
        "rapid_trigger": {
          "press_threshold_pm": 350,
          "release_delta_pm": 80,
          "repress_delta_pm": 80
        }
      }
    },
    "bindings": {
      "unbound": "no_op"
    }
  },
  "control_assignments": [
    {
      "controls": ["@right_half_keys"],
      "type": "akey",
      "mode": "normal"
    }
  ],
  "control_overrides": {
    "key_r_000": {
      "mode": "rapid_trigger",
      "params": {
        "press_threshold_pm": 300,
        "release_delta_pm": 60,
        "repress_delta_pm": 60
      }
    }
  },
  "binding_scopes": {
    "base": {
      "priority": 0,
      "default_active": true,
      "unbound": "no_op",
      "bindings": {
        "key_r_000": "b_keyboard_a",
        "key_r_001": "b_keyboard_b"
      }
    }
  },
  "behaviors": {
    "b_keyboard_a": {
      "kind": "host_input",
      "usage": "keyboard.a"
    },
    "b_keyboard_b": {
      "kind": "host_input",
      "usage": "keyboard.b"
    }
  },
  "interaction_rules": []
}
```

## Debug Runtime Table Skeleton

Until the PC/Profile compiler exists, H417 can use a fixed debug table compiled
into firmware:

```text
control_index -> control_id -> HID usage
0             -> key_r_000  -> keyboard.a
1             -> key_r_001  -> keyboard.b
...
40            -> key_r_040  -> keyboard.<debug key>
```

This table is only a bring-up shortcut. It should be replaced by the real
`ProfilePackage -> RuntimeTable` flow when the configuration pipeline is ready.

## Open Decisions

- Final right-half physical key order and stable `control_id` list.
- Final `control_map_hash` generation input.
- Whether CH585 trigger parameters are copied from the active RuntimeTable or
  stored as a CH585 trigger-table side artifact.
- Final H417-to-left-CH585 wireless output command format.
