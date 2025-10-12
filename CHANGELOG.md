# VitaShell 2.15 (mod by theheroGAC)

## What's New in 2.15

### Settings & Configuration Changes
- **DISABLED Automatic Updates**: Automatic update checks disabled by default for faster startup
- **Battery Info Relocation**: Moved Battery Information from Power Settings to Main Settings menu
- **Audio Repeat Mode**: Added complete audio repeat functionality with 3 options (None/Repeat One/Repeat All)
- **Enhanced Settings Menu**: Reorganized settings layout for better user experience

### Menu Structure Updates
- **Main Settings Menu**: Added Battery Info and Audio Repeat Mode options
- **Power Settings Menu**: Streamlined to core power functions (Reboot/Power Off/Standby)
- **Improved Navigation**: Better organized settings with logical grouping

### Localization Updates
- **Italian Language Enhancement**: Updated both resources/italian.txt and l10n/italian.txt
- **New Translation Strings**: Added translations for all new settings options
- **Complete Localization**: Full Italian support for all new features

### Technical Improvements
- **Faster Startup**: Disabled automatic network update checks for improved boot time
- **Manual Updates**: Users can still check for updates manually via "Check Updates" option
- **Configuration Persistence**: All new settings properly saved and restored
- **Memory Optimization**: Efficient settings handling and menu navigation

---

# VitaShell 2.14 (mod by theheroGAC)

## What's New in 2.14

### Major Fixes and Improvements
- **Performance Optimizations**: Major QR scanner speed improvements with intelligent frame processing

### Performance Enhancements
- **Intelligent Frame Skipping**: Adaptive frame processing based on detection success rate
- **Batch Pixel Processing**: Optimized grayscale conversion processing 4 pixels simultaneously
- **Dynamic Delay Adjustment**: Automatic performance tuning (33ms-150ms range)
- **Early Exit Optimization**: Faster QR detection with priority-based code checking
- **Thread Priority Boost**: Maximum priority threads for optimal QR processing performance

### Technical Updates
- **Version Bump**: Updated to 2.14 across all build systems
- **Memory Optimization**: Reduced memory footprint in QR processing
- **Adaptive Algorithm**: Self-tuning performance based on success rates
- **Cache-Friendly Processing**: Optimized memory access patterns for better CPU utilization

### Localization & Themes
- **French Translation**: Complete French localization by chronoss09
- **Japanese Translation Update**: Enhanced Japanese translation by yyoossk
- **Electron Theme**: Updated Electron theme with improved styling and compatibility

### Code Quality
- **Clean Codebase**: Removed deprecated audio feedback systems
- **Optimized Algorithms**: Faster mathematical operations for image processing
- **Better Threading**: Improved thread priorities and resource management
- **Performance Monitoring**: Built-in performance tracking and adaptation

---

# VitaShell 2.13 (mod by theheroGAC)

## What's New in 2.13

### Major New Features
- **One-Time QR Scanner**: Access QR scanner once via SELECT button, then auto-switches to USB mode
- **Selection Bar Color**: Choose from 6 colors for file selection bars (Green, Grey, Blue, Red, Pink, Yellow)

### Technical Updates
- **Version Bump**: Updated to 2.13
- **Smart Menu Behavior**: QR option disappears after use for clean UI
- **Persistent Settings**: Color choices saved across sessions
- **Optimized UI**: Improved navigation and fallback behaviors
- **Updated Japanese Translation**: Thanks to yyoossk for improved Japanese localization

###  Known Limitations
- QR scanner limited by design to prevent abuse
- One-time use policy ensures secure functionality

---

This release adds convenient QR integration and UI customization while keeping VitaShell stable and user-friendly.


### Credits & Acknowledgments

- **Original Author**: TheFloW - Creator of VitaShell
- **Core Contributors**: isage and TheRealYoti - Major code and feature contributions
- **Mod Developer**: theheroGAC - Multilingual implementation and enhancements (active development since v2.06, continuously adding new features and fixing issues)
- **Version 2.15 Modifications**:
  - Disabled automatic updates for faster startup
  - Relocated Battery Info to Main Settings menu
  - Added complete Audio Repeat Mode functionality
  - Enhanced Italian language localization
- **Performance Optimization**: Advanced QR scanner optimizations and threading improvements
- **French Translation**: chronoss09 - Complete French localization
- **Japanese Translation**: yyoossk - Enhanced Japanese translation updates
- **Italian Translation**: Enhanced Italian localization for new features
- **Theme Development**: Electron theme improvements and compatibility updates
- **Community**: Beta testers and language contributors
- **Open Source**: Thanks to all contributors and the VitaSDK team
--

This custom build focuses on user experience improvements with faster startup times, better organized settings menus, complete audio repeat functionality, and enhanced localization support!
