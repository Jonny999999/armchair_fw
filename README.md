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
### Instal node packages
For the react app packages have to be installed with npm TODO: add this to cmake?
```
cd react-app
npm install
```



# Compilation
## react-webapp
For the webapp to work on the esp32 it has to be built.
When flashing, the folder react-app/build is flashed to siffs (which is used as webroot) onto the esp32.
The following command builds the react webapp and creates this folder
TODO: add this to flash target with cmake?
```bash
cd react-app
#compile
npm run build
#remove unwanted license file (filename too long for spiffs)
rm build/static/js/main.8f9aec76.js.LICENSE.txt
```
Note: Use `npm start` for starting the webapp locally for testing

## esp project
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
- drinks holder
- improved webinterface



# Todo
**Add switch functions**
- set loglevel
- define max-speed
- calibrate joystick (min, max, center)
- testing mode / dry-run



# Usage
## Switch functions
**Currently implemented**
| Count | Type | Action | Description |
| --- | --- | --- | --- |
| 1x | configure | [JOYSTICK] **calibrate stick** | when in joystick mode: set joystick center to current joystick pos |
| 1x | control | [MASSAGE] **freeze** input | when in massage mode: lock or unlock joystick input at current position |
| 2x | toggle mode | **IDLE** <=> previous | enable/disable chair armchair e.g. enable after startup or timeout |
| 3x | switch mode | **JOYSTICK** | switch to default mode JOYSTICK |
| 4x | toggle mode | **HTTP** <=> JOYSTICK | switch to '**remote control** via web-app `http://191.168.4.1`' or back to JOYSTICK mode |
| 5x | | | |
| 6x | toggle mode | **MASSAGE** <=> JOYSTICK | switch to MASSAGE mode or back to JOYSTICK mode |
| 7x | | | |
| 8x | toggle option | **deceleration limit** | disable/enable deceleration limit (default on) => more responsive |
| | | | |
| 12x | toggle option | **alt stick mapping** | toggle between default and alternative stick mapping (reverse swapped) |
| >1s | system | **restart** | Restart the controller when pressing the button longer than 1 second | 
| 1x short, 1x long | auto command | **eject** foot support | automatically go forward and reverse for certain time with no acceleration limits, so foot support ejects |


## HTTP mode
Control armchair via virtual joystick on a webinterface.  

**Usage**
- Connect to wifi `armchar`, no password
- Access http://192.168.4.1  (note: **http** NOT https, some browsers automatically add https!)  

**Current Features**
- Control direction and speed with joystick  

**Todo**
- Set parameters
- Control other modes
- Execute preset movement commands
