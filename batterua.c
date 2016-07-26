#include <xc.h>
#include "pic_config.h"
#define _XTAL_FREQ 16000000
#define LCD_DEFAULT
#include "LCD_44780.h" 
#include "LCD_44780.c"
#include "delay.h"
#include "delay.c"
#include <stdio.h>
#include <math.h>

#define BatteryCharger LATBbits.LATB7
#define Load LATEbits.LATE0
#define R1 67050 //INSERIRE VALORE Ohm PARTITORE
#define R2 33060 //INSERIRE VALORE Ohm PARTITORE

volatile int lettura [3] = 0;
volatile unsigned int ore, minuti, secondi, somme = 0;
volatile unsigned long tempo, tempo_old = 0;
volatile unsigned char str [8] = 0;
volatile unsigned char stati = 0;
volatile float rapporto, current, voltage, sommatoriaCorrente, correnteMedia, capacita = 0;

void inizializzazione(void);
void read_adc(void);
void display_voltage(unsigned char line);


unsigned char combinazioni[] = {
    0b00000001, //AN0
    0b00000101, //AN1
    0b00001001, //AN2
    0b00001101 //AN2
};

__interrupt(high_priority) void isr_alta(void) { //incremento ogni secondo
    INTCONbits.TMR0IF = 0;
    TMR0H = 0x0B;
    TMR0L = 0xDC;
    tempo++;
    secondi++;
    if (secondi == 60) {
        secondi = 0;
        minuti++;
        if (minuti == 60) {
            minuti = 0;
            ore++;
        }
    }
}

__interrupt(low_priority) void isr_bassa(void) {
    if (PIR1bits.TMR1IF == 1) {
        //read_adc();
        TMR1H = 0x3C;
        TMR1L = 0xB0;
        PIR1bits.TMR1IF = 0;
        T1CON = 0x31;
    }
}

void main(void) {
    delay_set_quartz(16);
    rapporto = (R1 + R2);
    rapporto = R2 / rapporto;
    inizializzazione();
    stati = 0;
    while (1) {
        read_adc();

        if (stati == 0) {
            while ((current < -0.5) || (voltage < 14)) {
                BatteryCharger = 1; //attivo ciclo ricarica
                LCD_goto_line(1);
                LCD_write_message("Ciclo ricarica..");
                display_voltage(2);
                delay_ms(500);
            }
            stati = 1;
        }

        if (stati == 1) {
            if ((current > -0.5)&&(voltage > 14.2)) {
                LCD_write_message("Carica terminata");
                BatteryCharger = 0; //attivo ciclo ricarica
                delay_ms(5000);
            }
            stati = 2;
        }

        if (stati == 2) {
            while (voltage > 13.0) {
                LCD_goto_line(1);
                LCD_write_message("     Attesa     ");
                LCD_goto_line(2);
                LCD_write_message("Stabilizzazione.");
                delay_s(2);
                display_voltage(2);
                delay_s(2);
            }
            stati = 3;
        }
        if (stati == 3) {
            tempo = 0;
            secondi = 0;
            minuti = 0;
            ore = 0;
            T0CON = 0x85;
            TMR0H = 0x0B;
            TMR0L = 0xDC;
            Load = 1;
            somme = 0;
            while (voltage > 10) {
                LCD_home();
                LCD_write_message("tempo:");
                LCD_write_integer(ore, 2, ZERO_CLEANING_OFF);
                LCD_write_message(":");
                LCD_write_integer(minuti, 2, ZERO_CLEANING_OFF);
                LCD_write_message(":");
                LCD_write_integer(secondi, 2, ZERO_CLEANING_OFF);
                display_voltage(2);
                delay_ms(100);
                if (tempo - tempo_old >= 59) {
                    tempo_old = tempo;
                    somme++;
                    sommatoriaCorrente = current + sommatoriaCorrente;
                }
            }
            stati = 4;
        }
        if (stati == 4){
            correnteMedia = sommatoriaCorrente/somme;
            capacita = (correnteMedia*(ore+(minuti/60)+(secondi/3600)));
            LCD_home();
            LCD_write_message(" test terminato ");
            LCD_goto_line(2);
            LCD_write_message("Capacita':");
            sprintf(str,"%.3f", capacita);
            str[5] = '\0';
            LCD_write_string(str);
            LCD_write_message("Ah");
            while(1);
        }
    }
}

void display_voltage(unsigned char line) {
    read_adc();
    LCD_goto_line(line);
    sprintf(str, "V:%.3f", voltage); //convert float to char
    str[7] = '\0'; //add null character
    LCD_write_string(str); //write Voltage in LCD
    sprintf(str, " I:%.3f", current); //convert float to char
    str[7] = '\0'; //add null character
    LCD_write_string(str); //write Current in LCD
    LCD_write_message("   "); //verificare questi spazi
}

void read_adc(void) {
    for (char i = 0; i < 4; i++) {
        ADCON0 = combinazioni[i]; //disattivo conversione, imposto il canale interessato
        ADCON0bits.GO = 1; //inzio conversione
        while (ADCON0bits.GODONE == 1); //attendo fine conversione
        lettura [i] = ADRESH; //salvo il dato
        lettura [i] = ((lettura[i] << 8) | ADRESL); //salvo il dato
        delay_ms(5); //attesa random
    }
    current = (lettura[2] - lettura[1]);
    current = (current * 5);
    current = current / 1024;
    current = current / 0.200;
    voltage = (lettura[0]);
    voltage = (voltage * 5) / 1024;
    voltage = voltage / rapporto; //Conversione in tensione reale
}

void inizializzazione(void) {
    LATA = 0x00;
    TRISA = 0xFF; //PORTA all input

    LATB = 0x00;
    TRISB = 0b01111111; //PORTB ALL OUTPUTS

    LATC = 0x00;
    TRISC = 0x00;

    LATD = 0x00;
    TRISD = 0x00;

    LATE = 0x00;
    TRISE = 0b00000110;

    LCD_initialize(16);
    LCD_write_message("TESTER BATTERIE");
    delay_ms(500);
    LCD_backlight(LCD_TURN_ON_LED);
    //LCD_clear();
    ADCON0 = 0b00000000; //DISABILITO TUTTO
    ADCON1 = 0b00001011;
    ADCON2 = 0b10110101;
    ADCON0bits.CHS3 = 0; //IMPOSTAZIONE DI SICUREZZA
    ADCON0bits.CHS2 = 0; //IMPOSTAZIONE DI SICUREZZA
    ADCON0bits.CHS1 = 0; //IMPOSTAZIONE DI SICUREZZA

    T0CON = 0x85;
    TMR0H = 0x0B;
    TMR0L = 0xDC;

    T1CON = 0x31;
    TMR1H = 0x3C;
    TMR1L = 0xB0;

    PIR1bits.TMR1IF = 0;
    PIE1bits.TMR1IE = 1;
    IPR1bits.TMR1IP = 0;

    RCONbits.IPEN = 1;
    INTCONbits.GIEH = 1;
    INTCONbits.GIEL = 1;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;
    ADCON0bits.ADON = 1; //attivo ADC
}