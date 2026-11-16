# Testing Zippypet with Wokwi Simulator

This guide explains how to run and test **Zippypet** using the **Wokwi** simulator, either directly inside VS Code or through the web browser at [wokwi.com](https://wokwi.com).

---

## Method 1: Testing Inside VS Code (Recommended)

### 1. Install Wokwi Extension
1. In VS Code, go to the **Extensions** view (`Ctrl + Shift + X`).
2. Search for **Wokwi Simulator** and click **Install**.
3. Activate your license or start the free trial (Wokwi prompts you once on first use).

### 2. Configuration Files (Already Created)
* **`wokwi.toml`**: Points the simulator to your compiled firmware:
  ```toml
  [wokwi]
  version = 1
  firmware = '.pio/build/wokwi/firmware.merged.bin'
  elf = '.pio/build/wokwi/firmware.elf'
  ```
* **`diagram.json`**: Pre-configured with the **M5Stack CoreS3** board profile (ESP32-S3 with native color LCD screen, serial console, and interactive Red/Blue pushbuttons).

### 3. Build & Run
1. Build the firmware in PlatformIO:
   ```powershell
   python -m platformio run -e wokwi
   ```
2. Press `F1` in VS Code and type:
   ```
   Wokwi: Start Simulator
   ```
3. The simulation window will open with your Ceratosaurus dinosaur walking and animating live in the center of the display!

---

## Method 2: Testing Online at wokwi.com (Browser)

If you prefer testing without local VS Code extensions:

1. Open **[wokwi.com/projects/new/esp32-s3](https://wokwi.com/projects/new/esp32-s3)** or the **[M5Stack CoreS3 template](https://wokwi.com/projects/403861982905665537)**.
2. In the code editor tab, press `F1` and choose **"Upload Firmware Binary"**.
3. Select `.pio/build/wokwi/firmware.merged.bin` from your project folder.
4. Click the green **Play** button to run!

---

## Controls & Interaction in Simulation

| Action | Simulated Interaction | Behavior in Zippypet |
| :--- | :--- | :--- |
| **Btn A Click** | Click **Red Pushbutton** or press `'a'` in Serial Monitor | Poke dinosaur / Confirm menu action / Wake up |
| **Btn B Click** | Click **Blue Pushbutton** or press `'b'` in Serial Monitor | Open Menu / Cycle between Feed, Train, Clean, Stats |
| **Btn B Long Press** | Hold **Blue Pushbutton** > 1.5s or press `'h'` in Serial Monitor | Return to Idle / Dismiss menu |
| **Secret Combo** | Hold Blue + Click Red twice, or press `'s'` in Serial Monitor | Parent reward combo (Stars & +10 Discipline) |
| **Serial Console** | Serial Monitor (115200 baud) | Debug logs, hourly status ticks, and single-key controls (`a`, `b`, `h`, `s`) |
