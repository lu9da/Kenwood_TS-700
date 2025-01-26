
/*****************************************************************
* MicroLCD library
* For more information, please visit http://arduinodev.com
******************************************************************
* encoder library https://github.com/PaulStoffregen/Encoder
******************************************************************
*                 TABLAS DE ENTRADAS Y SALIDAS
******************************************************************
*    BAND_  A  B    |            VFO  1 2 3 4 5 6 7 8 9 10 11
*     144   1  1    |  MEM_A      0   1 0 1 0 1 0 1 0 1  0  1 
*     145   0  0    |  MEM_B      0   0 1 1 0 0 1 1 0 0  1  1
*     146   1  0    |  MEM_C      0   0 0 0 1 1 1 1 0 0  0  0 
*     147   0  1    |  MEM_D      0   0 0 0 0 0 0 0 0 1  1  1
******************************************************************
*   REPE_   A  B    |             144  145  146  147 145.4 147.6  
*  simplex  1  1    |  DRI_A       0    1    0    1    1     1    
*   -600    0  1    |  DRI_B       0    0    1    1    0     1    
*   +600    1  0    |  DRI_C       0    0    0    0    1     1    
******************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>
#include <MicroLCD.h>
#include <Encoder.h>
#include <EEPROM.h>


//*********** DEFINES

//#define test

#define REPE_A 2
#define REPE_B 3
#define ENCODER_BTN 4  // boton
//efine ENCODER_A     5
//efine ENCODER_B     6

//port B
#define BAND_A 8
#define BAND_B 9
#define MEM_A 10
#define MEM_B 11
#define MEM_C 12
#define MEM_D 13
//fin port B

#define RIT   A6
#define RIT_SW A7
#define DRI_A A1
#define DRI_B A2
#define DRI_C A3
#define TX_RX A0

//*********** END DEFINES

//*********** VARIABLES

bool RX = LOW;  // HIGH=RX, LOW=TX
byte flags = B00000000;
byte flags_old = B10000000;
byte vfo_mem = B00111100;
byte band = B00000011;
byte opciones = 3;
byte cursor_ = 48;
long newPosition;
long oldPosition = -999;
unsigned int ctcss_ = 0;
unsigned int ctcss_old_ = 0;
unsigned long memoria_tx = 14600000;
unsigned long memoria_rx = 14600000;
int rit_;
int rit_old;
int rit_min;
int rit_cen;
int rit_max;

int ctcss[43]= {0, 670, 719, 744,770,797,825,854,885,
                 915, 948, 974, 1000, 1035, 1072, 1109, 1148,
                 1188, 1230, 1273, 1318, 1365, 1413, 1462, 1514, 
                 1567, 1622, 1679, 1738, 1799, 1862, 1928, 2035,
                 2107, 2181, 2257, 2336, 2418,2503};



//*********** END VARIABLES

//*********** INSTANCES


LCD_SH1106 lcd; /* for SSD1306 OLED module */

// The Si5351 instance.
Si5351 si5351;

// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder myEnc(5, 6);


//*********** END INSTANCES


//************ SETUP

void setup() {


  # ifdef test

  pinMode(TX_RX, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(BAND_A, INPUT_PULLUP);
  pinMode(BAND_B, INPUT_PULLUP);
  pinMode(REPE_A, INPUT_PULLUP);
  pinMode(REPE_B, INPUT_PULLUP);
  pinMode(MEM_A, INPUT_PULLUP);
  pinMode(MEM_B, INPUT_PULLUP);
  pinMode(MEM_C, INPUT_PULLUP);
  pinMode(MEM_D, INPUT_PULLUP);

  #endif

  #ifndef test

  pinMode(TX_RX, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(BAND_A, INPUT);
  pinMode(BAND_B, INPUT);
  pinMode(REPE_A, INPUT_PULLUP);
  pinMode(REPE_B, INPUT_PULLUP);
  pinMode(MEM_A, INPUT);
  pinMode(MEM_B, INPUT);
  pinMode(MEM_C, INPUT);
  pinMode(MEM_D, INPUT);

  #endif

  pinMode(DRI_A, OUTPUT);
  pinMode(DRI_B, OUTPUT);
  pinMode(DRI_C, OUTPUT);


  bool i2c_found = false;

  lcd.begin();
  lcd.clear();

  i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000597UL, 0);

  /*
  if(!i2c_found){  // test del SI5351
    lcd.setCursor(17, 5);
	  lcd.setFontSize(FONT_SIZE_SMALL);
    lcd.println("SI5351 not found!");
    while(1){}

  }
  */


  if (digitalRead(ENCODER_BTN) == LOW) {
    if (analogRead(RIT_SW) >= 800) {
      setup_rit();
    }
    
  }


  EEPROM.get(200,rit_min);    //rit_min
  EEPROM.get(210,rit_cen);    //rit_cen
  EEPROM.get(220,rit_max);    //rit_max


  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);


  lcd.setCursor(33, 2);
  lcd.setFontSize(FONT_SIZE_MEDIUM);
  lcd.println("TS-700s");
  lcd.setCursor(17, 5);
  lcd.setFontSize(FONT_SIZE_SMALL);
  lcd.println("(c) LU9DA - 2025");

  if (EEPROM.read(254) != 170 && EEPROM.read(253) != 85) {
    lcd.println();
    lcd.println("     eeprom error!");
  }

  delay(750);

  if (digitalRead(ENCODER_BTN) == LOW) {
    eeprom_erase();
    delay(250);
    lcd.clear();
    setup_rit();
    lcd.clear();
  }

  delay(750);

  newPosition = myEnc.read();

  if (newPosition != oldPosition) {
    oldPosition = newPosition;
  }

  lcd.clear();


}

//************ END SETUP


//************* LOOP

void loop() {

  //******** TOMA DE DECISIONES


  // VFO / MEMORIA

  flags = PINB;

  if (flags != flags_old){   // selecciona vfo/memoria solo sia hay cambio en flags

    flags_old = flags;

    lcd.clear();

    if ((flags & vfo_mem) >> 2 != 0) {

      // MEMORIA

      si5351.output_enable(SI5351_CLK0, 1);
      pinMode(BAND_A, OUTPUT);
      pinMode(BAND_B, OUTPUT);
      unsigned long data_tmp;
      int i = (flags & vfo_mem) >> 2;
      lcd.setCursor(0, 0);
      lcd.setFontSize(FONT_SIZE_MEDIUM);
      lcd.print("    MEM: ");
      lcd.println((flags & vfo_mem) >> 2);
      lcd.setFontSize(FONT_SIZE_SMALL);
      lcd.println(" ");
      lcd.print("  RX: ");
      EEPROM.get(i * 8, memoria_rx);
      lcd.println(memoria_rx);
      lcd.println(" ");
      lcd.print("  TX: ");
      EEPROM.get((i * 8) + 4, memoria_tx);
      lcd.println(memoria_tx);
      lcd.println();
      ctcss_ = ctcss[EEPROM.read(i+100)];
      lcd.print("  CTCSS: ");
      lcd.print(ctcss_/10);
      lcd.print(".");
      lcd.print(ctcss_%10);
      lcd.print(" Hz ");

      switch (memoria_rx/100000) {

        case 145:  //a:0 b:0 145mhz
   
          digitalWrite(BAND_A, LOW);
          digitalWrite(BAND_B, LOW);

        break;

        case 146:  //a:1 b:0 146mhz

          digitalWrite(BAND_A, HIGH);
          digitalWrite(BAND_B, LOW);

        break;

        case 147:  //a:0 b:1 147mhz

          digitalWrite(BAND_A, LOW);
          digitalWrite(BAND_B, HIGH);

        break;

        case 144:  //a:1 b:1 144mhz

          digitalWrite(BAND_A, HIGH);
          digitalWrite(BAND_B, HIGH);

        break;

      }


      // fin MEMORIA

    } else {

      // VFO

      si5351.output_enable(SI5351_CLK0, 0);
      pinMode(BAND_A, INPUT);
      pinMode(BAND_B, INPUT);
      lcd.setCursor(0, 0);
      lcd.setFontSize(FONT_SIZE_SMALL);
      lcd.print(" ");
      lcd.setFontSize(FONT_SIZE_MEDIUM);
      lcd.print("     VFO      ");
      lcd.setFontSize(FONT_SIZE_SMALL);
      lcd.println();
      ctcss_ = ctcss[EEPROM.read(100)];
      lcd.setCursor(25,6);
      lcd.print("CTCSS: ");
      lcd.print(ctcss_/10);
      lcd.print(".");
      lcd.print(ctcss_%10);
      lcd.print(" Hz ");
      lcd.setCursor(0,4);
  

      switch (flags & band) {

        case 0:  //a:0 b:0 145mhz
          lcd.print("    Band: 145 MHz");
          memoria_rx=14500000;  // 145-10,7-8,2 = 126,1
          memoria_tx=14500000;
        break;

        case 1:  //a:1 b:0 146mhz
          lcd.print("    Band: 146 MHz");
          memoria_rx=14600000;  // 146-10,7-8,2 = 127,1
          memoria_tx=14600000;
        break;

        case 2:  //a:0 b:1 147mhz
          lcd.print("    Band: 147 MHz");
          memoria_rx=14700000;  // 147-10,7-8,2 = 128,1
          memoria_tx=14700000;
        break;

        case 3:  //a:1 b:1 144mhz
          lcd.print("    Band: 144 MHz");
          memoria_rx=14400000; // 144-10,7 -8,2 = 125,1
          memoria_tx=14400000;
        break;


      }  



    }




    
    lcd.setCursor(0, 0);
    if (RX == HIGH){
      lcd.print("RX");
      drive(memoria_rx);    
    }else{
      lcd.print("TX");
      drive(memoria_tx);
    }

  }

  // END VFO / MEMORIA


  // EDITAR SOLO EN MEMORIA y RX
  // EN VFO Y RX EDITA CTCSS

  if (RX == HIGH){

    if ((flags & vfo_mem) >> 2 != 0 && (digitalRead(ENCODER_BTN) == LOW)) {

      delay(100);

      if (digitalRead(ENCODER_BTN) == LOW) {

        while (digitalRead(ENCODER_BTN) == LOW) {
          //
        }

        lcd.setCursor(0, 6);
        lcd.println("\n  write     Y   N");
        edita_sel();
      }
    }

    if ((flags & vfo_mem) >> 2 == 0 && (digitalRead(ENCODER_BTN) == LOW)) {

      while (digitalRead(ENCODER_BTN) == LOW) {
        //
      }

     // lcd.setCursor(12, 6);
     // lcd.println("CTCSS: ");
        
      boolean conf_ = HIGH;
      
      oldPosition = newPosition;

      while (conf_) {

        newPosition = myEnc.read() / 4;

        if (newPosition != oldPosition) {

          if (newPosition > oldPosition) {

            if (opciones != 38) {
              opciones++;
            }

          } else {

            if (opciones != 0) {
              opciones--;
            }
                
          }
          oldPosition = newPosition;
          lcd.setCursor(25, 6);
          lcd.print("CTCSS: ");
          lcd.print(ctcss[opciones]/10);
          lcd.print(".");
          lcd.print(ctcss[opciones]%10);
          lcd.print(" Hz ");

        }

        if (digitalRead(ENCODER_BTN) == LOW) {

          while (digitalRead(ENCODER_BTN) == LOW) {
            //
          }
        

          EEPROM.write(100,opciones);

          delay(100);

          ctcss_ = opciones;

          conf_ = LOW;

        }  
      }
    }  

  }

  // FIN EDITAR SOLO EN MEMORIA Y RX

  // RX

  if (digitalRead(TX_RX) == HIGH && RX == LOW) {  

    delay(50);

    if (digitalRead(TX_RX) == HIGH) {  // sisi, es RX

      RX = HIGH;  // STATUS RX/TX EN RX MODE

      drive(memoria_rx);

    }
  }  

  // END RX

  // TX

  if (digitalRead(TX_RX) == LOW && RX == HIGH) {  

    delay(50);

    if (digitalRead(TX_RX) == LOW) {  // sisi, es TX

      RX = LOW;  // STATUS RX/TX EN TX MODE

      drive(memoria_tx);      

    }
  }  
  
  // END TX


  //RIT

  if(RX == HIGH && analogRead(RIT_SW) >= 800){ //rit ON

    //lcd.setCursor(0,7);

    
    int ppp=analogRead(RIT);

    if(ppp > rit_cen - 20 || ppp < rit_cen + 20){ 
      rit_ = 0;
      //lcd.print("0    ");
    }  


    if (ppp <= rit_cen - 20){
      rit_ = map(ppp, rit_min,rit_cen-20, -250, 0);
      //lcd.print(map(analogRead(RIT), rit_min,rit_cen-20, -100,0));
      //lcd.print("   ");
    }

    if(ppp >= rit_cen + 20){ 
      rit_ = map(ppp, rit_cen+20, rit_max, 0, 250);
      //lcd.print(map(analogRead(RIT), rit_cen+20, rit_max, 0,100));
      //  lcd.print("   ");
    }

    if(rit_ < rit_old - 2  || rit_ > rit_old + 2){

        rit_old = rit_;

        //lcd.print(memoria_rx + (rit_ * 5));
        //lcd.print(" ");

        drive(memoria_rx + (rit_));

      } 


    }

  // end RIT

//******** END TOMA DE DECISIONES

}

//************* END LOOP


//************* FUNCIONES


// *************   eeprom erase & init
void eeprom_erase() {

  lcd.clear();

  lcd.println("     Memory INIT");
  lcd.println("");
  lcd.println("    release button");
  lcd.println("    and push again");
  lcd.println("");
  lcd.println("  power off to cancel"); 


  while (digitalRead(ENCODER_BTN) == LOW) {}
  delay(500);
  while (digitalRead(ENCODER_BTN) == HIGH) {}
  
  for (int i = 0; i < 96; i += 8) {

    lcd.setCursor(10, 3);

    lcd.print(i / 8);

    EEPROM.put(i, 14600000UL);
    EEPROM.put(i + 4, 14600000UL);

    delay(250);


  }

  lcd.setCursor(10, 3);

  lcd.print("  ");
    

  for (int i = 0; i <= 12; i++) {

    lcd.setCursor(10, 3);

    lcd.print(i);
    
    EEPROM.write(i+100, 0);

    delay(250);

  }

  EEPROM.write(200, 0);       //rit_min
  EEPROM.write(202, 512);     //rit_cen
  EEPROM.write(204, 1023);    //rit_max
  
  EEPROM.write(254, 170);
  EEPROM.write(253, 85);

  delay(250);

}

//********** fin eeprom erase & init init





//********** edita sel

void edita_sel() {

  boolean conf_ = HIGH;

  while (conf_) {

    newPosition = myEnc.read() / 4;

    if (newPosition != oldPosition) {

      if (newPosition > oldPosition) {

        if (opciones != 5) {
          opciones++;
        }

      } else {

        if (opciones != 0) {
          opciones--;
        }
      }

      oldPosition = newPosition;
    }


    switch (opciones) {

      case 0:

        lcd.setCursor(0, 3);
        lcd.print(">");
        lcd.setCursor(0, 5);
        lcd.print(" ");

        break;

      case 1:

        lcd.setCursor(0, 5);
        lcd.print(">");
        lcd.setCursor(0, 3);
        lcd.print(" ");
        lcd.setCursor(60, 7);
        lcd.print(" ");

        break;

      case 2:

        lcd.setCursor(60, 7);
        lcd.print(">");
        lcd.setCursor(0, 5);
        lcd.print(" ");
        lcd.setCursor(85, 7);
        lcd.print(" ");

        break;

      case 3:

        lcd.setCursor(85, 7);
        lcd.print(">");
        lcd.setCursor(60, 7);
        lcd.print(" ");

        break;
    }

    if (digitalRead(ENCODER_BTN) == LOW) {

      delay(100);

      if (digitalRead(ENCODER_BTN) == LOW) {

        while (digitalRead(ENCODER_BTN) == LOW) {
        }

        switch (opciones) {

          case 0:
            lcd.setCursor(0, 3);
            lcd.print(" ");
            conf_ = LOW;
            oldPosition = -999;
            memoria_rx = edita(memoria_rx, 3);
            break;

          case 1:
            lcd.setCursor(0, 5);
            lcd.print(" ");
            conf_ = LOW;
            oldPosition = -999;
            memoria_tx = edita(memoria_tx, 5);

            break;

          case 2:

            EEPROM.put(((flags & vfo_mem) >> 2) * 8, memoria_rx);
            EEPROM.put((((flags & vfo_mem) >> 2) * 8) + 4, memoria_tx);
            EEPROM.write(100+((flags & vfo_mem) >> 2),EEPROM.read(100));
            lcd.setCursor(0, 7);
            lcd.print("                 ");
            ctcss_ = ctcss[EEPROM.read(100+(flags & vfo_mem) >> 2)];
            lcd.setCursor(0, 7);
            lcd.print("  CTCSS: ");
            lcd.print(ctcss_/10);
            lcd.print(".");
            lcd.print(ctcss_%10);
            lcd.print(" Hz ");
            
            opciones = 3;
            conf_ = LOW;

            break;

          case 3:

            lcd.setCursor(0, 7);
            lcd.print("                 ");
            ctcss_ = ctcss[EEPROM.read(100+(flags & vfo_mem) >> 2)];
            lcd.setCursor(0, 7);
            lcd.print("  CTCSS: ");
            lcd.print(ctcss_/10);
            lcd.print(".");
            lcd.print(ctcss_%10);
            lcd.print(" Hz ");

            conf_ = LOW;

            break;
        }
      }
    }
  }
}

//********** fin edita sel

//********** edita

unsigned long edita(unsigned long frecuencia, byte linea) {
  //void edita(unsigned long frecuencia, byte linea){

  oldPosition = newPosition = myEnc.read() / 4;

  cursor_ = 48;
  lcd.setCursor(cursor_, linea + 1);
  lcd.print("-");
  lcd.setCursor(90, linea);
  lcd.print("<");

  while (1) {

    newPosition = myEnc.read() / 4;

    if (newPosition != oldPosition) {
      if (newPosition > oldPosition) {

        switch (cursor_) {
          case 48:
            frecuencia = frecuencia + 100000;
            break;

          case 54:
            frecuencia = frecuencia + 10000;
            break;

          case 60:
            frecuencia = frecuencia + 1000;
            break;

          case 66:
            frecuencia = frecuencia + 100;
            break;

          case 72:
            frecuencia = frecuencia + 10;
            break;

          case 90:
            lcd.setCursor(cursor_, linea);
            lcd.print(" ");
            lcd.setCursor(cursor_, linea + 1);
            lcd.print(" ");
            lcd.setCursor(0, linea);
            lcd.print(">");
            return frecuencia;
            break;
        }

      } else {

        switch (cursor_) {
          case 48:
            frecuencia = frecuencia - 100000;
            break;

          case 54:
            frecuencia = frecuencia - 10000;
            break;

          case 60:
            frecuencia = frecuencia - 1000;
            break;

          case 66:
            frecuencia = frecuencia - 100;
            break;

          case 72:
            frecuencia = frecuencia - 10;
            break;
          case 90:
            lcd.setCursor(cursor_, linea);
            lcd.print(" ");
            lcd.setCursor(cursor_, linea + 1);
            lcd.print(" ");
            lcd.setCursor(0, linea);
            lcd.print(">");
            return frecuencia;
            break;
        }
      }

      oldPosition = newPosition;

      if (frecuencia > 14799990) {
        frecuencia = 14400000;
      }
      if (frecuencia < 14400000) {
        frecuencia = 14799990;
      }
      lcd.setCursor(36, linea);
      lcd.printLong(frecuencia);
      lcd.setCursor(90, linea);
      lcd.print("<");
      lcd.setCursor(cursor_, linea + 1);
      lcd.print("-");
    }

    if (digitalRead(ENCODER_BTN) == LOW) {
      delay(100);
      if (digitalRead(ENCODER_BTN) == LOW) {

        while (digitalRead(ENCODER_BTN) == LOW) {
        }

        lcd.setCursor(cursor_, linea + 1);
        lcd.print(" ");
        cursor_ = cursor_ + 6;
        if (cursor_ > 90) {
          cursor_ = 48;
        }
        if (cursor_ > 72) {
          cursor_ = 90;
        }

        lcd.setCursor(cursor_, linea + 1);
        lcd.print("-");
      }
    }
  }
}

//********** fin edita

//*********** drive

void drive(unsigned long memoria) {

  //los drive estan invertidos por los conversores de TTL a CMOS

  byte option = memoria/100000;


  lcd.setFontSize(FONT_SIZE_SMALL);
  lcd.setCursor(0, 0);

  if (RX == HIGH){
    lcd.print("RX");
  }else{
    lcd.print("TX");

    if(option == 144){

      if ( digitalRead(REPE_B) != HIGH ){  // es -600  
        //NADA, NO PUEDE IR ABAJO DE 144
      }

    
      if ( digitalRead(REPE_A) != HIGH ){  // es +600
        option = 1; 
      }

    }

    if(option == 145){

      if ( digitalRead(REPE_B) != HIGH ){  // es -600  
        option = 2; 
      }

    
      if ( digitalRead(REPE_A) != HIGH ){  // es +600
        option = 3;  
      }

    }
    
    if(option == 146){

      if ( digitalRead(REPE_B) != HIGH ){  // es -600  
        option = 4; 
      }

    
      if ( digitalRead(REPE_A) != HIGH ){  // es +600
        option = 5; 
      }

    }

    if(option == 147){

      if ( digitalRead(REPE_B) != HIGH ){  // es -600  
        option = 6;
      }

    
      if ( digitalRead(REPE_A) != HIGH ){  // es +600
        option = 7;
      }

    }


  }
  
  if (ctcss_ != 0){

    si5351.output_enable(SI5351_CLK2, 1);
    si5351.set_freq(ctcss_ * 102400ULL, SI5351_CLK2);    //subtono 67,0 = 670 * 10 * 1024 = 686080 -> 250,3 * 10 * 32 = 2563072
  
  }else{

    si5351.output_enable(SI5351_CLK2, 0);

  }

  switch (option) {

    case 145:  //a:0 b:0

      si5351.set_freq(12610000000ULL, SI5351_CLK1);  //126100, 145 mhz band
      si5351.set_freq((memoria-13680000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, LOW);
      digitalWrite(DRI_B, HIGH);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13680000ul);


    break;

    case 146:  //a:1 b:0

      si5351.set_freq(12710000000ULL, SI5351_CLK1);  //127100,  146 mhz band
      si5351.set_freq((memoria-13780000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, HIGH);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13780000ul);

    break;

    case 147:  //a:0 b:1

      si5351.set_freq(12810000000ULL, SI5351_CLK1);  //128100,  147 mhz band
      si5351.set_freq((memoria-13880000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, LOW);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);                     
      //lcd.print(memoria-13880000ul);               

    break;

    case 144:  //a:1 b:1

      si5351.set_freq(12510000000ULL, SI5351_CLK1);  //125100,  144 mhz band
      si5351.set_freq((memoria-13580000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, HIGH);
      digitalWrite(DRI_B, HIGH);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13580000ul);

    break;


/*
144 -600 NO 
144 +600 SI 125.700  (1)

145 -600 SI 125.500  (2)
145 +600 SI 126.700  (3)

146 -600 SI 126.500  (4)
146 +600 SI 127.700  (5)

147 -600 SI 127.500  (6)
147 +600 SI 128.700  (7)

*/

    case 1:  // 144 +600

      si5351.set_freq(12570000000ULL, SI5351_CLK1);  //125700, 144,6 mhz band (125.1 + .600)
      //si5351.set_freq((memoria-1372000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, HIGH);
      digitalWrite(DRI_B, HIGH);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13720000ul);
    break;


    case 2:  // 145 -600

      si5351.set_freq(12550000000ULL, SI5351_CLK1);  //125500, 145,6 mhz band (126.1 - .600)
      //si5351.set_freq((memoria-1372000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, HIGH);
      digitalWrite(DRI_B, HIGH);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13720000ul);
    break;

    case 3:  // 145 +600

      si5351.set_freq(12670000000ULL, SI5351_CLK1);  //125700, 145,6 mhz band (126.1 + .600)
      //si5351.set_freq((memoria-1372000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, HIGH);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13720000ul);
    break;

    case 4:  // 146 -600

      si5351.set_freq(12650000000ULL, SI5351_CLK1);  //126500, 145,4 mhz band (127.1 - .600)
      //si5351.set_freq((memoria-1372000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, HIGH);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13720000ul);
    break;

    case 5:  // 146 +600

      si5351.set_freq(12770000000ULL, SI5351_CLK1);  //127700, 146,6 mhz band (125.1 + .600)
      //si5351.set_freq((memoria-1372000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, LOW);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13720000ul);
    break;

    case 6:  // 147 -600

      si5351.set_freq(12750000000ULL, SI5351_CLK1);  //125700, 144,6 mhz band (125.1 + .600)
      //si5351.set_freq((memoria-1372000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, LOW);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);
      //lcd.print(memoria-13720000ul);
    break;

    case 7:  // 147 + 600

      si5351.set_freq(12870000000ULL, SI5351_CLK1);  //128700,  147,6 mhz band (128.1 + .600)
      //si5351.set_freq((memoria-13940000ul)*1000ull, SI5351_CLK0);    //8.2-9.2 vfo
      digitalWrite(DRI_A, LOW);
      digitalWrite(DRI_B, LOW);
      digitalWrite(DRI_C, HIGH);    
      //lcd.print(memoria-13940000ul);
    break;
  
  }

}

//*********** fin drive
// Calibracion del rit

void setup_rit(){

  lcd.println("       RIT INIT");
  lcd.println("RIT off graba/sale");
  lcd.println("Boton Enc. cambia");

  while(analogRead(RIT_SW) >= 800){

    while(digitalRead(ENCODER_BTN) == LOW){
      delay(100);
    }

    while(digitalRead(ENCODER_BTN) != LOW){
      lcd.setCursor(10, 4);
      lcd.print("min: ");
      rit_min = analogRead(RIT);
      lcd.print(rit_min);
      lcd.print("    ");  
      delay(100);
    }
    
    while(digitalRead(ENCODER_BTN) == LOW){
      delay(100);
    }

    while(digitalRead(ENCODER_BTN) != LOW){
      lcd.setCursor(10, 5);
      lcd.print("cen: ");
      rit_cen = analogRead(RIT);
      lcd.print(rit_cen);
      lcd.print("    ");  
      delay(100);
    }
    
    while(digitalRead(ENCODER_BTN) == LOW){
      delay(100);
    }


    while(digitalRead(ENCODER_BTN) != LOW){
      lcd.setCursor(10, 6);
      lcd.print("max: ");
      rit_max = analogRead(RIT);
      lcd.print(rit_max);
      lcd.print("    ");
      delay(100);  
    }
    
    while(digitalRead(ENCODER_BTN) == LOW){
      delay(100);
    }
    
    int ppp;

    while(digitalRead(ENCODER_BTN) != LOW){
      lcd.setCursor(10, 7);
      lcd.print("map: ");
      ppp=analogRead(RIT);
      if (ppp <= rit_cen - 20){
        lcd.print(map(analogRead(RIT), 0,rit_cen-20, -100,0));
      }else if(ppp >= rit_cen + 20){ 
        lcd.print(map(analogRead(RIT), rit_cen+20, rit_max, 0,100));
      }else{
        lcd.print("0");
      }  
      lcd.print("    ");
      delay(100);  
    } 
    
    
    while(digitalRead(ENCODER_BTN) == LOW){
      delay(100);
    }

  }
  
  lcd.clear();
  EEPROM.put(200, rit_min);    //rit_min
  EEPROM.put(210, rit_cen);    //rit_cen
  EEPROM.put(220, rit_max);    //rit_max

}

// end calibracion del rit

//************* END FUNCIONES


//************* DUMP




//************* END DUMP 
