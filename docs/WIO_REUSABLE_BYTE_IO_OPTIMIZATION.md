# N04: caller-owned byte read buffers

2026-10-19. This is an opt-in reusable-buffer slice of N04, not closure of N04,
N14, VM optimization or the compiler-throughput pass.

## Contract and implementation

`std::io::ReadBytesInto(file: ref File, buffer: ref u8[]) -> usize` fills the
buffer's existing length exactly. `ReadSomeBytesInto` performs a bounded read and
returns the valid prefix length without changing the destination length. Its
unread suffix stays intact. Reserve is not resize: a reserved empty array still
requests zero bytes. Empty destinations return zero without validating the file,
as with existing zero-count reads. Nonempty reads preserve the existing file
validation, OS error classification, partial byte counts and cursor behavior.
Exact-read failure does not promise transactional buffer contents or rollback.

The native runtime uses the existing raw read primitives with a mutable span of
caller elements. The vector overload passes its storage directly; neither size
nor capacity changes and there is no intermediate owning payload. The C++ span
overload also supports subranges, checked with neighboring canaries. Old owning
reads still return independent arrays and keep their existing behavior.

The public Wio surface uses ordinary mutable array borrows. No compiler intrinsic,
ABI version change, unchecked arbitrary pointer or buffer-pool global is added.
Native caller reuse is not automatically a zero-copy VM path: the VM's generic
array bridge still materializes/copies elements. VM zero-copy/bulk transport is
separate work and must retain lifetime and boundary checks.

## Measurement design

The expanded byte-I/O fixture has four cases: owned exact/some and reusable
exact/some. Every language performs the same 257 seeks plus 64 KiB reads plus
full-byte FNV checksum (16.0625 MiB). Reusable storage is prepared before warmups,
the measured kernel and allocation window in Wio, C++ and C#. Setup/open/close
stay outside the kernel; the cache is hot, not a cold disk throughput test.
C++ uses ifstream, C# FileStream with managed buffering disabled, and Wio OS file
handles. Error/syscall implementations are not identical across languages.

Compare storage strategies inside the same selected session. Expanded source
files and binaries differ from the two-case historical fixture, so preserve new
hashes and do not call them unchanged inputs. Historical original/previous
observations remain in the [direct-read](WIO_BYTE_IO_OPTIMIZATION.md) and
[runtime-profile](WIO_RUNTIME_PROFILE_OPTIMIZATION.md) reports, with their timing
qualification limits. No favorable retry/trimming or power/priority change.

The expanded local byte-I/O gate passes 1/1 in 117.64 seconds (29 processes):
native O0/O2, allocation/span controls and both O0/O2 VM bridges, including
reusable partial/closed errors. The C++ reuse loop performs zero allocations.
Cross-language performance and actual Wio heap evidence remain pending at this
source commit. The
initial fixture mistakenly used an unsupported array `Resize` member; it was
corrected to explicit Reserve/Push initialization outside the measured kernel.
Do not hide that failed test or treat it as a runtime regression.
