# GBR-CC Repository Notes

## Paper Mapping

This repo corresponds to the MsQuic transport implementation used for the bulk
upload experiments in the GBR-CC paper. The related WebRTC/GoogCC implementation
is in the sibling repository `~/webrtc-local`.

## Data Path

1. The DIAG parser in `tools/cellninjia/diag_get_5g_msquic.py` or
   `tools/cellninjia/diag_get_lte_msquic.py` extracts BSR demand and uplink grant/PUSCH
   allocation.
2. The parser computes `GBR = granted_resource / requested_resource`.
3. The parser sends two little-endian doubles, `(ratio, saturation)`, to
   `/tmp/msquic_cellular_ratio.sock`.
4. `src/perf/lib/cellular_ratio.c` receives samples on a background thread and
   exposes the latest values through weak globals.
5. `src/core/bbr.c` reads fresh samples and updates the ratio pacing rate and
   ratio cwnd when `-cellular:1` is enabled.

## Runtime Outputs

The following are generated artifacts and are ignored by Git:

- `artifacts/bbr_logs/`
- `build/`
- `tools/cellninjia/test0/`
- `tools/cellninjia/pre_hdlc_hex/`
- `tools/cellninjia/pre_hdlc_raw/`
- root-level plots, paper PDFs, packet captures, `.out`, and `.deb` files

Keep large experimental results in an external archive, release asset, or a
separate artifact repository. Do not commit new logs or generated plots to this
source repository.

## Known Cleanup Still Needed

- Move ratio-control globals from process-wide static state into per-connection
  BBR state before relying on multi-connection experiments.
- Replace hard-coded `/home/qwu26/msquic_cellular` paths with environment
  variables such as `MSQUIC_LOG_DIR` and `MSQUIC_CELLULAR_RATIO_SOCK`.
- Make per-packet logging optional or buffered; current logging can perturb
  high-throughput measurements.
- Guard all weak cellular symbols before reading them from generic library code.
