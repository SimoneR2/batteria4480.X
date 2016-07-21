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

#define R1 68000 //INSERIRE VALORE Ohm PARTITORE
#define R2 33000 //INSERIRE VALORE Ohm PARTITORE
void inizializzazione(void);
void read_adc(void);
unsigned int lettura [3] = 0;
unsigned int ore, minuti, secondi = 0;
unsigned long tempo = 0;
unsigned char str [8] = 0;
float rapporto, current, voltage = 0;

unsigned char combinazioni[] = {
    0b00000001, //AN0
    0b00000101, //AN1
    0b00001001  //AN2
    0b00001101  //AN2
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

void main(void) {
    delay_set_quartz(16);
    rapporto = (R1 + R2);
    rapporto = R2 / rapporto;
    inizializzazione();
    while(1){
         read_adc();
    while ((current < -0.5) || (voltage < 13.5)) {
        PORTBbits.RB7 = 1; //attivo ciclo ricarica
        LCD_goto_line(1);
        LCD_write_message("Ciclo ricarica..");
        LCD_goto_line(2);
        sprintf(str, "V:%.3f", voltage); //convert float to char
        str[7] = '\0'; //add null character
        LCD_write_string(str); //write Voltage in LCD
        sprintf(str, " I:%.3f", current); //convert float to char
        str[7] = '\0'; //add null character
        LCD_write_string(str); //write Current in LCD
        read_adc();
        delay_ms(500); //attendi un po' prima di rileggere il tutto
       //
        delay_ms(1);
    }
    if ((current > -1)&&(voltage > 14.5)) {
        LCD_write_message("Carica terminata");
        PORTBbits.RB7 = 0; //attivo ciclo ricarica
        delay_ms(5000);
    }
    }
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
    INTCONbits.GIE = 1;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;
    ADCON0bits.ADON = 1; //attivo ADC
}