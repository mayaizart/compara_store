/**
  *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS 1    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required **
 * SPI SS 2    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required **
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 */

/*  Wiring up the RFID Readers ***
 *  RFID readers based on the Mifare RC522 like this one:  http://amzn.to/2gwB81z
 *  get wired up like this:
 *
 *  RFID pin    Arduino pin (above)
 *  _________   ________
 *  SDA          SDA - each RFID board needs its OWN pin on the arduino
 *  SCK          SCK - all RFID boards connect to this one pin
 *  MOSI         MOSI - all RFID boards connect to this one pin
 *  MISO         MISO - all RFID boards connect to this one pin
 *  IRQ          not used
 *  GND          GND - all RFID connect to GND
 *  RST          RST - all RFID boards connect to this one pin
 *  3.3V         3v3 - all RFID connect to 3.3v for power supply
 *
 */


#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define UNICORN_PIN             7          // DIN pin of unicorn hat.
Adafruit_NeoPixel display_result = Adafruit_NeoPixel(64, UNICORN_PIN, NEO_GRB + NEO_KHZ800);

// Light configurations
int gt[] = {5, 4, 11, 12, 19, 18, 29, 30, 34, 33, 44, 45, 52, 51, 58, 59};                    // greater than sign '>'
int lt[] = {2, 3, 12, 11, 20, 21, 26, 25, 37, 38, 43, 42, 51, 52, 61, 60};                    // less than sign '<'
int eq[] = {9,22, 10,21, 11,20, 12,19, 13,18, 14,17, 41,54, 42,53, 43,52, 44,51, 45,50, 46,49};   // equal sign '='

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RST_PIN         9          // Pin of RFID's reset value

//each SS_x_PIN variable indicates the unique SS pin for another RFID reader
#define SS_LEFT        6         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_RIGHT       5          // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1

//must have one SS_x_PIN for each reader connected
#define NR_OF_READERS   2

byte ssPins[] = {SS_LEFT, SS_RIGHT};

MFRC522 mfrc522[NR_OF_READERS];   // Create MFRC522 instance.
String read_rfid;

// Structure representing the information of a Data
struct Data {
  String id;
  int number;
  String color;
  int color_order;
  String shape;
  int shape_sides;
  int user_order;
};

// Setup the existing Datas.
/*                   ID         | no.  | color        | color  | shape     | shape |
                    ____________|______|______________|_order__|___________|_sides_|
*/                       
Data blue_data =   {"a9aec85",    1,     "blue",       6,       "square",    4  };
Data yellow_data = {"aac8d54",    2,     "yellow",     3,       "hexagon",   6  };
Data green_data =  {"dabbeb85",   3,     "green",      4,       "circle",    0  };
Data pink_data =   {"fa37ec85",   4,     "pink",       1,       "star",      10 };
Data light_data =  {"5a93fe56",   5,     "light blue", 5,       "triangle",  3  };
Data orange_data = {"2a3ded85",   6,     "orange",     2,       "circle",    0  };

#define NR_DATAS  6
Data data_order[] = {blue_data, yellow_data, green_data, pink_data, light_data, orange_data};

String compare_by[] = { "Color Name", "Shape Edges", "Color", "Shape Name", "Number" };
int compare_index = 0;
int counter = 0;

int left_data = -1;
int right_data = -1;

int buttonValue = 0; //Stores analog value when button is pressed

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // Initialize serial communications with the PC
  counter = 0;
  left_data = -1;
  right_data = -1;
  
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  showLogo();
  SPI.begin();        // Init SPI bus

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }  
  delay(2000);
  welcomeDisplay();

  display_result.begin();
  display_result.show(); // Initialize all pixels to 'off'

  
}

void loop() {
  buttonValue = analogRead(A0); 
  // Check if a button is pressed (will reset the game)
  if (buttonValue>=1000){
    // If right button is pressed, the  Data's will be shuffled and the comparator will be kept secret
    Serial.println("Right Button Pressed");
    reset(false);
    delay(500);
  }
  //For 2nd button:
  else if (buttonValue>=900 && buttonValue<=950) {
    // If right button is pressed, the  Data's will be shuffled and the comparator will be revealed
    Serial.println("Left Button Pressed");
    reset(true);
    delay(500);
  }
  // run code for compara-store game
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    // Look for new cards
    if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
      Serial.print(F("Reader "));
      Serial.print(reader);
      // Show some details of the PICC (that is: the tag/card)
      dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);
      Serial.println(read_rfid);
      for (int i = 0; i < NR_DATAS; i++) {
        if (data_order[i].id == read_rfid) {
          // get index of data
          int data = data_order[i].number - 1;
          if (reader == 0) {
            left_data = data;
          } else {
            right_data = data;
          }
          // If both scales have Data's, make comparison
          if (left_data > -1 && right_data > -1) {
            compare(data_order[left_data], data_order[right_data]);
          }
        }
      }  
      // Halt PICC
      mfrc522[reader].PICC_HaltA();
      // Stop encryption on PCD
      mfrc522[reader].PCD_StopCrypto1();
    }
  } 
}

void reset(boolean is_known) {
  // reset values
  counter = 0;
  left_data = -1;
  right_data = -1;
  
  // turn off scale
  display_scale_result(0, true);
  
  // Randomly select by what to compare
  if (compare_index > 2) {
    compare_index = random(0,3);
  }
  else {
    compare_index = random(2,5);
  }
  Serial.print("comparing by: ");
  Serial.println(compare_by[compare_index]);
  resetGame(is_known);
  
}

void dump_byte_array(byte *buffer, byte bufferSize) {
  read_rfid = "";
  for (byte i = 0; i < bufferSize; i++) {
    read_rfid = read_rfid + String(buffer[i], HEX);
  }
}

int compare(Data cmp1, Data cmp2) {
  printComparison(cmp1, cmp2);
  // "Number", "Color", "Color Name", "Shape Name", "Shape Edges"
  String compare_type = compare_by[compare_index];
  int result;
  if (compare_type == "Number") {
    result = num_cmpr(cmp1, cmp2);
  }
  else if (compare_type == "Color") {
    result = color_order_cmpr(cmp1, cmp2);
  }
  else if (compare_type == "Color Name") {
    result = color_cmpr(cmp1, cmp2);
  }
  else if (compare_type == "Shape Name") {
    result = shape_cmpr(cmp1, cmp2);
  }
  else if (compare_type == "Shape Edges") {
    result = shape_sides_cmpr(cmp1, cmp2);
  }
  // display result
  display_scale_result(result, false);
  // reset scale
  left_data = -1;
  right_data = -1;
  // update counter
  counter += 1;
  updateCounter();
  delay(3000);
  // turn off scale
  display_scale_result(result, true);
}

int num_cmpr(Data cmp1, Data cmp2) {
  // Need to cast the void * to Data *
  int a = cmp1.number;
  int b = cmp2.number;
  // The comparison
  return b - a;
}

int color_cmpr(Data cmp1, Data cmp2) {
  // Need to cast the void * to Data *
  String a = cmp1.color;
  String b = cmp2.color;
  // The comparison
  return a > b ? -1 : (a < b ? 1 : 0);
}

int color_order_cmpr(Data cmp1, Data cmp2) {
  // Need to cast the void * to Data *
  int a = cmp1.color_order;
  int b = cmp2.color_order;
  // The comparison
  return a > b ? -1 : (a < b ? 1 : 0);
}

int shape_cmpr(Data cmp1, Data cmp2) {
  // Need to cast the void * to Data *
  String a = cmp1.shape;
  String b = cmp2.shape;
  // The comparison
  return a > b ? -1 : (a < b ? 1 : 0);
}

int shape_sides_cmpr(Data cmp1, Data cmp2) {
  // Need to cast the void * to Data *
  int a = cmp1.shape_sides;
  int b = cmp2.shape_sides;
  // The comparison
  return a > b ? -1 : (a < b ? 1 : 0);
}

void printComparison(Data cmp1, Data cmp2) {
  Serial.print("Comparing Data no. ");
  Serial.print(cmp1.number);
  Serial.print(" with Data no. ");
  Serial.println(cmp2.number);
}

// Fill the dots one after the other with a color
void display_scale_result(int result, bool turn_off) {
  // Turning display on or off?
  uint32_t color = display_result.Color(0, 0, 255);
  if (turn_off) {
    color = display_result.Color(0, 0, 0);
    for (int i = 0; i<64; i++) {
      display_result.setPixelColor(i, color);
      display_result.show();
    }
  }
  else if (result > 0){
    for(int i = 0; i < 8; i++) {
    // Greater Than
      display_result.setPixelColor(gt[2*i], color);
      display_result.setPixelColor(gt[2*i+1], color);
      delay(50);
      display_result.show();
    }
  }
  else if (result < 0){
    // Less Than
    for(int i = 0; i < 8; i++) {
      display_result.setPixelColor(lt[2*i], color);
      display_result.setPixelColor(lt[2*i+1], color);
      delay(50);
      display_result.show();
    }
  }
  else if (result == 0){
    // Equal
    for(int i = 0; i < 12; i++) {
      display_result.setPixelColor(eq[2*i], color);
      display_result.setPixelColor(eq[2*i+1], color);
      delay(50);
      display_result.show();
    }
  }
}

void showLogo() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(25, 10); 
  // Compara \n Store
  display.setTextSize(2); 
  display.println("Compara");
  display.display();
  display.setCursor(38, 40); 
  display.println("Store");
  display.display();
}

void welcomeDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  // What to do
  display.setTextSize(2);
  display.setCursor(17, 10); 
  display.println("Organize");
  display.setCursor(7, 35); 
  display.println("the Datas!");
  display.display();
  delay(2500);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); 
  display.println("The scale will show \nyou what order they \nneed to be in!");
  display.display();
  display.println("The more comparisons you make, the more \nexpensive it gets!");
  display.display();
  display.println("Press one of the \nbuttons to start.");
  display.display();

}

// Display text for new game (show by what data's are compared if hint==true)
void resetGame(boolean hint) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  // Comparing By...
  display.setTextSize(2.5); 
  display.setCursor(10, 15); 
  display.println("Comparing");
  display.display(); 
  display.setTextSize(2); 
  display.setCursor(37, 35); 
  display.print("By...");
  display.display();
  delay(2500);
  display.clearDisplay();
  // '?' Or Compare Type
  if (hint) {
    display.setTextSize(2);
    String text = compare_by[compare_index];
    text.replace(" ", "\n");
    for (int i = 0; i <= SCREEN_HEIGHT; i++) {
      display.setCursor(0, i); 
      display.println(text);
      display.display();
      delay(2); 
      display.clearDisplay();
    }
  } else {
    display.setTextSize(8); 
    display.setCursor(45, 5); 
    display.println("?");
    display.display();
    delay(3000);
  }
  updateCounter();
}

// update counter display to current count.
void updateCounter() {
  display.clearDisplay();
  display.setTextSize(7);      
  display.setTextColor(WHITE); // Draw white text
  if (counter > 9) {
    display.setCursor(10, 0);
  } else {
    display.setCursor(40, 0);    
  }
  display.print(counter);
  display.display();
  display.setTextSize(5);     
  display.println("$");
  display.display();
  display.setTextSize(1); 
  display.setCursor(20, display.height()-10);    // Start at top-left corner
  display.println(F("Comparisons made"));
  display.display();
}
