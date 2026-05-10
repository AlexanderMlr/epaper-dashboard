# LilyGo ePaper Dashboard 

An info screen for the LilyGo T5 EPD47-S3 e-paper board. Shows a weather
forecast on the left half of the screen and, depending on time of day, either
public-transit commute options or upcoming calendar events on the right half.
A footer line reports last-update time, next refresh, and battery level.

## Hardware

- LilyGo T5 4.7" e-paper (S3 variant)
- Powered via exteral LiPo or USB

## What it shows

- **Weather** — multi-step forecast from Open-Meteo (icon, temperature,
  conditions).
- **Commute (morning)** — Deutsche Bahn journeys via the
  [transport.rest](https://v6.db.transport.rest) wrapper around HAFAS,
  including delays, transfers, and platform info.
- **Calendar (rest of day)** — upcoming events parsed from a Google Calendar
  private ICS link.
- **Bike vs. train recommendation** — derived from the weather forecast over
  the commute window.

The right-half panel switches between commute and calendar based on the local
hour (`COMMUTE_HOURS_START` / `COMMUTE_HOURS_END`). Outside the commute
window the device also refreshes less frequently to save battery.

## Setup

1. Install [PlatformIO](https://platformio.org/) (CLI or VS Code extension).
2. Copy `src/config.example.h` to `src/config.h` and fill in:
   - WiFi credentials
   - Location latitude/lonitude
   - DB station IDs (EVA numbers) for your commute
   - Google Calendar private ICS URL
     (Calendar settings → Secret address in iCal format)
3. Build & flash

## Notes

- TLS certificates are not pinned (`setInsecure()` in both API clients).
  Acceptable for read-only personal data; revisit before exposing the device.
- During development, set `ARDUINO_USB_CDC_ON_BOOT=1` in `platformio.ini` to
  get a serial console. Set back to `0` to enable battery use.
- A short hold-off on cold boot leaves a window for re-flashing before
  the device enters its deep-sleep cycle.

## Credits

- Weather icons from the
  [Basic Rounded Lineal](https://www.flaticon.com/search?author_id=1&style_id=4&type=standard&word=weather)
  set by [Freepik](https://www.flaticon.com/authors/freepik) on Flaticon.