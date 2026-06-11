# secnetperf Experiments

Use this directory for sanitized bulk-transport experiment recipes. Local scripts
that contain machine-specific addresses, passwords, or paths should stay under
`artifacts/local/` and should not be committed.

The main binary is built at:

```text
build/bin/Release/secnetperf
```

Typical GBR-CC upload mode:

```bash
./build/bin/Release/secnetperf -target:<server> -cc:bbr -upload:200mb -cellular:1
```
