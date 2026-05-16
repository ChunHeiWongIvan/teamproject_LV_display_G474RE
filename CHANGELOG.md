# Changelog

## 2026-05-16

### GUI Optimisation
- Added DMA acceleration to the 16-bit to 24-bit RGB conversion stage required by the ILI9488 display driver, significantly reducing display flush duration.
- Enabled `-O2` compiler optimisation instead of `-O0` to improve GUI rendering and touchscreen responsiveness.
- Evaluated `-O3` optimisation; however, no significant performance improvement was observed compared to `-O2`, so `-O2` was retained for improved stability and debugging behaviour.
- Reduced display update latency, resulting in faster screen loading and improved touchscreen responsiveness.
