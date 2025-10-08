# wgetX

A simple HTTP client that downloads web content from a given URL.

## Building

```bash
make all
```

This builds the `wgetx` executable.

## Usage

```bash
./wgetx <url>
```

Downloads the content from the specified URL and saves it to a file named `received_page`.

## Example

```bash
./wgetx http://example.com
```

## Cleaning

```bash
make clean
```

Removes compiled object files and the executable.
