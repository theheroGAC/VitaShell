# GitHub cache buster - force refresh

# VitaShell 2.12 (mod by theheroGAC)

## Major Release - Auto-Launch Suite & Enhanced Installation Workflows

### New Features

#### Post-Installation Auto-Launch (⭐ MAJOR FEATURE ⭐)
- **App Launch Prompt**: After VPK installation, users are prompted to launch the installed app/game
- **Immediate Launch**: Click "Yes" to start the app immediately after installation
- **Title ID Extraction**: Automatically extracts Title ID from VPK param.sfo for correct app launching
- **Multilingual Prompts**: Launch dialog adapts to currently selected language
- **User Choice**: Option to decline launch and return to file browser
- **Seamless Experience**: VitaShell closes automatically when launching installed app

#### "Launch app/game" Context Menu Option for VPK Files (⭐ NEW ⭐)
- **TRIANGLE Menu Access**: Press TRIANGLE on any VPK file → "Launch app/game" appears in context menu
- **Smart VPK Detection**: Option appears automatically only for TYPE_VPK files (no manual detection needed)
- **One-Click Installation & Launch**: Selects "Launch app/game" → installs VPK + launches app immediately
- **Skip Confirmation Dialogs**: No intermediate "Do you want to launch?" prompts when using direct method
- **Error Safety**: Displays error dialogs if installation fails (network issues, storage space, etc.)
- **Dual Method Compatibility**: Traditional installation (with launch prompts) remains fully available
- **Intelligent Context**: Menu adapts based on file type - smart, intuitive, and clean UX

#### Smart Installer Workflows
- **Dual Installation Methods**: Traditional method with user choices vs. direct launch method for quick deployment
- **Installation State Management**: Proper tracking of installation states and cleanup
- **UI Responsiveness**: Prevents UI freezing and ensures proper control restoration after installations
- **Intelligent File Handling**: Contextual menu appears only for compatible file types

#### Other Improvements
- **Enhanced Installation Flow**: More intuitive user experience with smart prompts
- **Automatic Cleanup**: Last installed app tracking and cleanup
- **Responsive Controls**: Fixed issues where VitaShell would freeze after installation dialogs
- **Simplified User Dialogs**: Removed redundant confirmation messages for better UX

### Inherited Features (from v2.11)
This release maintains all features from VitaShell 2.11 and requires VitaShell 2.11 as prerequisite. All previous functionality including complete multilingual support, enhanced settings menu, progress display improvements, custom LiveArea template, and repository migration are preserved.

### Technical Improvements

#### Code Quality
- **Unicode Support**: Full UTF-8 support for international characters
- **Memory Management**: Optimized resource handling for language files
- **Backward Compatibility**: All existing features preserved
- **Clean Architecture**: Modular language system implementation

#### User Experience
- **Intuitive Controls**: Standard PS Vita button mapping (O=confirm, X=cancel)
- **Error Prevention**: Crash fixes and better error handling
- **Responsive UI**: Smooth language switching without lags
- **Accessibility**: Multiple language options for global users

### Installation & Compatibility

#### Version Update
- **Bumped from 2.11 to 2.12**
- **Hex Version**: `0x020C` (2.12)
- **Release Template**: Updated with mod branding

#### Build System
- **Embedded Resources**: All language files included in binary
- **Automatic Setup**: Language files install automatically
- **Cross-Platform**: Maintains compatibility with VitaSDK builds

### Bug Fixes

#### Stability
- **Crash Prevention**: Fixed potential crashes during language operations
- **Memory Leaks**: Proper cleanup of language resources
- **Dialog Consistency**: Fixed button mapping in confirmation dialogs

#### UI/UX Fixes
- **Menu Navigation**: Improved settings menu flow
- **Text Rendering**: Better handling of international characters
- **Progress Display**: Fixed counters in transfer operations

### Development Notes

#### Architecture Changes
- **Modular Design**: Separate language system for easy expansion
- **Resource Management**: Efficient loading of embedded resources
- **Configuration System**: Persistent language preferences
- **Plugin Architecture**: Future extensibility for more languages

#### Testing
- **Multi-language Testing**: Verified on all 12 language variants
- **UI Testing**: Comprehensive menu interaction testing
- **Performance Testing**: Memory usage and load times optimized

#### Translations
- **Updated Japanese Translation**: Special thanks to yyoossk for improving Japanese localization

### Credits & Acknowledgments

- **Original Author**: TheFloW - Creator of VitaShell
- **Core Contributors**: isage and TheRealYoti - Major code and feature contributions
- **Mod Developer**: theheroGAC - Multilingual implementation and enhancements (active development since v2.06, continuously adding new features and fixing issues)
- **Community**: Beta testers and language contributors
- **Open Source**: Thanks to all contributors and the VitaSDK team

### Statistics
- **Lines Modified**: 3,200+ lines across critical modules
- **Languages Added**: 12 complete translations with full UI localization
- **New Features**: 10+ major enhancements including dual installation workflows
- **Bug Fixes**: 18+ issues resolved including UI freezing and dialog management
- **Files Changed**: 28+ files including core installer, menu system, and language module
- **VPK Handling**: Enhanced with intelligent file type recognition and context-aware menus
- **Installation Workflows**: Dual-method system for traditional and quick-deployment scenarios

---

This release transforms VitaShell into a professional, user-friendly, and intelligent application manager with advanced installation automation and comprehensive internationalization!
