#include <msp430.h>
#include <stdint.h>

#include "console.h"
#include "timer.h"
#include "uart_hub.h"
#include "error.h"

#include "modem.h"
#include "dht22.h"
#include "gps.h"
#include "mqtt.h"
#include "sps30.h"

#include "settings.h"

// Overclocks to 12288000 Hz = (374 + 1) * 32768 Hz
void overclock_to_12mhz() {
    UCSCTL3 |= SELREF_2;

    __bis_SR_register(SCG0);

    UCSCTL0 = 0x0000;
    UCSCTL1 = DCORSEL_5;
    UCSCTL2 = FLLD_1 + 374;

    __bic_SR_register(SCG0);

    __delay_cycles(375000);
}

size_t pmcu_read_dht22(uint8_t *buffer) {
    uart_hub_select(0);

    if ((pmcu_error = dht22_read(buffer)) == PMCU_OK) {
        return 4;
    } else {
        PMCU_log("Error during DHT22 data reading");
        return 0;
    }
}

size_t pmcu_read_gps(uint8_t *buffer) {
    uart_hub_select(0);
    uart_setup(UART_A0, UART_BAUD_RATE_9600_SMCLK_12MHZ);
    uart_hub_select(2);

    if ((pmcu_error = gps_read_sentence("$GPGGA,", (char *) buffer)) == PMCU_OK) {
        return strlen((char *) buffer) + 1;
    } else {
        PMCU_log("Error during GY-GPSM6V2 data reading");
        return 0;
    }
}

size_t pmcu_read_modem_location(uint8_t *buffer) {
    uart_hub_select(0);
    uart_setup(UART_A0, UART_BAUD_RATE_9600_SMCLK_12MHZ);
    uart_hub_select(1);

    if ((pmcu_error = modem_get_location((char *) buffer)) == PMCU_OK) {
        return strlen((char *) buffer) + 1;
    } else {
        PMCU_log("Error during SIM800L location reading");
        return 0;
    }
}

size_t pmcu_read_sps30_data(uint8_t *buffer) {
    size_t payload_length;

    uart_hub_select(0);
    uart_setup(UART_A0, UART_BAUD_RATE_115200_SMCLK_12MHZ);
    uart_hub_select(3);

    if ((pmcu_error = sps30_read_measured_values(buffer, 64, &payload_length)) == PMCU_OK) {
        return payload_length;
    } else {
        PMCU_log("Error during SPS30 data reading");
        return 0;
    }
}

size_t pmcu_measure(uint8_t *buffer) {
    size_t pos, len;

    pos = 0;

    // dht22 measure
    len = pmcu_read_dht22(buffer);
    if (len) {
        pos += len;
    } else {
        return 0;
    }

    // gps measure
    len = pmcu_read_gps(&buffer[pos]);
    if (len) {
        pos += len;
    } else {
        return 0;
    }

    // modem measure
    len = pmcu_read_modem_location(&buffer[pos]);
    if (len) {
        pos += len;
    } else {
        return 0;
    }

    // sps30 measure
    len = pmcu_read_sps30_data(&buffer[pos]);
    if (len) {
        pos += len;
    } else {
        return 0;
    }

    return pos;
}

int main() {
    uint8_t buffer[256];
    size_t pos, pkt_sz, len;

    char pmcu_id[32];

    WDTCTL = WDTPW | WDTHOLD;

    __bis_SR_register(GIE);

    // ***************************************** HW init

    P1DIR |= BIT0;
    P1SEL &= ~BIT0;
    P1OUT |= BIT0;

    overclock_to_12mhz();

    uart_setup(UART_A1, UART_BAUD_RATE_9600_SMCLK_12MHZ); // logger init
    PMCU_log("+++ PMCU v1.0 +++");

    uart_hub_init();

    timer_init();

    // ***************************************** SPS30 init
    PMCU_log("Initializing SPS30...");

    uart_hub_select(0);
    uart_setup(UART_A0, UART_BAUD_RATE_115200_SMCLK_12MHZ);
    uart_hub_select(3);

    pmcu_error = sps30_start_measurement();
    if (pmcu_error != PMCU_OK && pmcu_error != SPS30_COMMAND_NOT_ALLOWED_IN_CURRENT_STATE) {
        PMCU_error_print("sps30", pmcu_error, NULL);
    }

    // ***************************************** Modem init
    PMCU_log("Initializing modem...");

    uart_hub_select(0);
    uart_setup(UART_A0, UART_BAUD_RATE_9600_SMCLK_12MHZ);
    uart_hub_select(1);

    PMCU_log("Syncing & resetting modem...");
    __pmcu_assert("modem", modem_sync());

    PMCU_log("Attaching GPRS service to modem...");
    __pmcu_assert("modem", modem_gprs_attach());

    // pmcu id
    PMCU_log("Creating PMCU id...");
    strcpy(pmcu_id, "pmcu/");
    __pmcu_assert("pmcu", modem_get_imei(&pmcu_id[5]));
    pmcu_id[20] = '\0';

    PMCU_log("PMCU id:");
    PMCU_log(pmcu_id);

    // When looping starts, glows the green led
    P4DIR |= BIT7;
    P4SEL &= ~BIT7;
    P4OUT |= BIT7;
    PMCU_log("-------------------------------- LOOPING");


    while (1) {
        // ***************************************** Measure & pack
        PMCU_log("Measuring & packing");

        // When starts measuring turns on the blue led.
        P2DIR |= BIT7;
        P2SEL &= ~BIT7;
        P2OUT |= BIT7;

        pos = 4;

        pos += mqtt_pack_string(&buffer[pos], pmcu_id);

        // tries to measure, if any error occurs, repeats
        len = pmcu_measure(&buffer[pos]);
        if (len) {
            pos += len;
        } else {
            PMCU_log("Repeating the measurement cycle since an error occurred");
            continue;
        }

        pkt_sz = pos;
        mqtt_pack_fixed_header(buffer, 0b00110000, pkt_sz - 4);

        uart_hub_select(0); // selects back modem
        uart_setup(UART_A0, UART_BAUD_RATE_9600_SMCLK_12MHZ);
        uart_hub_select(1);

        // ***************************************** MQTT connect

        PMCU_log("Connecting to broker at "PMCU_SETTINGS_BROKER_ADDR":"PMCU_SETTINGS_BROKER_PORT);

        pos = pkt_sz;
        pkt_sz = mqtt_create_connect_packet(&buffer[pos], pmcu_id, NULL, NULL);
        if (modem_tcp_connect(PMCU_SETTINGS_BROKER_ADDR, PMCU_SETTINGS_BROKER_PORT) != PMCU_OK) {
            PMCU_log(">> An error occured during TCP broker connection");
            continue;
        }

        if (mqtt_connect(&buffer[pos], pkt_sz) != PMCU_OK) {
            PMCU_log(">> An error occured during MQTT CONNECT packet");
            continue;
        }

        // ***************************************** MQTT publish
        PMCU_log("Publishing");

        pkt_sz = pos;
        if (modem_tcp_send(buffer, pkt_sz) != PMCU_OK) {
            PMCU_log(">> An error occured during MQTT PUBLISH packet");
            continue;
        }

        // ***************************************** MQTT disconnect
        PMCU_log("Disconnecting");

        pkt_sz = mqtt_create_disconnect_packet(buffer);
        if (mqtt_disconnect(buffer, pkt_sz)!= PMCU_OK) {
            PMCU_log(">> An error occured during MQTT DISCONNECT packet");
            continue;
        }

        if (modem_tcp_disconnect() != PMCU_OK) {
            PMCU_log(">> An error occured during TCP broker disconnect");
            continue;
        }

        // When the whole thing has finished, turns off the blue led.
        P2DIR |= BIT7;
        P2SEL &= ~BIT7;
        P2OUT &= ~BIT7;

        // ************** pause of 5 seconds
        __delay_cycles(12288000 * 5);
    }
}
