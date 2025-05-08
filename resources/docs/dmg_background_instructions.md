# Creating a DMG Background Image

To create a background image for your macOS DMG installer:

1. Create a PNG image with dimensions of 540x380 pixels
2. Save it as `dmg_background.png` in this directory (resources/macos/)
3. The image will be displayed as the background when users open the DMG file

## Design Tips

- Keep the design simple and clean
- Include clear instructions for installation (typically an arrow pointing to the Applications folder)
- Use high-quality graphics
- Test the appearance on both light and dark mode

## Example Layout

```
+----------------------------------+
|                                  |
|                                  |
|                                  |
|   +-------+           +-------+  |
|   |       |           |       |  |
|   |  App  |   ---->   | Apps  |  |
|   |       |           |       |  |
|   +-------+           +-------+  |
|                                  |
|   Drag DHT Hunter to Applications|
|                                  |
+----------------------------------+
```

You can find many DMG background templates online that you can customize for your application.
