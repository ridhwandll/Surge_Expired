<p align="center">
  <picture>
    <img alt="Surge" src="Docs/Branding/Raptor.png" width="100%">
  </picture>
</p>

<h1 align="center">Rapt0R</h1>
<p align="center">
  A simple & fast(actually fast), header-only C++20 binary serialization library for high performance C++ applications<br>
</p>


It is under active development and meant to become the unified binary and runtime serialization layer of the [Surge Engine](https://github.com/ridhwandll/Surge_Expired). Also is is a learning project of my own

**At a glance:** writes structured data 1.16x to 1.23x faster than [zpp_bits](https://github.com/eyalz800/zpp_bits) and 2.4x to 20x faster than bitsery, glaze and cereal; reads are on par with or faster than all of them
([benchmarks](#benchmarks)).

## Features

- **Header-only, C++20, no dependencies**
- **Two calls only:** `Writer::Write(...)` and `Reader::Read(...)`, both variadic
- **Fused writes and reads:** consecutive fixed-size fields, strings, POD vectors and small flat containers share a single bounds check
- **Raw copies where safe:** trivially copyable types are copied with `memcpy`; everything else goes through your own `Rapt0RSerialize` / `Rapt0RDeserialize`
- **Standard library support:** `std::string`, `std::string_view` (zero-copy reads), `std::vector`, `std::array`, `std::pair`, `std::tuple`,
  `std::optional`, `std::variant`, `std::map`, `std::unordered_map`, `std::set`, `std::unordered_set`, and `std::pmr` variants
- **Allocation reuse:** reading into existing containers keeps their memory (vector capacity, map/set nodes, string buffers)
- **Hardened against hostile input:** truncated data, invalid bools/optionals/variants and oversized counts are rejected, before any allocation
- **No exceptions:** the first error closes the stream, later calls fail cheaply, and you check once at the end
- **Flexible buffers:** an owning, growable writer, or any fixed external buffer

## Getting started

The library is header only with no source file to compile, just copy everything from `Raptor/Include` and add it to your include path. Include this file and start using:

```cpp
#include <Raptor/Raptor.hpp>
```

## Usage

All serialization goes through `Write` and `Read`.

### Writing and reading values

```cpp
Rapt0R::Writer w; // Owning, grows on demand (4 KB initial capacity)
w.Write(uint32_t(42), std::string("hello"), std::vector<float> { 1.f, 2.f, 3.f });

std::span<const Rapt0R::Byte> bytes = w.WrittenSpan(); // Save it, send it, ...

Rapt0R::Reader r(bytes);
uint32_t id = 0;
std::string name;
std::vector<float> values;
if(!r.Read(id, name, values))
    HandleError(r.Status()); // BUFFER_OVERFLOW, CORRUPT_DATA, ...
```

`Write(a, b, c)` produces exactly the same bytes as `Write(a); Write(b); Write(c);`, only faster: consecutive flat
values share one capacity check and are copied through a cursor the compiler keeps in a register. Pass all fields of
a struct in one call.

### Custom types

Plain trivially copyable structs need no code; they are copied as raw bytes:

```cpp
struct Vec3 { float x, y, z; };
w.Write(Vec3 { 1.f, 2.f, 3.f });
```

For anything else, add a member pair:

```cpp
struct Entity
{
    uint64_t id = 0;
    std::string name;
    Vec3 position {};
    std::optional<double> health;
    std::vector<uint32_t> assets;
    std::unordered_map<std::string, uint64_t> attributes;

    void Rapt0RSerialize(Rapt0R::Writer& w) const
    {
        w.Write(id, name, position, health, assets, attributes);
    }
    void Rapt0RDeserialize(Rapt0R::Reader& r)
    {
        r.Read(id, name, position, health, assets, attributes);
    }
};
```

or, for types you cannot modify, free functions in the type's namespace (found through ADL via Rapt0R):

```cpp
namespace WeAreCharlieKirk
{
    inline void Rapt0RSerialize(Rapt0R::Writer& w, const Mesh& m) { w.Write(m.vertices); }
    inline void Rapt0RDeserialize(Rapt0R::Reader& r, Mesh& m)     { r.Read(m.vertices); }
}
```

Custom types nest freely: `std::vector<Entity>` or `std::map<std::string, Entity>` work out of the box.
A custom serializer always takes priority over a raw copy, so give one to any trivially copyable type that holds a
pointer or a `std::string_view`; otherwise the address itself would be written.

### Error handling

Errors are sticky. Once a writer or reader fails, it records the first error and every later call fails without
touching memory, so you can serialize a whole object graph and check once:

```cpp
w.Write(header);
w.Write(scene);
if(w.HasError()) // or: if(!w)
    Log("Serialization failed: {}", int(w.Status()));
```

| Error             | Meaning                                                                            |
|-------------------|------------------------------------------------------------------------------------|
| `BUFFER_OVERFLOW` | Not enough input left, a fixed write buffer is full, or an allocation failed        |
| `CORRUPT_DATA`    | A value that cannot be valid: a bool byte above 1, a bad optional flag or variant index |
| `SIZE_OVERFLOW`   | A length or element count above 4,294,967,295 cannot be encoded                    |
| `MISALIGNED_DATA` | A zero-copy span would point at memory not aligned for its element type            |
| `FAILED`          | Invalid request, e.g. a non-power-of-two alignment or a valueless variant          |

`Writer::Clear()` resets the writer and its error while keeping the buffer, so it can be reused every frame (This is recommneded for more speed as it reuses the allocations).

### Fixed buffers

```cpp
Rapt0R::Byte buffer[256];
Rapt0R::Writer w(buffer, sizeof(buffer)); // Never allocates, fails with BUFFER_OVERFLOW when full
```

### Zero-copy reads

Reading into a `std::string_view` points into the input buffer instead of copying. For bulk POD data,
`WriteSpan` / `ReadSpan` hand out a `std::span` directly over the input:

```cpp
w.WriteSpan(std::span<const Vec3>(vertices), /*alignToType*/ true);

Rapt0R::Reader r(w.WrittenSpan());
std::span<const Vec3> view;
r.ReadSpan(view, true); // O(1); fails with MISALIGNED_DATA instead of handing out a misaligned pointer
```

Views are only valid as long as the input buffer is.

### Faster reads into new vectors

`std::vector::resize` zero-fills memory that the payload then overwrites. `Rapt0R::DefaultInitAllocator` skips the
zero-fill, which speeds up first loads of large arrays by about 25%:

```cpp
std::vector<Vec3, Rapt0R::DefaultInitAllocator<Vec3>> vertices;
r.Read(vertices);
```

### Wire format

Values are written in the order you pass them, with no field names, tags or padding. Reader and writer must agree on
the layout.

| Type                                         | Encoding                                       |
|----------------------------------------------|------------------------------------------------|
| Trivially copyable value                     | `sizeof(T)` raw bytes                          |
| `bool`                                       | 1 byte, 0 or 1                                 |
| Strings                                      | `uint32` length + characters                   |
| `std::vector`, maps, sets                    | `uint32` count + elements (map: key, then value) |
| `std::optional`                              | `uint8` flag + value if set                    |
| `std::variant`                               | `uint8` index + active alternative             |
| `std::pair`, `std::tuple`, `std::array`      | members in order, no header                    |

## Benchmarks

*Note: Benchmark boilerplace is generated by AI and then cross-checked by human*
<br>

Measured on an AMD Ryzen 5 3600XT, Windows 11, MSVC 19.51, Release (`/O2`), against
[zpp_bits](https://github.com/eyalz800/zpp_bits) 4.7.6, [bitsery](https://github.com/fraillt/bitsery) 5.2.5,
[alpaca](https://github.com/p-ranav/alpaca) 0.3.0, [cereal](https://github.com/USCiLab/cereal) 1.3.2 and
[glaze](https://github.com/stephenberry/glaze) 9.0.0 (BEVE binary format).

Each chart compares every library with Rapt0R (the 1.00× line). Bars right of the line are slower, or larger for
output size. Bars are the median of 10 repetitions, whiskers span the fastest to the slowest repetition, and labels
show the ratio and the median time. `Rapt0R per-field` is the same library with one `Write`/`Read` call per field
instead of one fused call.

![Scene, 10,000 entities](Docs/Benchmarks/Scene10k.png)

![Scene, 100 entities](Docs/Benchmarks/Scene100.png)

![Mesh, 1,000,000 vertices](Docs/Benchmarks/Mesh1M.png)

How to read these numbers:

- **Writes are stable** (short whiskers), so Rapt0R's 1.16x to 1.23x lead over zpp_bits on the scenes is solid.
- **10,000-entity reads are noisy** (long whiskers, 10% to 30% spread). They are dominated by heap allocation and
  hashing, which every library pays. Where whiskers overlap, treat the difference as a tie.
- **The mesh is a single large `memcpy`** for both Rapt0R and zpp_bits, so those two are effectively tied there.
- **These are warm-cache numbers.** Each repetition runs many iterations back to back, so the source data stays in
  cache. They describe steady-state throughput, not first-touch performance.
- alpaca is missing from the scene charts on MSVC: its member counting breaks on structs with `std::optional` fields.

### Throughput suite

`RaptorTest` measures raw throughput on its own (same machine, idle, middle value of 3 runs):

| Workload                                               | Median     |
|--------------------------------------------------------|------------|
| Bulk mesh (32 MB) write                                | 18.96 GB/s |
| Bulk mesh read into existing vector                    | 19.20 GB/s |
| Bulk mesh read into new vector                         | 4.90 GB/s  |
| Bulk mesh read into new vector, `DefaultInitAllocator` | 6.17 GB/s  |
| Scene graph (10,000 entities, 1.56 MB) write           | 7.98 GB/s  |
| Scene graph read                                       | 0.91 GB/s  |
| Zero-copy `ReadSpan` of a 12 MB mesh                   | 1.42 ns    |

### How the benchmark works

The head-to-head benchmark lives in [`Benchmarks/CompetitorBench.cpp`](Benchmarks/CompetitorBench.cpp) and runs on
[Google Benchmark](https://github.com/google/benchmark) 1.9.5.

- **Same data, idiomatic code.** Every library serializes an identical data model, using its own recommended API: a
  `Rapt0RSerialize` pair for Rapt0R, automatic reflection for zpp_bits, alpaca and glaze, `serialize` functions for
  bitsery and cereal. All libraries are compiled in one translation unit with the same flags (C++23, which glaze requires).
- **Workloads.** *Scene*: entities with a 64-bit id, a 22 to 24 character name, a position, a rotation quaternion, an
  optional health value (on every second entity), five asset indices and an `unordered_map` of two attributes,
  at 10,000 entities (1.4 MB) and at 100 entities (14 KB). *Mesh*: one vector of 1,000,000 three-float vertices (12 MB).
- **Operations.** `Write` serializes into a reused buffer. `ReadReuse` deserializes into an object that already
  holds the previous result, like a hot reload or a network tick. `ReadFresh` deserializes into a new object, like a
  first load; it is timed around the read call only, so building and destroying the object are not counted.
- **Verification first.** Before anything is timed, each library's output is read back, both into a fresh object
  and into an already filled one, and compared with the original. A library that fails is reported as an error, never
  timed.
- **Fair measurement.** Each benchmark runs 10 repetitions, and the repetitions of all benchmarks are shuffled into
  random order, so no library always runs first on a cold heap or right after the same neighbour. The median is reported.

### Running the benchmarks

```sh
cmake -B build -DRAPTOR_BUILD_COMPETITOR_BENCHMARKS=ON   # Downloads Google Benchmark and the five competitors
cmake --build build --config Release

./build/Benchmarks/Release/RaptorCompetitorBench          # Head-to-head comparison (~90 s)
./build/Release/RaptorTest                                # Throughput suite
./build/Release/RaptorUnitTests                           # Unit tests
```

The paths above are for multi-configuration generators such as Visual Studio; with Ninja or Makefiles, drop the
`Release/` folder. Close other programs while measuring: background load easily moves results by 30% or more.

Benchmarks are named `<Workload>/<Operation>/<Library>`, and every Google Benchmark flag works:

```sh
RaptorCompetitorBench --benchmark_filter=Scene10k/Write/    # One workload and operation
RaptorCompetitorBench --benchmark_filter=/Rapt0R$           # Rapt0R only
RaptorCompetitorBench --benchmark_repetitions=20            # More samples
```

### Generating the results and charts

A run prints Google Benchmark's usual per-benchmark statistics (mean, median, standard deviation and coefficient of
variation over the repetitions), followed by a summary table that compares each library's median with Rapt0R and
confirms that every round-trip succeeded.

To keep the full results, write them to a JSON file. It contains every individual repetition, not just the summary:

```sh
RaptorCompetitorBench --benchmark_out=results.json
```

The charts in this README are drawn from that file by [`Benchmarks/plot_results.py`](Benchmarks/plot_results.py)
(needs `matplotlib`). It computes each library's median and fastest/slowest repetition relative to Rapt0R, reads the
output sizes from the `wire_bytes` counter, and writes one PNG per workload:

```sh
python Benchmarks/plot_results.py results.json Docs/Benchmarks   # Writes Scene10k.png, Scene100.png and Mesh1M.png
```

To check whether a change to Rapt0R is really faster, save a run before and after the change and compare them with
Google Benchmark's `compare.py`. It runs a significance test for each benchmark (needs `numpy` and `scipy`):

```sh
RaptorCompetitorBench --benchmark_filter=/Rapt0R --benchmark_out=before.json
# ... rebuild with the change ...
RaptorCompetitorBench --benchmark_filter=/Rapt0R --benchmark_out=after.json
python build/_deps/googlebenchmark-src/tools/compare.py benchmarks before.json after.json
```

Results depend on CPU, compiler and memory, so measure on your own target hardware.

## Testing

`RaptorUnitTests` ([`Tests/Tests.cpp`](Tests/Tests.cpp)) is a dependency-free test program, also registered with
CTest (`ctest --test-dir build -C Release`). It covers round-trips of every supported type, byte-for-byte equality of
fused and per-field calls, allocation reuse, and hostile input: truncated streams, corrupt values, oversized counts and
misaligned spans.

## Limitations

- **Little-endian only.** The wire format is raw native memory, which a `static_assert` restricts to little-endian.
  targets (every mainstream desktop, mobile and console CPU). I do not plant to add support for Big-endian.
- **No schema evolution.** There are no field names or versions in the stream, so adding or reordering fields breaks
  existing data.
- **Raw copies include padding.** Structs with padding write those bytes as-is; give them a serializer if you need
  byte-identical output.
- Measured on MSVC only. But should work with other compiler too as it is written in a compiler independent way.

<br>

# Projects and Products using Rapt0R

- [Surge Engine, RidT](https://github.com/ridhwandll/Surge_Expired)

Feel free to open and Issue/PR to add your project name here if you are using Rapt0R!

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="Docs/Branding/SurgeLogo-DarkMode.png">
    <source media="(prefers-color-scheme: light)" srcset="Docs/Branding/SurgeLogo-LightMode.png">
    <img alt="rid TECH" src="Docs/Branding/SurgeLogo-LightMode.png" width="90">
  </picture>
</p>
