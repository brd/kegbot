// Kegbot

#include <SdFat.h>
#include <SdFatUtil.h>  // define FreeRam()
#include <SPI.h>
#include <Ethernet.h>
#include "WebServer.h"
//#include "Wire.h"
//#include "LiquidCrystal.h"

// Configure the Ethernet card MAC and IP
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,34,2);

// Configure the server to listen on port 80
#define PREFIX ""
WebServer webserver(PREFIX, 80);

// Configure the SD Card
const uint8_t SdChipSelect = 4; // Pin 4 on Ethernet
SdFat file;

// Configure the LCD
//LiquidCrystal lcd(4, 5, 6, 7, 8, 9);
//LiquidCrystal lcd(3, 5, 6, 7, 8, 9);

// Configure the files
const char t1_name_file[] = "T1_NAME.TXT";
const char t1_brew_file[] = "T1_BREW.TXT";
const char t1_pour_file[] = "T1_POUR.TXT";
const char t1_weight_file[] = "T1_WEIGH.TXT";

const char t2_name_file[] = "T2_NAME.TXT";
const char t2_brew_file[] = "T2_BREW.TXT";
const char t2_pour_file[] = "T2_POUR.TXT";
const char t2_weight_file[] = "T2_WEIGH.TXT";

const prog_int16_t size = 32;
typedef struct {
  volatile bool update;
  volatile int pour;
  char brewery[size];
  char name[size];
  //float weight;
} 
beer;

beer tap1;
beer tap2;


// Configure the Flow Sensor
#define FLOWSENSORPIN1 2
#define FLOWSENSORPIN2 3
// track the state of the pulse pin
volatile uint8_t lastflowpinstate1;
volatile uint8_t lastflowpinstate2;

// Uptime
unsigned long time;


void status(WebServer &server, WebServer::ConnectionType type, char *, bool) {
  // Status headers
  server.httpSuccess("application/json");

  if (type == WebServer::GET) {
    Serial.print(F("processing GET request from: "));
    Serial.println();

    P(beer_msg_open) = "{"
      "  \"beers\": [";

    P(beer_msg_tap) =
      "      {"
      "        \"tap\": \"";

    P(beer_msg_brewery) = "\","
      "        \"brewery\": \"";

    P(beer_msg_name) = "\","
      "        \"name\": \"";

    P(beer_msg_pour) = "\","
      "        \"pour\": \"";

    P(beer_msg_pour_end) = "\""
      "      }";

    P(beer_msg_close) = 
      "  ]"
      "}";

    // Print the JSON msg
    server.printP(beer_msg_open);

    server.printP(beer_msg_tap);
    server.print("1");
    server.printP(beer_msg_brewery);
    server.print(tap1.brewery);
    server.printP(beer_msg_name);
    server.print(tap1.name);
    server.printP(beer_msg_pour);
    server.print(convert_pulse_to_liter(&(tap1.pour)));
    //server.print(tap1.pour);
    server.printP(beer_msg_pour_end);
    server.print(",");

    server.printP(beer_msg_tap);
    server.print("2");
    server.printP(beer_msg_brewery);
    server.print(tap2.brewery);
    server.printP(beer_msg_name);
    server.print(tap2.name);
    server.printP(beer_msg_pour);
    server.print(convert_pulse_to_liter(&(tap2.pour)));
    //server.print(tap2.pour);
    server.printP(beer_msg_pour_end);

    server.printP(beer_msg_close);
  }
}


void post(WebServer &server, WebServer::ConnectionType type, char *, bool) { 
  if (type == WebServer::POST) {
    Serial.println(F("processing POST request"));
    bool repeat;
    char name[16], value[32];
    int tap = 0;
    int stringsize = 32;

    // Do we need to write the data out to the SD Card
    bool save = 0;
    while (repeat) {
      repeat = server.readPOSTparam(name, 16, value, 32);
      if (strcmp(name, "tap") == 0) {
        // set tap
        Serial.print(F("post: tap: \""));
        Serial.print(value);
        Serial.println(F("\""));
        tap = atoi(value);
        if((tap==1) || (tap==2)) {
          // Continue..
        } 
        else {
          Serial.print(F("Tap not set to 1 or 2: "));
          Serial.println(value);
          return;
        }
      }
      else if (strcmp(name, "brewery") == 0) {
        if (tap==1) {
          // set brewery
          Serial.print(F("post: brewery: "));
          Serial.println(value);
          strncpy(tap1.brewery, value, stringsize - 1);
          tap1.brewery[stringsize - 1] = '\0';
          Serial.print(F("Struct brewery: "));
          Serial.println(tap1.brewery);
          save = 1;
        }
        else if (tap==2) {
          // set brewery
          Serial.print(F("post: brewery: "));
          Serial.println(value);
          strncpy(tap2.brewery, value, stringsize - 1);
          tap2.brewery[stringsize - 1] = '\0';
          Serial.print(F("Struct brewery: "));
          Serial.println(tap2.brewery);
          save = 1;
        }
        else {
          Serial.print(F("Unknown tap for brewery name: "));
          Serial.println(value);
        }
      }

      // Set the Name
      else if (strcmp(name, "name") == 0) {
        if (tap==1) {
          // set name
          Serial.print(F("post: name: "));
          Serial.println(value);
          strncpy(tap1.name, value, stringsize - 1);
          tap1.name[stringsize - 1] = '\0';
          Serial.print(F("Struct name: "));
          Serial.println(tap1.name);
          save = 1;
        }
        else if (tap==2) {
          // set name
          Serial.print(F("post: name: "));
          Serial.println(value);
          strncpy(tap2.name, value, stringsize - 1);
          tap2.name[stringsize - 1] = '\0';
          Serial.print(F("Struct name: "));
          Serial.println(tap2.name);
          save = 1;
        }
        else {
          Serial.print(F("Unknown tap for beer name: "));
          Serial.println(value);
        }
      }

      // Reset the pour numbers
      else if (strcmp(name, "reset") == 0) {
        if(tap == 1) {
          Serial.print(F("reset tap1"));
          tap1.pour = 0;
          save = 1;
        }
        if(tap == 2) {
          Serial.print(F("reset tap2"));
          tap2.pour = 0;
          save = 1;
        }
      }
      
      // Force a save
      else if (strcmp(name, "save") == 0) {
        save_all();
      }
    }

    // Write out to SD Card
    if(save == 1) {
      // Write out tap1 name
      if(tap1.name) {
        if(write_file(t1_name_file, tap1.name, NULL) != 0) {
          // Fail
          Serial.println(F("Error writing out tap1 name"));
        }
        else {
          // Success
          Serial.println(F("Success writing out tap1 name"));
        }
      }
      // Write out tap2 name
      if(tap2.name) {
        if(write_file(t2_name_file, tap2.name, NULL) != 0) {
          // Fail
          Serial.println(F("Error writing out tap1 name"));
        }
        else {
          // Success
          Serial.println(F("Success writing out tap1 name"));
        }
      }

      // Write out tap1 brewery
      if(tap1.brewery) {
        if(write_file(t1_brew_file, tap1.brewery, NULL) != 0) {
          // Fail
          Serial.println(F("Error writing out tap1 brewery"));
        }
        else {
          // Success
          Serial.println(F("Success writing out tap1 brewery"));
        }
      }
      // Write out tap2 brewery
      if(tap2.brewery) {
        if(write_file(t2_brew_file, tap2.brewery, NULL) != 0) {
          // Fail
          Serial.println(F("Error writing out tap1 brewery"));
        }
        else {
          // Success
          Serial.println(F("Success writing out tap1 brewery"));
        }
      }

      // Write out tap1 pour
      if(write_file(t1_pour_file, NULL, &tap1.pour) != 0) {
        // Error writing
        Serial.println(F("Error writing tap1 pour data"));
      }
      else {
        // Success
        Serial.println(F("Success writing tap1 pour data"));
      }
      // Write out tap2 pour
      if(write_file(t2_pour_file, NULL, &tap2.pour) != 0) {
        // Error writing
        Serial.println(F("Error writing tap2 pour data"));
      }
      else {
        // Success
        Serial.println(F("Success writing tap2 pour data"));
      }


    }
  }
}


int read_file(const char *filename, char *location, volatile int *pour) {

  // open file
  SdFile file;
  file.open(filename, O_READ);

  if (!file.isOpen()) {
    Serial.print(F("read_file(): Unable to open file: "));
    Serial.println(filename);
    return 1;
  }

  Serial.println(F("start reading file"));
  Serial.print(":");

  char temp[size];

  if(file.fgets(temp, sizeof(char[size]))) {
    Serial.print(temp);
    Serial.println(F(":"));
    Serial.println(F("done reading file"));
    if(pour == NULL) {
      strncpy(location, temp, size);
    }
    else {
      *pour = atoi(temp);
    }
  }
  else {
    Serial.println(F("Failed to read"));
  }


  file.close();

  return 0;
} // read_file()


void read_in_existing() {
  // Tap 1 / Name
  Serial.print(F("Reading in tap1.name"));
  Serial.println();
  if(read_file(t1_name_file, tap1.name, NULL) != 0) {
    // Handle error condition
    Serial.println(F("Error reading in name 1"));
  }
  else {
    Serial.print(F("Contents: \""));
    Serial.print(tap1.name);
    Serial.print(F("\""));
    Serial.println();
  }

  // Tap 2 / Name
  Serial.print(F("Reading in tap2.name"));
  Serial.println();
  if(read_file(t2_name_file, tap2.name, NULL) != 0) {
    // Handle error condition
    Serial.println(F("Error reading in name 2"));
  }
  else {
    Serial.print(F("Contents: \""));
    Serial.print(tap2.name);
    Serial.print(F("\""));
    Serial.println();
  }

  // Tap 1 / Brewery
  Serial.print(F("Reading in tap1.brewery"));
  Serial.println();
  if(read_file(t1_brew_file, tap1.brewery, NULL) != 0) {
    // Handle error condition
    Serial.println(F("Error reading in brewery 1"));
  }
  else {
    Serial.print(F("Contents: \""));
    Serial.print(tap1.brewery);
    Serial.print(F("\""));
    Serial.println();
  }

  // Tap 2 / Brewery
  Serial.print(F("Reading in tap2.brewery"));
  Serial.println();
  if(read_file(t2_brew_file, tap2.brewery, NULL) != 0) {
    // Handle error condition
    Serial.println(F("Error reading in brewery 2"));
  }
  else {
    Serial.print(F("Contents: \""));
    Serial.print(tap2.brewery);
    Serial.print(F("\""));
    Serial.println();
  }

  // Tap 1 / Pour
  Serial.print(F("Reading in tap1.pour"));
  Serial.println();
  if(read_file(t1_pour_file, NULL, &tap1.pour) != 0) {
    // Handle error condition
    Serial.println(F("Error reading in pour 1"));
  }
  else {
    Serial.print(F("Contents: \""));
    Serial.print(tap1.pour);
    Serial.print(F("\""));
    Serial.println();
  }

  // Tap 2 / Pour
  Serial.print(F("Reading in tap2.pour"));
  Serial.println();
  if(read_file(t2_pour_file, NULL, &tap2.pour) != 0) {
    // Handle error condition
    Serial.println(F("Error reading in pour 2"));
  }
  else {
    Serial.print(F("Contents: \""));
    Serial.print(tap2.pour);
    Serial.print(F("\""));
    Serial.println();
  }

}


int write_file(const char *filename, char *location, volatile int *pour) {
  int ret = 0;

  // open file for writing
  SdFile wrfile(filename, O_WRITE | O_CREAT);
  if (!wrfile.isOpen()) {
    Serial.print(F("write_file(): Unable to open file: "));
    Serial.println(filename);
    return 1;
  }
  else {
    Serial.print(F("write_file(): File successfully opened: "));
    Serial.println(filename);
  }

  Serial.println(F("start writing file"));

  char temp[size];
  if(pour != NULL) {
    itoa(*pour, temp, 10);
    ret = wrfile.write(temp, '\n');
  }
  else {
    strncpy(temp, location, size);
    ret = wrfile.write(temp);
  }
  
  if (ret > 0) {
    Serial.print(F("done  writing file. Wrote: "));
    Serial.print(ret);
    Serial.print(F(" characters to file: "));
    Serial.println(filename);
  } 
  else {
    Serial.print(F("An error occured writing file: "));
    Serial.println(filename);
  }

  wrfile.close();
} // write_file()


void save_all() {
  // Write out pour data
  // Tap 1 - Pour
  // XXX: Testing
  tap1.pour = 300;
  if(write_file(t1_pour_file, NULL, &tap1.pour) != 0) {
    // Error writing
    Serial.println(F("Error writing tap1 pour data"));
  }
  else {
    // Success
    Serial.println(F("Success writing tap1 pour data"));
  }

  // Tap 2 - Pour
  if(write_file(t2_pour_file, NULL, &tap2.pour) != 0) {
    // Error writing
    Serial.println(F("Error writing tap2 pour data"));
  }
  else {
    // Success
    Serial.println(F("Success writing tap2 pour data"));
  }
}


void write_pour() {
  if(millis() - time > 300000) {

    // Check tap1 pour
    if(tap1.update == 1) {
      if(write_file(t1_pour_file, NULL, &tap1.pour) != 0) {
        // Error writing
        Serial.println(F("Error writing tap1 pour data"));
      }
      else {
        // Success
        Serial.println(F("Success writing tap1 pour data"));
      }
    }

    // Check tap2 pour
    if(tap2.update == 1) {
      if(write_file(t2_pour_file, NULL, &tap2.pour) != 0) {
        // Error writing
        Serial.println(F("Error writing tap2 pour data"));
      }
      else {
        // Success
        Serial.println(F("Success writing tap2 pour data"));
      }
    }

    time = millis();

  }
}


void ls() {
  Serial.println(F("Files found (name, date, time, size):"));
  file.ls(LS_R | LS_DATE | LS_SIZE);
  Serial.println();
}


void freemem() {
  Serial.print(F("FreeRAM: "));
  Serial.println(FreeRam());
}


void tap1_interrupt() {
  tap1.pour++;
  tap1.update = 1;
}


void tap2_interrupt() {
  tap2.pour++;
  tap2.update = 1;
}


int convert_pulse_to_liter(volatile int *pulse) {
  float amount;
  amount = *pulse * 0.00225;
  return amount;
}


void lcd_update() {
  // Update LCD
  // Line 1
  //  lcd.clear();
  //  lcd.setCursor(0, 0);  // col, row
  //  lcd.print("Tap1: ");
  //float amount1 = tap1.pour * .00225;
  //  lcd.print(convert_pulse_to_liter(&(tap1.pour)));
  //  lcd.print("Liters");
  // Line 2
  //  lcd.setCursor(0, 1);
  //  lcd.autoscroll();
  //  lcd.print(tap1.brewery);
  //  lcd.print(": ");
  //  lcd.print(tap1.name);
  // Line 3
  //  lcd.setCursor(0, 3);  // col, row
  //  lcd.print("Tap2: ");
  //  lcd.print(convert_pulse_to_liter(&(tap2.pour)));
  // Line 4
  //  lcd.setCursor(0, 4);
  //  lcd.autoscroll();
  //  lcd.print(tap2.brewery);
  //  lcd.print(": ");
  //  lcd.print(tap2.name);
}


void setup() {

  // Initialize
  tap1.pour = 0;
  tap1.update = 0;
  tap2.pour = 0;
  tap2.update = 0;

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(0, tap1_interrupt, FALLING);
  attachInterrupt(1, tap2_interrupt, FALLING);

  Serial.begin(9600);
  Serial.println();
  Serial.println(F("Starting up.."));
  freemem();

  //  lcd.begin(20,4);

  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with breadboards
  Serial.print(F("Initializing the SD Card.."));
  if (!file.begin(SPI_HALF_SPEED, SdChipSelect)) {
    Serial.print(F("SD init failed: "));
    file.initErrorHalt();
  }
  Serial.println(F(". done"));

  freemem();

  // Initialize the Web Server
  Serial.println();
  Serial.print(F("Initializing the Web server.."));
  Ethernet.begin(mac, ip);
  webserver.setDefaultCommand(&status);
  webserver.addCommand("post", &post);
  webserver.begin();
  Serial.println(F(". done"));

  freemem();

  //write_file(t1_name_file, "FOO BAR BAZ\nlorem ipsum");

  //read_file(t1_name_file);
  Serial.println(F("Running ls"));
  //ls();
  Serial.println(F("End ls"));

  //read_file(t1_name_file);
  Serial.println(F("Done"));

  // Look for existing files and open them
  read_in_existing();

  time = millis();

} // setup()


void loop() {
  char buff[64];
  int len = 64;

  // process a connection
  webserver.processConnection(buff, &len);

  // Check if we need to write out pour data
  write_pour();

} // loop()









