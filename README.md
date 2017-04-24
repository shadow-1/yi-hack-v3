# yi-hack-v3

This project is a collection of config files, Makefiles and scripts to allow custom firmware images to be created and deployed to extend the functionality of Xiaomi Cameras based on HiSilicon Hi3518e V200 chipset.

Currently this project supports:
* Yi 1080p Dome
* Yi 1080p Home
* Yi Dome

![Alt text](yi-cam.png?raw=true "Supported Yi Cameras")

## Table of Contents

- [Features](#features)
- [Cameras that are Region Locked to Mainland China](#cameras-that-are-region-locked-to-mainland-china)
- [Getting Started - Step by Step Guide](#getting-started---step-by-step-guide)
- [Which Smartphone App to use?](#which-smartphone-app-to-use)
    - [Chinese Version of the Camera](#chinese-version-of-the-camera)
    - [International Version of the Camera](#international-version-of-the-camera)
- [Using the Camera](#using-the-camera)
    - [Telnet Server](#telnet-server)
    - [FTP Server](#ftp-server)
    - [Startup Shell Script](#startup-shell-script)
    - [Which Version is Installed?](#which-version-is-installed)
- [Going Back to Stock Firmware](#going-back-to-stock-firmware)
- [Development](#development)
- [Acknowledgments](#acknowledgments)

## Features
The supported cameras have the following features by default:
* Wifi
* Motion detection - a video file is generated if a motion have been detected in the last 60 seconds.
* Send video/audio data through a cloud service to allow people to view camera data from their smartphone wherever they are.
* Setup thanks to a smartphone app.
* Local video storage on a microSD card.

This firmware includes:
* Telnet server - _Enabled by default._
* FTP server - _Enabled by default._
* Web server - _Enabled by default._
* Proxychains-ng - _Enabled by default. Useful if the camera is region locked._

## Cameras that are Region Locked to Mainland China
This firmware includes Proxychains-ng. This allows communication between the camera and Xiaomi server to be routed through a proxy server. If Proxychains-ng is configured with a proxy server from Mainland China, the camera will no longer be region locked.

Performance is not degraded as the cameras video/audio feed is not routed through the proxy server.

## Getting Started - Step by Step Guide
1. Check that you have a correct Xiaomi Yi camera. Currently 3 cameras are supported: 
* Yi 1080p Dome Camera
* Yi 1080p Home Camera
* Yi Dome Camera

2. Get an microSD card, preferably of capacity 16gb or less and format it by selecting File System as FAT32.

**_Note: The microSD card must be formatted in FAT32. exFAT formatted microSD cards will not work._**

3. Get the correct firmware files for your camera from this link: https://github.com/shadow-1/yi-hack-v3/releases

| Camera | rootfs partition | home partition | Remarks |
| --- | --- | --- | --- |
| **Yi 1080p Dome** | rootfs_h20 | home_h20 | Firmware files required for the Yi 1080p Dome camera. |
| **Yi 1080p Home** | rootfs_y20 | home_y20 | Firmware files required for the Yi 1080p Home camera. |
| **Yi Dome** | rootfs_v201 | home_v201 | Firmware files required for the Yi Dome camera. |

4. Save both files on root path of microSD card.

**_IMPORTANT: Make sure that the filename stored on microSD card are correct and didn't get changed. e.g. The firmware filenames for the Yi 1080p Dome camera must be home_h20 and rootfs_h20._**

5. Remove power to the camera, insert the microSD card, turn the power back ON. 

6. The yellow light will come ON and flash for roughly 30 seconds, which means the firmware is being flashed successfully. The camera will boot up.

7. Install the _correct_ smartphone app onto your smartphone. Refer to [Which Smartphone App to use?](#which-smartphone-app-to-use) for guidance.

8. Configure the camera as normal by scanning the QR code on the smartphone. Ensure that your smartphone is connected to 2.4GHz wireless network.

9. Blue light should come ON indicating that your Wifi connection has been successful.

10. Although the WiFi connection on your camera has been successful but you won't be able to pair it with your phone yet until you perform the following steps.

11. Find the IP address has been assigned to your camera. This can be found on most routers. Alternatively you can install an app on your phone to scan your wifi network. Android users can install "Network Scanner" and run it to find the IP address of the camera. e.g. 192.168.1.5. The camera should be listed as “Shenzhen Zowee Technology Co. Ltd”.

12. Go in the browser and open it as a website e.g. http://192.168.1.5

13. It will open a configuration page of the camera.
![Alt text](web_interface.png?raw=true "Web Interface")

14. For those with the **International Version of the Camera**. ProxyChains-ng is not required. It can be disabled by going to "System Config", select "No" against Proxy-Chains-ng and clicking "Apply". No further configuration is required.

15. For those with the **Chinese Version of the Camera**. Enter socks4 or socks5 proxy servers under [ProxyList] in the configuration. The proxy servers listed below are an example only. These might or might not be working. Find a working socks4 or socks5 proxy server and list it under [ProxyList] in the given syntax.
```
socks5 183.232.25.100 3080
socks5 27.152.181.217 80
socks5 125.67.236.195 8080
socks4 115.29.192.194 1080
```

16. Click "Save"

17. Give it about 30-40 seconds and try to connect from your smartphone.

18. If it is not connecting, most likely reason is that the proxy entered is not working.

19. If it gives you the alert "This camera can only be used in China", this means that the proxy that you entered in not based in Mainland China.

## Which Smartphone App to use?
### Chinese Version of the Camera
The Chinese version of the camera will **only** work with the Chinese version of the app.

Android users, download the Chinese version of the Yi Home app and install the apk file manually. You can download the app from here under the directory 'Yi Home - Android': https://app.box.com/s/cibs7n1mgvhqaqjlidtveegu1uajt5yr

iPhone users will need to change their App Store to the Chinese App Store and install Chinese version of the Yi Home app.

### International Version of the Camera
The International version of the camera will **only** work with the International version of the app.

Android users can download the International version of the Yi Home app from the Google Play Store. Link: https://play.google.com/store/apps/details?id=com.ants360.yicamera.international

iPhone users can download the International version of the Yi Home app from the App Store. Link: https://itunes.apple.com/au/app/yi-home/id1011626777?mt=8

## Using the Camera

### Telnet Server
The telnet server is on port 23.

Default user is root. Password is _blank_.

### FTP Server
The FTP server is on port 21.

Default user is root. Password is _blank_.

### Startup Shell Script
On the microSD card. The following shell script is executed after the camera has booted up:

`startup.sh`

### Which Version is Installed?
The base firmware (Xiaomi firmware) version and y-hack-v3 firmware version is accessible through the webinterface on the About page.

## Going Back to Stock Firmware
Recovery images have been created to go back to stock firmware. You can download the recovery images from here under the directory 'yi-hack-v3/Recovery': https://app.box.com/s/cibs7n1mgvhqaqjlidtveegu1uajt5yr

| Camera | rootfs partition | home partition | Remarks |
| --- | --- | --- | --- |
| **Yi 1080p Dome** | rootfs_h20 | home_h20 | Stock firmware is version 1.9.2.0C_201611011902. |
| **Yi 1080p Home** | rootfs_y20 | home_y20 | Stock firmware is version 2.0.0.1A_201612051401. |
| **Yi Dome** | rootfs_v201 | home_v201 | Stock firmware is version 1.9.1.0F_201701041701. |

## Development
**TODO**

## Acknowledgments

Special thanks to the following projects for their efforts on other Xiaomi cameras and giving inspiration for me to develop and publish my own custom firmware.

**fritz-smh** : https://github.com/fritz-smh/yi-hack

**niclet** : https://github.com/niclet/yi-hack-v2

**xmflsct** : https://github.com/xmflsct/yi-hack-1080p
