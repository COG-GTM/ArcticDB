# Branch work log

- Bounded V2 segment decoding against the segment buffer before compressed blocks reach the zstd/lz4/passthrough decoders (code scan finding sfind-4dae3fb4).
- `check_compressed_size_in_bounds` (codec-inl.hpp) is called for metadata, descriptor, index descriptor, string pool and every column field in `decode_v1`/`decode_v2`.
- `Segment::from_bytes`/`from_buffer` now reject V2 segments whose `footer_offset` exceeds the readable size, or whose header and body field compressed sizes exceed `footer_offset`.
- Added C++ regression tests in `codec/test/test_codec.cpp` for oversized metadata blocks, oversized column blocks at decode time, and tampered serialized footers.
- Local build not possible (network-restricted VM, no vcpkg deps); verification relies on CI.
- Review follow-up: `decode_header_and_fields` validates the fixed header and `header_bytes` against the readable size before deserializing the header; added truncated-header test; documented the decoding-safety checks in `docs/claude/cpp/CODEC.md`.
