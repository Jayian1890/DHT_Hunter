# Creating an App Icon for macOS

To create a proper .icns file for your macOS application, follow these steps:

## Option 1: Using Icon Composer (Easiest)

1. Create a high-resolution PNG image (1024x1024 pixels recommended)
2. Use an online converter like https://cloudconvert.com/png-to-icns or https://img2icnsapp.com/

## Option 2: Using Command Line Tools

1. Create a high-resolution PNG image (1024x1024 pixels recommended)
2. Create an iconset directory:
   ```
   mkdir AppIcon.iconset
   ```
3. Create various sized PNG files:
   ```
   sips -z 16 16 original.png --out AppIcon.iconset/icon_16x16.png
   sips -z 32 32 original.png --out AppIcon.iconset/icon_16x16@2x.png
   sips -z 32 32 original.png --out AppIcon.iconset/icon_32x32.png
   sips -z 64 64 original.png --out AppIcon.iconset/icon_32x32@2x.png
   sips -z 128 128 original.png --out AppIcon.iconset/icon_128x128.png
   sips -z 256 256 original.png --out AppIcon.iconset/icon_128x128@2x.png
   sips -z 256 256 original.png --out AppIcon.iconset/icon_256x256.png
   sips -z 512 512 original.png --out AppIcon.iconset/icon_256x256@2x.png
   sips -z 512 512 original.png --out AppIcon.iconset/icon_512x512.png
   sips -z 1024 1024 original.png --out AppIcon.iconset/icon_512x512@2x.png
   ```
4. Convert the iconset to .icns:
   ```
   iconutil -c icns AppIcon.iconset -o AppIcon.icns
   ```
5. Place the resulting AppIcon.icns file in this directory (resources/macos/)

## Using the Icon

Once you have created your .icns file, update the Meson build file to reference it:

```python
# In meson.build
app_icon_macosx = meson.project_source_root() / 'resources/macos/AppIcon.icns'

# When creating the app bundle
custom_target('app_bundle',
  # ...
  command: [
    # ...
    '&&', 'cp', app_icon_macosx, '@OUTPUT@/Contents/Resources/AppIcon.icns',
  ],
  # ...
)
```

Make sure the icon file is properly copied to the Resources directory in the app bundle.
