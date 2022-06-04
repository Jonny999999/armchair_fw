Firmware for a homemade automated electric armchair.  
More details about this project: https://pfusch.zone/electric-armchair


# Installation
### Install esp-idf
For this project **ESP-IDF v4.4.1** is required (with other versions it might not compile)
```bash
#download esp-idf
yay -S esp-idf #alternatively clone the esp-idf repository from github
#run installation script in installed folder
/opt/esp-idf/install.sh
```
### Clone this repo
```
git clone git@github.com:Jonny999999/armchair_fw
```



# Compilation
### Set up environment
```bash
source /opt/esp-idf/export.sh
```
(run once in terminal)

### Compile
```bash
idf.py build
```

### Upload
- connect FTDI programmer to board (VCC to VCC; TX to RX; RX to TX)
- press REST and BOOT button
- release RESET button (keep pressing boot)
- run flash command:
```bash
idf.py flash
```
- once "connecting...' successfully, BOOT button can be released

### Monitor
- connect FTDI programmer to board (VCC to VCC; TX to RX; RX to TX)
- press REST and BOOT button
- release RESET button (keep pressing boot)
- run monitor command:
```bash
idf.py monitor
```
- once connected release BOOT button
- press RESET button once for restart



# Hardware setup
## pcb
Used pcb developed in this project: https://pfusch.zone/project-work-2020

## connection plan
A diagram which shows what components are connected to which terminals of the pcb exists here:  
[connection-plan.drawio.pdf](connection-plan.drawio.pdf)



# Planned Features
- More sensors:
  - Accelerometer
  - Lidar sensor
  - GPS receiver
- Anti slip regulation
- Self driving algorithm
- Lights



# Usage
## Switch functions
**previous functions - not implemented yet**
| Count | Action |
| --- | ---|
| 1 | define joystick center |
| 2 | toggle motors |
| 3 | toggle log-level (WARN, DEBUG, INFO) |
| 4 | define max duty |
| 5 | toggle mode MQTT/JOYSTICK |
| 6 | toggle mode SHAKE/JOYSTICK |
| 7 | toggle testing-mode (dry-run) |
