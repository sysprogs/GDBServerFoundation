The GDBServerFoundation is a framework that greatly simplifies building gdbserver-compatible stubs. It handles packet decoding, RLE encoding, escaping/unescaping and provides implementations for the most common GDB packet types.

Building your own gdbserver-compatible stub with GDBServerFoundation is as easy as implementing the ISyncGDBTarget interface.