# Packaging

Senticli now includes CPack scaffolding for Linux packages.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Package

From the `build` directory:

```bash
cpack
```

Default generators:

- `TGZ` (`senticli-<version>-Linux.tar.gz`)
- `DEB` (`senticli_<version>_*.deb`)
