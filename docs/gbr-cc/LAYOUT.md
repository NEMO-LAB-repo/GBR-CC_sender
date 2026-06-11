# Repository Layout

```text
msquic_cellular/
  src/                    MsQuic source and GBR-CC hooks
  tools/cellninjia/       DIAG parser and GBR socket sender tools
  tools/build-scripts-scripts/            project-specific build helper scripts
  analysis/transport/     BBR/GBR transport analysis scripts
  docs/gbr-cc/            paper-specific implementation notes
  scripts/                upstream MsQuic packaging/runtime scripts
  bbr_logs -> artifacts/bbr_logs   compatibility link for local logs
  artifacts/              ignored local experiment outputs
```

`bbr_logs` and `logcode` may exist locally as compatibility symlinks. They are
ignored by Git so older hard-coded experiment commands can still run while the
tracked repository has a cleaner structure.
