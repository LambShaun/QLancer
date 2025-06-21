# QLancer - A Lightweight and Fast Keyboard Launcher for Windows

 **QLancer** is a keyboard-driven, high-performance launcher designed for Windows power users. Built with pure Win32 API, it aims for blazing-fast performance and a minimal resource footprint. Whether you're a developer, a writer, or an efficiency enthusiast, QLancer helps you ditch the mouse and launch apps, run commands, and manage processes in the fastest way possible.

### ✨ Features

* **Fast App Search**: Get instant, fuzzy-search results for all your applications from the Start Menu, Desktop, and Program Files.
* **Keyboard-Driven**: Invoke with `Alt` + `Space` by default. Every action is designed to be completed without leaving the keyboard.
* **System Commands**: Built-in support for common system commands. Just type `shutdown`, `restart`, `sleep`, or `empty recycle bin` to execute them.
* **Process Management**: Use the `quit` keyword to quickly find and terminate running non-critical processes, or even quit all eligible applications at once.
* **Modern UI**: A clean, minimalistic dark theme that keeps you focused on your search.
* **Extremely Lightweight**: Developed with C++ and the pure Win32 API. No external frameworks or dependencies, ensuring a very low memory footprint.
* **Run on Startup**: Easily configure the app to run on system startup via the tray icon context menu.
* **Highly Extensible**: The codebase is clean and well-structured, making it easy to extend and customize.

---

### 🚀 How to Use

1.  Go to the [GitHub Releases](https://github.com/LambShaun/QLancer/releases) page and download the latest version (e.g., `QLancer.zip`).
2.  Extract the archive to a location of your choice (e.g., `D:\QLancer`).
3.  Run `QLancer.exe`.
4.  Press `Alt` + `Space` to get started!

---

### 🔧 How to Build (For Developers)

If you wish to compile the project yourself or contribute to its development, follow these steps:

1.  **Prerequisites**:
    * Visual Studio 2022 (or a later version)
    * Windows SDK

2.  **Build Steps**:
    * Clone this repository to your local machine: `https://github.com/LambShaun/QLancer.git`
    * Open the `.sln` solution file in Visual Studio.
    * Select the `Release` and `x64` configuration.
    * Select "Build" -> "Build Solution" (or press F7).
    * The compiled executable will be located in the `x64/Release` directory.

---

### 💻 Technology Stack

* **Core Language**: C++
* **Core API**: Windows API (Win32)

This project does not rely on any heavy frameworks like MFC, ATL, or Qt, aiming to provide a pure and efficient native Windows experience.

---

### 🤝 How to Contribute

Contributions of all kinds are welcome! If you have ideas, suggestions, or have found a bug, please feel free to:

* Open an [Issue](https://github.com/LambShaun/QLancer/issues) to provide feedback.
* Fork the repository and submit a Pull Request.

---

### 📄 License

This project is licensed under the [MIT License](LICENSE).