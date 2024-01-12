#define VPB   22
#define RDY   24
#define PHI1O 26
#define IRQB  28
#define MLB   30
#define NMIB  32
#define SYNC  34
#define A0    36
#define A1    38
#define A2    40
#define A3    42
#define A4    44
#define A5    46
#define A6    48
#define A7    50
#define A8    52
#define A9    69
#define A10   67
#define A11   66
#define A12   68
#define A13   53
#define A14   51
#define A15   49
#define D7    47
#define D6    45
#define D5    43
#define D4    41
#define D3    39
#define D2    37
#define D1    35
#define D0    33
#define RWB   2
#define BE    31
#define PHI2  29
#define SOB   27
#define PHI2O 25
#define RESB  23

const int ADDRESS_BUS[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15};
const int ADDRESS_BUS_LOW[] = {A0, A1, A2, A3, A4, A5, A6, A7};
const int ADDRESS_BUS_HIGH[] = {A8, A9, A10, A11, A12, A13, A14, A15};
const int DATA_BUS[] = {D0, D1, D2, D3, D4, D5, D6, D7};

int hz = 5;

byte data = 0x00;

byte address[] = {0x00, 0x00};

byte ram[0x4FF];
byte pmem[0xFFF];

byte program[] = {
  0x58,
  0xA9, 0x01,
  0x1A,
  0xD0, 0xFD,
  0x8D, 0x00, 0x02,
  0x40
  };

bool _VPB   = false;
bool _RDY   = false;
bool _PHI1O = false;
bool _IRQB  = false;
bool _MLB   = false;
bool _NMIB  = false;
bool _SYNC  = false;
volatile bool _RWB   = false; // vars updated in interupts should be volatile
bool _BE    = false;
bool _PHI2  = false;
bool _SOB   = false;
bool _PHI2O = false;
bool _RESB  = false;

bool pause = false;

static byte toByte(bool bits[8]) {
  byte output = 0x00;
  for (int i = 7; i >= 0; i--) {
    output = (output << 1) + (bits[i] ? 1 : 0);
  }
  return output;
}

static void toBits(byte inputByte, bool* returnBits) {
    for (int i = 0; i < 8; i++) {
        returnBits[7-i] = (inputByte >> (7 - i)) & 0x01;
    }
}

void setup() {
  for (int i = 0; i < 0x100; i++){
    pmem[i] = 0xEA; //nop
  }

  for (int i = 0; i < 11; i++) {
    pmem[i] = program[i];
  }
  
  pinMode(VPB, INPUT);
  pinMode(RDY, OUTPUT);
  pinMode(PHI1O, INPUT);
  pinMode(IRQB, OUTPUT);
  pinMode(MLB, INPUT);
  pinMode(SYNC, INPUT);

  for (int i = 0; i < 16; i++) {
    pinMode(ADDRESS_BUS[i], INPUT);
  }

  pinMode(RWB, INPUT);
  pinMode(BE, OUTPUT);
  pinMode(PHI2, OUTPUT);
  pinMode(SOB, OUTPUT);
  pinMode(PHI2O, INPUT);
  pinMode(RESB, OUTPUT);

  handleRWB();

  attachInterrupt(digitalPinToInterrupt(RWB), handleRWB, CHANGE);

  Serial.begin(115200);
  Serial.setTimeout(10);
  
  startup();
}

void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil("\n");
    input.replace("\n", "");
    Serial.println(input);

    if (input.equalsIgnoreCase("irq")) {
      sendIRQ();
      Serial.println("irq sent");
    }

    else if (input.equalsIgnoreCase("nmi")) {
      sendNMI();
      Serial.println("nmi sent");
    }
    
    else if (input.equalsIgnoreCase("res")) {
      reset();
    }
    
    else if (input.equalsIgnoreCase("pause")) {
      pause = true;
    }
    
    else if (input.equalsIgnoreCase("step")) {
      cycle();
      pause = true;
    }
    
    else if (input.startsWith("run ")) {
      int steps = input.substring(4).toInt();
      pause = false;
      for (int i = 0; i < steps; i++) {
        cycle();
      }
      pause = true;
    }
    
    else if (input.equalsIgnoreCase("run")) {
      pause = false;
    }

    else if (input.startsWith("hz ")) {
      hz = input.substring(3).toInt();
    }
    
    else if (input.startsWith("read ")) {
      Serial.println("implement read");
      Serial.println(input.substring(5));
    }

    else if (input.startsWith("write ")) {
      Serial.println("implement write");
      Serial.println(input.substring(6));
    }

    else if (input.startsWith("io ")) {
      Serial.println("implement io");
      Serial.println(input.substring(3));
      // write data + irq
    }
  }
}

void loop() {
  handleSerialInput();
  if (!pause) {
    cycle();
  }
}

void setDataPinInputMode() {
  if (!_RWB) {
    for (int i = 0; i < 8; i++) { 
      pinMode(DATA_BUS[i], INPUT);
    }
    writeDataBusToRAM();
  } else {
    for (int i = 0; i < 8; i++) { 
      pinMode(DATA_BUS[i], OUTPUT);
      readAddress();
      setDataBus();
    }
  }
}

void writeDataBusToRAM() {
  int addressFull = (address[0] << 8) + address[1];
  if (addressFull < 0x1000) {
    bool dataBus[8];
    readBus(DATA_BUS, dataBus);
    byte dataBusByte = toByte(dataBus);
    Serial.println(ram[addressFull], HEX);
    ram[addressFull] = dataBusByte;
      
    char buff[120];
    sprintf(buff, "    \t    \t%02X>%02X%02X", dataBusByte, address[0], address[1]);
    Serial.println(buff);
    Serial.println(ram[addressFull], HEX);
  }
}

void readBus(int pins[8], bool* returnBits) {
  for (int i = 0; i < 8; i++) {
    returnBits[i] = digitalRead(pins[i]);
  }
}

char fbool(bool b) {
  return b ? '+' : '-';
}

void displayStatus() {
  char buff[120];
  sprintf(buff, "CLK%c\tRWB%c\t%02X@%02X%02X", fbool(_PHI2), fbool(_RWB), data, address[0], address[1]);
  Serial.println(buff);
}

void startup(){
  // set pins to inital values
  _RDY = true;
  _IRQB = true;
  _NMIB = true;
  _BE = true;
  _PHI2 = false;
  _SOB = false;
  _RESB = false;

  handleRWB();
  
  reset();
}


void reset() {
  _RESB = false;
  cycle();
  _RESB = true;
  for (int i = 0; i < 9; i++) {
    cycle();
  }
  Serial.println("completed reset");
}

void cycle() {
  _PHI2 = true;
  delay(500.0/hz);
  updateIO();
  displayStatus();
  

  _PHI2 = false;
  delay(500.0/hz);
  updateIO();
  displayStatus();
}

void handleRWB() {
  _RWB = digitalRead(RWB);
  setDataPinInputMode();
}

void updateData() {
  int addressFull = (address[0] << 8) + address[1];
  if (addressFull == 0xFFFC) {
    data = 0x00;
  }
  else if (addressFull == 0xFFFD) {
    data = 0x40;
  } else if (addressFull == 0xFFFE) {
    data = 0x06;
  } else if (addressFull == 0xFFFF) {
    data = 0x40;
  } else if (addressFull < 0x1000) {
    data = ram[addressFull];
  } else if (addressFull >= 0x4000 && addressFull < 0x5000) {
    data = pmem[addressFull - 0x4000];
  } else {
    // data = 0xEA; // NOP  
    data = 0xCB; // WAI
  }
}

void updateIO() {
  // read
  readAddress();
  _VPB = digitalRead(VPB);
  _PHI1O = digitalRead(PHI1O);
  _MLB = digitalRead(MLB);
  _SYNC = digitalRead(SYNC);
  _RWB = digitalRead(RWB);
  _PHI2O = digitalRead(PHI2O);
  
  // write
  digitalWrite(RDY, _RDY);
  digitalWrite(IRQB, _IRQB);
  digitalWrite(NMIB, _NMIB);
  digitalWrite(BE, _BE);
  digitalWrite(PHI2, _PHI2);
  digitalWrite(SOB, _SOB);
  digitalWrite(RESB, _RESB);
  
  setDataBus();
  if (!_RWB) {
    writeDataBusToRAM();
  }
  
}

void setDataBus() {
  bool bits[8];
  toBits(data, bits);
  
  if (_RWB) {
    for (int i = 0; i < 8; i++) {
      digitalWrite(DATA_BUS[i], bits[i]);
    }
  }
}

void readAddress() {
  bool highBits[8];
  readBus(ADDRESS_BUS_HIGH, highBits);
  address[0] = toByte(highBits);
  
  bool lowBits[8];
  readBus(ADDRESS_BUS_LOW, lowBits);
  address[1] = toByte(lowBits);

  updateData();
}

void sendIRQ() {
  _IRQB = false;
  cycle();
  cycle();
  cycle();
  
  _IRQB = true;
}

void sendNMI() {
  _NMIB = false;
  cycle();
  cycle();
  cycle();
  _NMIB = true;
}
