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

#define fan LATCbits.LATC2
#define rapporto 0.3302367395864549 // R2/(R1+R2)
#define load LATEbits.LATE0
#define batteryCharger LATBbits.LATB7
#define led_green LATCbits.LATC0
#define led_red LATCbits.LATC1 
#define R0 10000 //valore NTC a Tnom
#define Tnom 25
#define  B 4300//coefficente beta

volatile bit inizio = 0;
volatile int lettura [4] = 0;
volatile unsigned int ore, minuti, secondi, battery = 0;
volatile unsigned long tempo, somme, tempo_old = 0;
volatile unsigned char str [8] = 0;
volatile unsigned char stati = 0;
volatile float current, voltage, sommatoriaCorrente, temperature = 0;

void inizializzazione(void);
void read_adc(void);
void display_voltage(unsigned char line);
void ricarica(void);
void stabilizzazione(void);
void scarica(void);

unsigned char combinazioni[] = {
    0b00000001, //AN0
    0b00000101, //AN1
    0b00001001, //AN2
    0b00001101 //AN2
};

__interrupt(high_priority) void isr_alta(void) { //incremento ogni secondo
    if (INTCONbits.TMR0IF == 1) {
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
        INTCONbits.TMR0IF = 0;
    }

    if (INTCONbits.INT0IF == 1) {
        if (PORTBbits.RB2 == 1) {
            battery += 5;
        }
        if (PORTBbits.RB2 == 0) {
            if (battery>4){
            battery -= 5;
            
        }
            else{
                battery = 0;
            }
        }
        INTCONbits.INT0IF = 0;
    }
}

__interrupt(low_priority) void isr_bassa(void) {
//    if (PIR1bits.TMR1IF == 1) {
//        //read_adc();
//        TMR1H = 0x3C;
//        TMR1L = 0xB0;
//        PIR1bits.TMR1IF = 0;
//        T1CON = 0x31;
//    }
}

void main(void) {

    //Funzione di inizializzazione periferiche e I/O
    inizializzazione();
    read_adc();
    LCD_write_message("selezionare capacità");
    LCD_goto_line(2);
    LCD_write_message("batteria:");
    while (1) {
        while (inizio != 1) {
            LCD_goto_xy(10, 2);
            LCD_write_integer(battery, 3, ZERO_CLEANING_OFF);
            LCD_write_message("Ah");
            delay_ms(10);
            if (PORTBbits.RB1 == 0) {
                inizio = 1;
                INTCONbits.INT0IE = 0;
            }
        }

        ricarica();
        stabilizzazione();
        scarica();

        if (stati == 4) {
            LCD_initialize(16);
            load = 0;
            if (somme != 0) {
                sommatoriaCorrente = sommatoriaCorrente / somme;
            }
            sommatoriaCorrente = sommatoriaCorrente * (ore + ((float) minuti / 60)+((float) secondi / 3600));
            LCD_home();
            LCD_write_message("test completato:");
            LCD_goto_line(2);
            LCD_write_message("capacita':");
            sprintf(str, "%.3f", sommatoriaCorrente);
            str[5] = '\0';
            LCD_write_string(str);
            while (1);
        }
    }
}

void ricarica(void) {
    LCD_initialize(16);
    while ((current < -0.5) || (voltage < 14)) {
        batteryCharger = 1; //attivo ciclo ricarica
        LCD_goto_line(1);
        LCD_write_message("Carica in corso:");
        display_voltage(2);
        delay_s(1);
        read_adc();
    }
    batteryCharger = 0;
    LCD_clear();
    LCD_write_message("Carica terminata");
    stati = 1;
    delay_s(5);
}

void stabilizzazione(void) {
    LCD_initialize(16);
    if (stati == 1) {
        LCD_initialize(16);
        while (voltage > 13.2) {
            LCD_goto_line(1);
            LCD_write_message("     Attesa     ");
            LCD_goto_line(2);
            LCD_write_message("Stabilizzazione.");
            delay_s(3);
            LCD_goto_line(1);
            LCD_write_message("  Informazioni  ");
            display_voltage(2);
            delay_s(2);
            load = 1;
            delay_ms(10);
            load = 0;
        }
        stati = 2;
    }
}

void scarica(void) {
    LCD_initialize(16);
    if (stati == 2) {
        tempo = 0;
        secondi = 0;
        minuti = 0;
        ore = 0;
        T0CON = 0x85;
        TMR0H = 0x0B;
        TMR0L = 0xDC;
        load = 1;
        somme = 0;
        LCD_initialize(16);
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
}

void display_voltage(unsigned char line) {
    read_adc();
    LCD_goto_line(line);
    LCD_goto_xy(1,line);
    sprintf(str, "V:%.2f", voltage); //convert float to char
    str[7] = '\0'; //add null character
    LCD_write_string(str); //write Voltage in LCD
    sprintf(str, " I:%.3f", current); //convert float to char
    str[7] = '\0'; //add null character
    LCD_write_string(str); //write Current in LCD
    sprintf(str, "T:%.1f", temperature);
    str[4] = '\0';
    LCD_write_string(str);

}

void read_adc(void) {
    for (char i = 0; i < 4; i++) {
        ADCON0 = combinazioni[i]; //disattivo conversione, imposto il canale interessato
        ADCON0bits.GO = 1; //inzio conversione
        while (ADCON0bits.GODONE == 1); //attendo fine conversione
        lettura [i] = ADRESH; //salvo il dato
        lettura [i] = ((lettura[i] << 8) | ADRESL); //salvo il dato
        delay_ms(1); //attesa random
    }
    current = (lettura[2] - lettura[1]);
    current = (current * 5);
    current = current / 1024;
    current = current / 0.200;
    current = 3.0;
    voltage = (lettura[0]);
    voltage = (voltage * 5) / 1024;
    voltage = (float) voltage / rapporto; //Conversione in tensione reale
    temperature = lettura[3];
    temperature = R0 / ((1023 / temperature) - 1);
    temperature = temperature / R0; // (R/Ro)
    temperature = log(temperature); // ln(R/Ro)
    temperature = temperature / B; // 1/B * ln(R/Ro)
    temperature = temperature + 1.0 / (Tnom + 273.15); // + (1/To)
    temperature = 1.0 / temperature; // Invert
    temperature = temperature - 273.15; // convert to C
    if (temperature > 40.0) {
        fan = 1;
    } else {
        fan = 0;
    }
}

void inizializzazione(void) {
    stati = 0; //Funzione di sicurezza
    delay_set_quartz(16);

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
    //LCD_write_message("TESTER BATTERIE");
    //delay_ms(500);
    //LCD_backlight(LCD_TURN_ON_LED);
    //LCD_clear();

    //IMPOSTAZIONE ADC
    ADCON0 = 0b00000000; //DISABILITO TUTTO
    ADCON1 = 0b00001011;
    ADCON2 = 0b10110101;
    ADCON0bits.CHS3 = 0; //IMPOSTAZIONE DI SICUREZZA
    ADCON0bits.CHS2 = 0; //IMPOSTAZIONE DI SICUREZZA
    ADCON0bits.CHS1 = 0; //IMPOSTAZIONE DI SICUREZZA
    //----------------------------------

    T0CON = 0x85;
    TMR0H = 0x0B;
    TMR0L = 0xDC;

    INTCONbits.INT0IF = 0;
    INTCONbits.INT0IE = 1; //già in alta priorità
    INTCON2bits.INTEDG0 = 1; //rising edge
    INTCON2bits.TMR0IP = 1; //alta priorità
    RCONbits.IPEN = 1;
    INTCONbits.GIEH = 1;
    INTCONbits.GIEL = 1;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;

    ADCON0bits.ADON = 1; //attivo ADC
}