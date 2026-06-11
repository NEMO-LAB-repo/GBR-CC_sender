# CellNinjia / DIAG GBR Tools

This directory contains the runtime telemetry tools used to compute the
Grant-to-BSR Ratio (GBR) from LTE/5G DIAG streams and feed MsQuic.

Important entry points:

- `diag_get_5g_msquic.py`: 5G parser that sends GBR samples to MsQuic.
- `diag_get_lte_msquic.py`: LTE parser that sends GBR samples to MsQuic.
- `hdlc.py`: HDLC encoder/decoder used by both parsers.
- `cellninjia_mobile/`: Android-side native helper for DIAG streaming.

Runtime flow:

```text
cellninjia_mobile on phone
  -> adb forward tcp:43555 tcp:43555
  -> diag_get_*_msquic.py on host
  -> /tmp/msquic_cellular_ratio.sock
  -> MsQuic cellular_ratio.c / bbr.c
```

Archived analysis scripts, notebooks, sample traces, generated plots, and old
runtime outputs were moved under:

```text
artifacts/archive/cellninjia_aux_20260610_1736/
```

Runtime outputs such as `test0/`, `pre_hdlc_raw/`, decoded messages, debug logs,
generated plots, and native binaries are ignored by Git.
