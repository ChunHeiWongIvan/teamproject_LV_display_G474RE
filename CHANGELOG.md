# 2026-06-09

## Charger HMI Updates

### UI Improvements

* Updated main menu title to **"EZEV Charger"**
* Added additional debug variables to the debug screen, synchronising with HV MCU
* Renamed temperature graphs:

  * Temperature 1 → **PFC Temperature**
  * Temperature 2 → **PSFB Temperature**
  * Temperature 3 → **Enclosure Temperature**
* Increased voltage graph maximum range from **600 V** to **700 V**

### CAN / DBC Updates

* Split BMS communication into separate CAN messages:

  * **CHARGER_RX_CCCV** for CV/CC setpoints
  * **CHARGER_RX_FAULT** for BMS fault/status information
* Updated DBC definitions to match new message structure
* Removed `BMS_Status` from CCCV message and moved it into dedicated fault message

### HMI Behavior Changes

* Automatically return to the main menu on charger state transitions
* Improved fault screen behavior:

  * Output voltage and current are no longer displayed while in fault state

### Debug & Diagnostics

* Power graph bug fix


# 2026-05-25

## GUI finalised
- Added status screen to Main Menu; removed View Status button
- Statuses displayed: Overall charger state/ brief state description; Battery voltage/ Output voltage and current; CC/ CV parameters setting
- Monitor Charging reformatted: 9 plots on 9 pages for improved clarity and presentation
- Set Parameters finalised: Separate CV/CC setting pages, with logic to limit max output at 3 kW
- View Debug finalised: Display of boolean debug states separated by subsystem

# 2026-05-16

## GUI Optimisation
- Added DMA acceleration to the 16-bit to 24-bit RGB conversion stage required by the ILI9488 display driver, significantly reducing display flush duration.
- Enabled `-O2` compiler optimisation instead of `-O0` to improve GUI rendering and touchscreen responsiveness.
- Evaluated `-O3` optimisation; however, no significant performance improvement was observed compared to `-O2`, so `-O2` was retained for improved stability and debugging behaviour.
- Reduced display update latency, resulting in faster screen loading and improved touchscreen responsiveness.
