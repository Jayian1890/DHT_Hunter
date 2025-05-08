-- This is an AppleScript file to customize the appearance of the DMG window
-- It will be used by CPack when creating the DMG file

on run
    tell application "Finder"
        tell disk "BitScrape"
            -- Open the window
            open
            
            -- Set the window size
            set the bounds of container window to {400, 100, 940, 480}
            
            -- Set the view options
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            
            -- Set the icon size and arrangement
            set icon size of icon view options of container window to 128
            set arrangement of icon view options of container window to not arranged
            
            -- Position the application icon
            set position of item "BitScrape.app" of container window to {160, 200}
            
            -- Create an alias to the Applications folder
            make new alias file at container window to POSIX file "/Applications" with properties {name:"Applications"}
            set position of item "Applications" of container window to {380, 200}
            
            -- Close the window
            close
            
            -- Reopen and update
            open
            update without registering applications
            delay 5
            close
        end tell
    end tell
end run
