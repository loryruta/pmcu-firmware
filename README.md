# PM Control Unit firmware
The structure of the PM Control Unit (PMCU) firmware is articulated in many libraries and each of them is responsible for making a single sensor or task working. The chosen programming language is C and the arch used is MSP430F5529. In this document, we explain the main tasks of the firmware and the format of the MQTT payload sent.

* [Initialization](#initialization)
  * [First phase](#first-phase)
  * [Second phase](#second-phase)
* [Measuring loop](#measuring-loop)
  * [Measurement](#measure)
  * [Publish](#publish)
  * [Wait](#wait)
* [MQTT payload format](#mqtt-payload-format)

# Initialization

![init](https://drive.google.com/uc?export=view&id=1zpZdeSEkrxE3jHeRc7hVItVa8SP7DsID)

## Details
To represent the initialization clearly, we will divide it in two phases:
* [First phase](#first-phase)
* [Second phase](#second-phase)

### First phase
In first place, we initialize the hardware connections and utilities:
* Disable watchdog: we won't check whether the PMCU halts.
* Enable all system interrupts.
* Overclock MCLK (and SMCLK) to 12MHz, needed to communicate at 115200 baud rate with SPS30.
* Set up USCI_A1 for logging.
* Set up UART hub pins (4.1, SEL_A, and 4.0, SEL_B).
* Start the global timer, that will run every second.

After this phase, we glow a **red led every second**.

### Second phase
Now we bring SPS30 and SIM800L to a known state:
* Select the SPS30 and set it to measurement mode.
* Sync with SIM800L and wait until the AT command answers with OK.
* Reset SIM800L's config and disable commands echoes.
* Attach GPRS service.
* Retrieve SIM's IMEI and build PMCU_ID.

After this phase, we glow a **green led fixed**.

# Measuring loop

## Overview
![loop](https://drive.google.com/uc?export=view&id=12l-Mv-JJYRfgyS8B5XdCHHOmdOHNhQoY)

## Details
The measuring loop is entered soon after the initialization. It can be separate in three steps:
* [Measurement](#measurement)
* [Publish](#publish)
* [Wait](#wait)

### Measurement
During this phase we contact every sensor and ask for measurements:
* Select nothing on UART hub, set up 1.2 pin, read DHT22 temperature and humidity and put data on a buffer.
* Select GY-GPSM6V2 and read location data. Append data on the same buffer.
* Select SIM800L and read location data, append data on the same buffer.
* Select SPS30 and read PM data. Append data on the same buffer.

### Publish
We decided to create a new TCP connection every loop to avoid undefined behavior when the UART interface isn't listening to the modem. For example, an issue would be that the connection is lost while the modem is listening to SPS30 data. What we do is:

* Connect via TCP to broker.
* Send an MQTT CONNECT packet, with no login info at the moment.
* Send an MQTT PUBLISH packet containing the previous buffer, filled during measurement, the resulting payload format is described [here](#mqtt-payload-format). The target topic will be `pmcu/<PMCU_ID>`.
* Send an MQTT DISCONNECT packet.
* Close TCP connection.

### Wait
Wait 5 seconds before repeating again.

# MQTT payload format
The raw packet published to the broker isn't treated in any way by the MCU to increase performance. Its structure is:

| Content | Size | Format |
| --- | --- | --- |
| RH | 2 bytes | MSB integer |
| Temperature | 2 bytes | MSB integer |
| GPS $GPGGA sentence | ? bytes | string (ends with \0) | 
| GSM location sentence | ? bytes | string (ends with \0) |
| PM data | 40 bytes | 10x IEEE 754 float (4 bytes each) |

Data splitting and parsing must be done by the MQTT client subscribed to the PMCU topic. A working Python parser can be found [here](https://gitlab.com/pmcontrolunit/webserver/snippets/1851169).

