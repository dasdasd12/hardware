# Legacy host helpers

Files in this directory are retained only to explain or reproduce older
bring-up sessions. They are not part of the supported test workflow.

- `read_serial.ps1` is a hard-coded COM5 capture helper. Current tests should
  ship a reader with explicit command-line port and timeout arguments.
