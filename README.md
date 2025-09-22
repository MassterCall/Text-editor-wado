# ⌨️ Wado Text Editor
A personal journey to build a simple, lightweight text editor in C from scratch. 🚀

📝 About This Project
The goal is to create a functional, terminal-based text editor by implementing features one step at a time. This project handles low-level terminal input, screen rendering, and user interaction to provide a basic but robust editing experience.

✨ Current Features
Raw Mode Enabled 🕹️: The terminal's canonical mode is disabled, allowing the program to process each keystroke instantly without waiting for the Enter key.

Arrow Key Navigation 🖱️: Full cursor movement is supported using the arrow keys.

Status Bar 📊: A status bar is displayed at the bottom of the screen, showing the editor's name and version.

Screen Refresh ✨: The screen is efficiently redrawn after each keypress using an append buffer to prevent flickering.

Safe Exit 🚪: The program exits cleanly when you press Ctrl-Q and safely restores the original terminal settings.
