# Programs

This directory contains standalone example/utility programs that use the
`opendroneid-core-c` libraries.

## remoteidv1

`remoteidv1` is a terminal window (ncurses) scanner that periodically runs
`iw dev <iface> scan`, extracts ASTM Remote ID Wi-Fi beacon vendor elements
(OUI `fa:0b:bc`), and decodes the Open Drone ID message pack using
`odid_message_process_pack()` from `libopendroneid`.
