# BIRD FEEDER: Biometric Identification and Research-based Distribution Framework for Environmental Enrichment and Data-Enabled Ecological Research

## Overview
BIRD FEEDER, a collaborative effort between Embry-Riddle Aeronautical University, Prescott Campus (ERAU-PC), and the University of Georgia (UGA), epitomizes a multidisciplinary, multi-university initiative. This project, partially supported by a grant from the National Science Foundation (NSF), aims to create a bird feeder that is a cost-effective and solar-powered while engineered to detect, weigh, and selectively grant or deny access to food for specific bird species whose migratory patterns we wish to study. It merges technology with wildlife conservation, leveraging the diverse expertise of ERAU-PC’s engineering departments and UGA’s Warnell School of Forestry and Natural Resources.

## Introduction
This guide will take you through the steps of creating your very own Bird Feeder :)

## Installing Arduino
For this manual, we will be walking through the steps on how to use the Arduino IDE to write and upload code to the microcontroller powering your bird feeder. A future manual may be created to utilize the Arduino Cloud.

1.	Visit the official [Arduino website](https://www.arduino.cc)
2.	Click on the Software section at the top of the site.
3.	Under downloads, select the appropriate download option for your system.

**Windows:**
1.	Open the downloaded file to start the installation.
2.	You may be prompted with a security warning. If so, choose to run the installer.
3.	Follow the installation prompts. It’s usually safe to proceed with the default settings, but you can customize the installation location and components if needed.
4.	During the installation, you might be asked to install device drivers, accept, and install these drivers for Arduino boards to work properly with your computer.
5.	After installation, launch the Arduino IDE from your Start menu or desktop shortcut.

**Mac:**
1.	Open the downloaded .zip file to extract the Arduino application.
2.	Drag the Arduino application into your Applications folder.
3.	You might need to authorize the application due to macOS security preferences. You can do this in System Preferences > Security & Privacy.
4.	Open your Applications folder and double-click on the Arduino application to launch it.

**Linux:**
1.	Once downloaded, extract the tar archive to a location of your choice.
2.	Open a terminal and navigate to the extracted folder.
3.	Run the install.sh script to install the Arduino IDE. This can be done by executing ./install.sh from the terminal within the folder.
4.	You can start the Arduino IDE from your application menu or use the terminal to navigate to the installation directory and run ./Arduino.

## Installing Libraries
Installing libraries is an important step in ensuring that code runs as it should. Installing the wrong library may cause the code to behave strangely or not work at all.
**Libraries supported by Arduino:**
1.	With the Arduino IDE open, navigate to Tools > Manage Libraries. A menu should open from the left side of the window.
2.	In the Library Manager search bar, enter the name of the library and click install. This should install the latest version of the library.
3.	If it asks to Install Library Dependencies, select Install All. Libraries in the list below may appear as if they have already been installed, ignore them and continue installing the rest.

**For libraries unsupported by Arduino:**
1.	Download the library .zip file onto your computer.
2.	With the Arduino IDE open, navigate to Sketch > Include Library > Add .Zip Library…
3.	Find the library .zip file that you downloaded, select it, and select Open.

**Libraries to Install**

| Library Name | Author | Supported by Arduino?|
|--------------|--------|----------------------|
|SDFat - Adafruit Fork|Adafruit|Yes|
|Adafruit GFX Library| Adafruit| Yes|
|Adafruit AHTX0| Adafruit| Yes|
|Adafruit NAU7802 Library| Adafruit| Yes|
|RTClib| Adafruit| Yes|
|Boulder Flight Systems Circular Buffer| Brian Taylor| Yes|
|PCA9536D| Gavin Hurlbut| Yes|
|Adafruit INA219| Adafruit| Yes|
|NTPClient| Fabrice Weinberg| Yes|

## Ordering Parts
The code is developed to use these specific electronics. Ordering other electronics may result in unexpected consequences. Before ordering different parts, please consult with the Embry-Riddle Professors or modify the code to accommodate the changes.

|Part Name|Link|# of Parts Ordered per Unit|
|---------|----|---------------------------:|
|Mounting Grid|https://www.adafruit.com/product/5779|1|
|RFID Reader Kit| https://handsontec.com/index.php/product/hz-1050-wiegand-125khz-rfid-reader-kit/|1|
|Load Cell| https://www.sparkfun.com/products/14727|1|
|Load Cell ADC|https://www.adafruit.com/product/4538 |1|
|Mounting Screws & Standoff Set*|https://www.adafruit.com/product/3299|1|
|STEMMA QT Cables|https://www.adafruit.com/product/4210|2|
|ESP32-S3 Feather|https://www.adafruit.com/product/5477|1|
|SD Add-on|https://www.adafruit.com/product/2922|1|
|Coin Cell Battery|https://www.adafruit.com/product/380|1|
|Stacking Headers|https://www.adafruit.com/product/2830|1|
|Female Headers|https://www.adafruit.com/product/2886|1|
|Temperature + Humidity Sensor|https://www.adafruit.com/product/4566|1|
|MicroSD Card|https://www.amazon.com/MicroSD-Memory-Class10-SDAdapter-Reader/dp/B07R3QRGGF|1|
|Jumper Wires*|[https://a.co/d/4cGIVyl](https://a.co/d/iKBbOFw)|1|

\* Only one order is needed to build multiple units

## Configuring the Arduino IDE
Now that we have the Arduino IDE and our ESP32 on hand installed we have to configure the options so that the code can be uploaded to our microcontroller.

1.	Connect the ESP32-S3 Feather 2MB PSRAM microprocessor to a computer via a USB-C cable.
2.	With the Arduino IDE open, navigate to Tools > Board: “There may be a board name here” > Boards Manager.
3.	In the Boards Manager search bar, enter the board name “esp32” and install the library labeled “esp32” by Espressif Systems.
4.	Select the board by navigating to Tools > Board: “There may be a board name here” > esp32 > Adafruit Feather ESP32-S3 2MB PSRAM board.
5.	Select the port you are connecting the microprocessor to the computer with by navigating to Tools > Port: “COM…” and selecting the COM port that is connected to the board. Typically, the “COM1” is not connected to the board on Windows OS.

Now we can begin uploading the code to the microprocessor.

## Uploading and Running Code
Please install libraries and configure the board before doing this step.
1.  Connect the ESP32-S3 Feather 2MB PSRAM microprocessor to a computer via a USB-C cable.
2.	Download the rtcLoggingWifi.ino file found in the rtcLoggingWifi folder found in the Austin Branch of this GitHub. The location of the most up to date .ino file will change in the future.
3.	Open Arduino IDE and open the sketch by navigating through File > Open and selecting the .ino file you just downloaded.
5.	To ensure that the code is working, select Sketch > Verify/Compile.
7.	To upload any code, the ESP32 has to be put into upload mode. While holding the Boot button (between the header and outgoing wire, labeled “Boot”), press and release the Reset button (labeled “Reset”). You can now release the Boot button.
8.	If the code compiled successfully, upload the code to the microprocessor by selecting Sketch > Upload.
9.	To run the program that was uploaded to the microcontroller, press the Reset button.
   
Since we haven't connected any of the other components this will likely fail or do nothing. Steps 7-9 are still important though because after installing all the electronics, it will be difficult to access the USB-C port on the microcontroller.

## Wiring the Electronics
The order in which you connect the electronics should not matter. For the STEMMA QT Cable ensure that it is properly oriented by checking that the holes at the end of the cable match the pins in the ports.
1.	Connect a STEMMA QT Cable to the microcontroller and the Load Cell ADC. The connection point can be found near the center of the board. The connection point for the Load Cell ADC is found on either end of the board.
3.	Insert 3 wires into the headers (The black piece of plastic on top of the board with a series of holes) found on the microcontroller. The wires go into the GND, USB, and RX pins of the header.
4.	On the RFID Reader board, connect the following wires to the following pins: GND wire to GND pin, USB wire to 5V pin, and RX wire to TX0 pin.

|![Wiring Diagram](https://github.com/semcneil/birdfeeder/blob/main/docs/Bird%20Feeder%20Diagram.jpg)|
|:-:|
| *Basic Diagram of the wiring layout for the base components in the BIRD FEEDER* |
