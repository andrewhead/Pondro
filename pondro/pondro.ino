/*
 Run Pondro!
 
 created 23 Dec. 2014
 modified 24 Dec. 2014
 by Andrew Head
 */

// Pin constants
const int soundPin = A0;  // sound detector 'envelope' pin
const int buttonPin = 4;  // toggle switch
const int buzzerPin = 2;  // piezo buzzer

// State constants
enum convState {
  waiting,
  listening,
  responding
};

// Logic constants
const int TALK_AMP = 15;          // Minimum sample amplitude to be considered start of speech
const int TALK_SAMPLES = 3;       // Minimum number of samples above threshold to be considered talking
const int SILENCE_SAMPLES = 20;   // Number of samples of silence after talking heard to be considered silence
const int SHORT_RESPONSE_LEN = 20;
const char boops[] = {20, 17, 5, 4, 3, 18, 21, 25, 2, 1, 25, 26, 21, 20, 18, 23, 5, 2, 1, 1};

// Global variables
int buttonState = 0;
int amp = 0;
int silences = 0;
int talks = 0;
int responseIndex = 0;
convState state;


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
     Serial.println("Waiting");
     noTone(buzzerPin);
     if (amp >= TALK_AMP) {
        state = listening;
        silences = 0;
        talks = 1;
     }
  }
  else if (state == listening) {
     Serial.println("Listening");
     silences = (amp >= TALK_AMP) ? 0 : (silences + 1);
     talks = (amp >= TALK_AMP) ? (talks + 1) : talks;
     if (silences > SILENCE_SAMPLES) {
       state = (talks >= TALK_SAMPLES) ? responding : waiting;
     }
  }
  else if (state == responding) {
    Serial.println("Responding");
    tone(buzzerPin, (boops[responseIndex] / 256.0) * 8000);
    responseIndex++;
    // Stop response either if it's over or if Pondro has been interrupted
    if ((responseIndex >= SHORT_RESPONSE_LEN) || amp >= TALK_AMP) {
      state = waiting;
      responseIndex = 0;
    }
  }
  
  delay(100);
}

void setup() {
  Serial.begin(9600); 
  state = waiting;
}
