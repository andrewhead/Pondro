/*
 Run Pondro!
 
 created 23 Dec. 2014
 modified 24 Dec. 2014
 by Andrew Head
 */

#include <EEPROM.h>

// Pin constants
const int soundPin = A0;  // sound detector 'envelope' pin
const int buttonPin = 4;  // toggle switch
const int buzzerPin = 2;  // piezo buzzer
const int currentPin = 3;

// EEPROM constants
const int shortWriteAddr = 0;
const int longWriteAddr = 1;
const int shortCountAddr = 2;
const int longCountAddr = 3;
const int longResponseAddr = 4;     // 500 bytes reserved for long responses
const int shortResponseAddr = 504;  // 500 bytes reserved for short responses

// State constants
enum convState {
  waiting,
  listening,
  responding
};

// Logic constants
const int TALK_AMP = 20;          // Minimum sample amplitude to be considered start of speech
const int INTERRUPT_AMP = 40;     // Sample amplitude to interrupt Pondro's speech
const int TALK_SAMPLES = 4;       // Minimum number of samples above threshold to be considered talking
const int SILENCE_SAMPLES = 20;   // Number of samples of silence after talking heard to be considered silence
const int SHORT_RESPONSE_LEN = 20;
const int LONG_RESPONSE_LEN = 100;
const int MIN_RESPONSE_LEN = 5;
const int STORE_RATE = 4;         // 1 / (this number) = chance of storing term
const int SHORT_RESPONSES = 25;
const int LONG_RESPONSES = 5;
const int SAMPLE_DELAY = 100;
const int REPLAY_DELAY = 10;

// Global variables
int buttonState = 0;
int amp = 0;
int silences = 0;
int talks = 0;
int responseTime = 0;
convState state;
char *response;
char recording[LONG_RESPONSE_LEN];
int recordIndex = 0;

char shortResponses[SHORT_RESPONSES][SHORT_RESPONSE_LEN];
char longResponses[LONG_RESPONSES][LONG_RESPONSE_LEN];
int shortWriteIndex;
int longWriteIndex;
int shortWrites;
int longWrites;


void resetRecording() {
  for (int i = 0; i < LONG_RESPONSE_LEN; i++) {
    recording[i] = '\0';
  }
  recordIndex = 0;
}

void recordSample() {
  recording[recordIndex] = amp;
  // Recording will wrap around if it gets too long
  recordIndex = (recordIndex + 1) % LONG_RESPONSE_LEN;
}

void stopRecording() {
  // Blank out the samples recorded in the silent periods
  for (int i = recordIndex; i > recordIndex - SILENCE_SAMPLES; i--) {
    recording[i] = '\0';
  }
}

void storeRecording() {
  // Serial.println("Storing");
  if (strlen(recording) <= SHORT_RESPONSE_LEN) {
    for (int i = 0; i < SHORT_RESPONSE_LEN; i++) {
      shortResponses[shortWriteIndex][i] = recording[i]; 
    }
    saveResponse(recording, shortResponseAddr + (shortWriteIndex * SHORT_RESPONSE_LEN), SHORT_RESPONSE_LEN);
    shortWriteIndex = (shortWriteIndex + 1) % SHORT_RESPONSES;
    shortWrites = min(shortWrites + 1, SHORT_RESPONSES);
  }
  else {
    for (int i = 0; i < LONG_RESPONSE_LEN; i++) {
      longResponses[longWriteIndex][i] = recording[i]; 
    }
    saveResponse(recording, longResponseAddr + (longWriteIndex * LONG_RESPONSE_LEN), LONG_RESPONSE_LEN);
    longWriteIndex = (longWriteIndex + 1) % LONG_RESPONSES;
    longWrites = min(longWrites + 1, LONG_RESPONSES);
  }
  saveState();
}

void processRecording() {
  // Decide whether to save the recording to Pondro's memory banks
  if (strlen(recording) < MIN_RESPONSE_LEN) {
    return; 
  }
  else {
    // Store first two recordings of both lengths
    if ((strlen(recording) > SHORT_RESPONSE_LEN && longWrites < 2) ||
        (strlen(recording) > MIN_RESPONSE_LEN && shortWrites < 2)) {
      storeRecording();
    }
    // After that, only store recordings randomly
    else if (random(STORE_RATE) == 0) {
      storeRecording();
    }
  }
}

void pickResponse() {
  // Respond to long requests with long responses
  // Choose randomly from available phrases
  if (strlen(recording) > SHORT_RESPONSE_LEN) {
    response = longResponses[random(longWrites)];
  } else {
    response = shortResponses[random(shortWrites)];    
  }
  // Serial.println(strlen(response));
}

void wait() {
  noTone(buzzerPin);
  delay(1000);
}

void loop() {
  
  buttonState = digitalRead(buttonPin);
  if (!buttonState) {
     wait();
    return; 
  }
    
  // Read in and threshold the amplitude
  amp = analogRead(soundPin);
  amp = amp > 255 ? 255 : amp;
  
  // Serial.print("amp = " );
  // Serial.println(amp);
  
  if (state == waiting) {
     // Serial.println("Waiting");
     noTone(buzzerPin);
     if (amp >= TALK_AMP) {
        state = listening;
        silences = 0;
        talks = 1;
        recordSample();
     }
    delay(SAMPLE_DELAY);
  }
  else if (state == listening) {
     // Serial.println("Listening");
     recordSample();
     silences = (amp >= TALK_AMP) ? 0 : (silences + 1);
     talks = (amp >= TALK_AMP) ? (talks + 1) : talks;
     if (silences > SILENCE_SAMPLES) {
       stopRecording();
       processRecording();
       pickResponse();
       responseTime = 0;
       state = (talks >= TALK_SAMPLES) ? responding : waiting;
     }  
    delay(SAMPLE_DELAY);
  }
  else if (state == responding) {
    int responseIndex = responseTime / SAMPLE_DELAY;
    // Serial.println(responseTime);
    // Serial.print(responseIndex);
    char boopBefore = response[responseIndex];
    char boopAfter = response[responseIndex + 1];
    int boop = (boopBefore + ((responseTime % SAMPLE_DELAY) / (float) SAMPLE_DELAY) * (boopAfter - boopBefore));
    int freq = 300 + (boop / 256.0) * 5000;
    // Stop response either if it's over or if Pondro has been interrupted
    if (boopBefore == '\0' || responseIndex == LONG_RESPONSE_LEN - 2 || amp >= INTERRUPT_AMP) {
      state = waiting;
      responseTime = 0;
      resetRecording();
    }
    if (boop > 8) {
      tone(buzzerPin, freq);
    } else {
      noTone(buzzerPin); 
    }
    responseTime += REPLAY_DELAY;
    delay(REPLAY_DELAY);
  }
}

int saveResponse(char* response, int addr, int length) {
  for (int i = 0; i < length; i++) {
    EEPROM.write(addr + i, response[i]);   
  }
  return 0;
}

void saveState() {
  EEPROM.write(shortWriteAddr, shortWriteIndex);
  EEPROM.write(shortCountAddr, shortWrites);
  EEPROM.write(longWriteAddr, longWriteIndex);
  EEPROM.write(longCountAddr, longWrites);
}

void loadState() {
  shortWriteIndex = EEPROM.read(shortWriteAddr) != 255 ? EEPROM.read(shortWriteAddr) : 0;
  longWriteIndex = EEPROM.read(longWriteAddr) != 255 ? EEPROM.read(longWriteAddr) : 0;
  shortWrites = EEPROM.read(shortCountAddr) != 255 ? EEPROM.read(shortCountAddr) : 0;
  longWrites = EEPROM.read(longCountAddr) != 255 ? EEPROM.read(longCountAddr) : 0;
  for (int i = 0; i < shortWrites; i++) {
    for (int j = 0; j < SHORT_RESPONSE_LEN; j++) {
      shortResponses[i][j] = EEPROM.read(shortResponseAddr + (i * SHORT_RESPONSE_LEN) + j);
    }
  }
  for (int i = 0; i < longWrites; i++) {
    for (int j = 0; j < LONG_RESPONSE_LEN; j++) {
      longResponses[i][j] = EEPROM.read(longResponseAddr + (i * LONG_RESPONSE_LEN) + j);
    }
  }
}

void setup() {
  Serial.begin(9600); 
  loadState();
  resetRecording();
  state = waiting;
  digitalWrite(currentPin, 1);
}
