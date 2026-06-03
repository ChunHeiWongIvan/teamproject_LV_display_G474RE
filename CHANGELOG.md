# Changelog

## 2026-05-16

### GUI Optimisation
- Added DMA acceleration to the 16-bit to 24-bit RGB conversion stage required by the ILI9488 display driver, significantly reducing display flush duration.
- Enabled `-O2` compiler optimisation instead of `-O0` to improve GUI rendering and touchscreen responsiveness.
- Evaluated `-O3` optimisation; however, no significant performance improvement was observed compared to `-O2`, so `-O2` was retained for improved stability and debugging behaviour.
- Reduced display update latency, resulting in faster screen loading and improved touchscreen responsiveness.

## 2026-05-25

### GUI finalised
- Added status screen to Main Menu; removed View Status button
- Statuses displayed: Overall charger state/ brief state description; Battery voltage/ Output voltage and current; CC/ CV parameters setting
- Monitor Charging reformatted: 9 plots on 9 pages for improved clarity and presentation
- Set Parameters finalised: Separate CV/CC setting pages, with logic to limit max output at 3 kW
- View Debug finalised: Display of boolean debug states separated by subsystem
