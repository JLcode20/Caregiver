Caregiver

An ESP32-Based Caregiving Assistance Project

Caregiver is an embedded systems project developed using the ESP32 platform and the Arduino framework. This repository contains the project's source code and configuration files for development, testing, and future improvements.

«Status: Test Release — v0.1.0»

Overview

The Caregiver project is currently in its initial testing stage. It provides a foundation for developing and evaluating an ESP32-based caregiving assistance system.

Further development and testing are needed before the project can be considered stable.

Technologies Used

- Microcontroller Platform: ESP32
- Framework: Arduino
- Development Environment: PlatformIO
- Programming Language: C++
- RTC Library: Adafruit RTClib
- Display Libraries: Adafruit GFX Library and Adafruit SSD1306

Project Structure

Caregiver/
├── include/        # Project header files
├── lib/            # Project-specific libraries
├── src/            # Main source code
├── test/           # Test files
├── .vscode/        # VS Code configuration
├── platformio.ini  # PlatformIO configuration
└── README.md       # Project documentation

Requirements

Before running the project, prepare the following:

- An ESP32-compatible development board
- A computer with Visual Studio Code
- The PlatformIO IDE extension
- A USB cable compatible with your development board

Additional hardware requirements will depend on the features implemented in the source code.

Getting Started

1. Clone or download this repository.
2. Open the project folder in Visual Studio Code.
3. Install the PlatformIO IDE extension if it is not already installed.
4. Allow PlatformIO to install the configured platform and libraries.
5. Connect your ESP32 development board to your computer.
6. Build the project to check for compilation errors.
7. Upload the firmware to the board when the hardware setup is ready.
8. Open the PlatformIO Serial Monitor to view available serial output.

Configuration

The "platformio.ini" file contains the board, framework, library dependencies, and serial monitor configuration.

The configured serial monitor speed is 115200 baud.

Testing Status

This is an early test release intended for development and evaluation.

- [ ] Verify successful compilation.
- [ ] Test firmware upload to the development board.
- [ ] Verify connected hardware and peripherals.
- [ ] Evaluate implemented features.
- [ ] Fix identified bugs and issues.

These are testing tasks, not claims that the tests have already passed.

Known Limitations

- Features may be incomplete or subject to change.
- Hardware compatibility and behavior require testing.
- This release is not intended to be treated as a validated medical or emergency-response system.

Feedback and Contributions

Bug reports, suggestions, and improvement ideas are welcome.

Please use the repository's "Issues page" (https://github.com/JLcode20/Caregiver/issues) to report problems or suggest improvements.

Version

Current Release: v0.1.0 — Initial Test Release

See the "Releases page" (https://github.com/JLcode20/Caregiver/releases) for available versions.

License

No license has been specified yet. Usage, modification, and redistribution permissions should be defined by the project owner.

---

Developed as an ongoing embedded systems project. Features and documentation will be updated as development progresses.
