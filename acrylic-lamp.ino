#include <EEPROM.h>
#include "FastLED.h"

#define DATA_PIN 5
#define BUTTON_PIN 2
#define NUM_LEDS 15
#define NUM_PERSEGMENT 5
#define MAX_COLOR 6
#define MAX_BRIGHTNESS 255
#define MAX_FADE_DURATION 2

#define UPDATES_PER_SECOND 100

CRGB leds[NUM_LEDS];

struct Color
{
	byte red;
	byte green;
	byte blue;
};

struct Data
{
	Color c1;
	Color c2;
	Color c3;
	byte dynamic_mode;
	byte random_mode;
	byte random_duration;
	byte brightness;
};


Color defaultColor[MAX_COLOR];
Color currentColor[3];
Color beforeRandomColor[3];

byte index_color = 0;
byte prev_index_color = 0;

byte random_mode = false;
byte random_duration = 10;
bool random_standBy = true;
Color random_colorDirection[3];

// 0 - 100%
byte brightness = 8;

byte buttonState = 0;
byte prevButtonState = 0;


bool dynamic_mode = false;
CRGBPalette16 currentPalette;
TBlendType    currentBlending;

extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;


long currentMillis = 0;
long prev_currentMillis = 2000;
long random_prevCurrentMillis = 2000;
long random_prevFadeColorMillis = 2000;



byte tag, b1, b2, b3;


void setup()
{
	delay( 2000 );

	FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
	pinMode(BUTTON_PIN, INPUT);
	
	defaultColorSetup();

	currentPalette = RainbowColors_p;
    currentBlending = LINEARBLEND;

    Serial.begin(9600);

    Load();
}

void loop()
{

	buttonInput();
	bluetoothRead();

	if (random_mode && !dynamic_mode)
		randomMode();
	else if (dynamic_mode)
		dynamicMode();
}

// +++++++++++++++++++++++++++++++++++++++++++++++
// +++++++++
// +++++++++++++++++++++++++++++++++++++++++++++++

void Save()
{
	Data D;

	D.random_mode = random_mode;
	D.dynamic_mode = dynamic_mode;

	if (dynamic_mode)
	{
		D.c1 = beforeRandomColor[0];
		D.c2 = beforeRandomColor[1];
		D.c3 = beforeRandomColor[2];
	}
	else
	{
		if (random_mode)
		{
			D.c1 = beforeRandomColor[0];
			D.c2 = beforeRandomColor[1];
			D.c3 = beforeRandomColor[2];
		}
		else
		{
			D.c1 = currentColor[0];
			D.c2 = currentColor[1];
			D.c3 = currentColor[2];
		}
	}

	D.random_duration = random_duration;
	D.brightness = brightness;

	EEPROM.put(0, D);
}

void Load()
{
	Data D;
	EEPROM.get(0, D);

	random_mode = D.random_mode;
	random_duration = D.random_duration;
	
	brightness = D.brightness;
	setBrightness(brightness);

	dynamic_mode = D.dynamic_mode;

	if (dynamic_mode)
	{
		beforeRandomColor[0] = D.c1;
		beforeRandomColor[1] = D.c2;
		beforeRandomColor[2] = D.c3;
		if (random_mode)
		{
			random_standBy = true;
			random_prevCurrentMillis = millis();
		}
	}
	else
	{
		if (random_mode)
		{
			beforeRandomColor[0] = D.c1;
			beforeRandomColor[1] = D.c2;
			beforeRandomColor[2] = D.c3;
			
			random_standBy = true;
			random_prevCurrentMillis = millis();
			setAllSegmentColor(defaultColor[0]);
		}
		else
		{
			setSegmentColor(0, D.c1);
			setSegmentColor(1, D.c2);
			setSegmentColor(2, D.c3);
		}
	}
	
}

void buttonInput()
{
	buttonState = digitalRead(BUTTON_PIN);
	if (buttonState == HIGH && buttonState != prevButtonState)
	{
		dynamic_mode = false;
		random_mode = false;

		index_color++;

		if (index_color >= MAX_COLOR)
			index_color = 0;


		setAllSegmentColor(defaultColor[index_color]);
	}
	prevButtonState = buttonState;
}



void bluetoothRead()
{
	if (Serial.available() >= 4)
	{
		tag = Serial.read(); delay(2);
		b1 = Serial.read(); delay(2);
		b2 = Serial.read(); delay(2);
		b3 = Serial.read(); delay(2);

		if (tag == 0)
		{
			random_mode = b1;
			switchToRandom(random_mode);
		}
		else if (tag == 1)
		{
			random_duration = b1;
		}
		else if (tag == 2 || tag == 3)
		{
			Color c = ColorCTOR(b1, b2, b3);
			setAllSegmentColor(c);
		}
		else if (tag == 4)
		{
			dynamic_mode = b1;
			switchToDynamic(dynamic_mode);
		}
		else if (tag == 5 || tag == 6 || tag == 7)
		{
			Color c = ColorCTOR(b1, b2, b3);
			setSegmentColor(tag-5, c);
		}
		else if (tag == 8)
		{
			setBrightness(b1);
		}
		else if (tag == 9)
		{
			Save();
		}
	}
}

void switchToRandom(byte x)
{
	if (x == 1)
	{
		dynamic_mode = false;

		setBeforeRandomColor();
		random_standBy = true;
		setAllSegmentColor(defaultColor[0]);
		random_prevCurrentMillis = millis();
	}
	else
	{
		setSegmentColor(0, beforeRandomColor[0]);
		setSegmentColor(1, beforeRandomColor[1]);
		setSegmentColor(2, beforeRandomColor[2]);
	}
}

void switchToDynamic(byte x)
{
	if (x != 1)
	{
		if (!random_mode)
		{
			setSegmentColor(0, beforeRandomColor[0]);
			setSegmentColor(1, beforeRandomColor[1]);
			setSegmentColor(2, beforeRandomColor[2]);
		}
	}
	else
	{
		if (!random_mode)
			setBeforeRandomColor();
	}
}


void randomMode()
{
	currentMillis = millis();
	if (currentMillis - random_prevCurrentMillis >= (random_duration * 1000) && random_standBy)
	{
		random_prevCurrentMillis = currentMillis;
		changeColor();

		random_standBy = false;
	}


	if (!random_standBy)
	{
		currentMillis = millis();
		if (currentMillis - random_prevFadeColorMillis >= MAX_FADE_DURATION * 1000 / 200)
		{
			random_prevFadeColorMillis = currentMillis;
			fadeToColor(0, random_colorDirection[0]);
			fadeToColor(1, random_colorDirection[1]);
			fadeToColor(2, random_colorDirection[2]);

			if (isEQColor(currentColor[0], random_colorDirection[0]) &&
			isEQColor(currentColor[1], random_colorDirection[1]) &&
			isEQColor(currentColor[2], random_colorDirection[2]))
			{
				random_standBy = true;
			}
		}
	}
}

void changeColor()
{
	random_colorDirection[0] = randomColor();
	random_colorDirection[1] = randomColor();
	random_colorDirection[2] = randomColor();
}

Color randomColor()
{
	randomSeed(analogRead(A0));
	Color c;
	c.red = random(50, 256);
	c.green = random(50, 256);
	c.blue = random(50, 256);

	return c;
}


void fadeToColor(int segment, Color c2)
{
	Color c1 = currentColor[segment];

	if (c2.red != c1.red)
	{
		int dir = c2.red - c1.red;
		c1.red += (dir / abs(dir));
	}

	if (c2.green != c1.green)
	{
		int dir = c2.green - c1.green;
		c1.green += (dir / abs(dir));
	}

	if (c2.blue != c1.blue)
	{
		int dir = c2.blue - c1.blue;
		c1.blue += (dir / abs(dir));
	}

	setSegmentColor(segment, c1);
	currentColor[segment] = c1;
}


void setBeforeRandomColor()
{
	beforeRandomColor[0] = currentColor[0];
	beforeRandomColor[1] = currentColor[1];
	beforeRandomColor[2] = currentColor[2];
}



void dynamicMode()
{
	ChangePalettePeriodically();
    
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed */
    
    FillLEDsFromPaletteColors( startIndex);
    
    FastLED.show();
    FastLED.delay(1000 / UPDATES_PER_SECOND);
}



void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;
    
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;
    }
}




void ChangePalettePeriodically()
{
    uint8_t secondHand = (millis() / 1000) % 60;
    static uint8_t lastSecond = 99;
    
    if( lastSecond != secondHand) {
        lastSecond = secondHand;
        if( secondHand ==  0)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
        if( secondHand == 10)  { currentPalette = RainbowStripeColors_p;   currentBlending = NOBLEND;  }
        if( secondHand == 15)  { currentPalette = RainbowStripeColors_p;   currentBlending = LINEARBLEND; }
        if( secondHand == 20)  { SetupPurpleAndGreenPalette();             currentBlending = LINEARBLEND; }
        if( secondHand == 25)  { SetupTotallyRandomPalette();              currentBlending = LINEARBLEND; }
        if( secondHand == 30)  { SetupBlackAndWhiteStripedPalette();       currentBlending = NOBLEND; }
        if( secondHand == 35)  { SetupBlackAndWhiteStripedPalette();       currentBlending = LINEARBLEND; }
        if( secondHand == 40)  { currentPalette = CloudColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 45)  { currentPalette = PartyColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 50)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = NOBLEND;  }
        if( secondHand == 55)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = LINEARBLEND; }
    }
}


void SetupTotallyRandomPalette()
{
    for( int i = 0; i < 16; i++) {
        currentPalette[i] = CHSV( random8(), 255, random8());
    }
}




void SetupBlackAndWhiteStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid( currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
    
}



void SetupPurpleAndGreenPalette()
{
    CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    
    currentPalette = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
}


const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM =
{
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Red,
    CRGB::Gray,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Blue,
    CRGB::Black,
    CRGB::Black
};







// ***********************************
// ******
// ***********************************

void setAllSegmentColor(Color c)
{
	setSegmentColor(0, c);
	setSegmentColor(1, c);
	setSegmentColor(2, c);
	
	showStrip();

	currentColor[0] = c;
	currentColor[1] = c;
	currentColor[2] = c;
}

void setSegmentColor(int segment, Color c)
{
	for(int i=0; i<NUM_PERSEGMENT; i++)
		setPixel(segment*NUM_PERSEGMENT + i, c);

	showStrip();

	currentColor[segment] = c;
}


// DEFAULT COLOR
void defaultColorSetup()
{
	defaultColor[0] = ColorCTOR(51, 51, 255);
	defaultColor[1] = ColorCTOR(255, 102, 204);
	defaultColor[2] = ColorCTOR(0, 255, 0);
	defaultColor[3] = ColorCTOR(255, 255, 0);
	defaultColor[4] = ColorCTOR(255, 128, 0);
	defaultColor[5] = ColorCTOR(0, 255, 255);
}

Color ColorCTOR(byte r, byte g, byte b)
{
	Color c;
	c.red = r;
	c.green = g;
	c.blue = b;

	return c;
}

bool isEQColor(Color c1, Color c2)
{
	return c1.red == c2.red && c1.green == c2.green && c1.blue == c2.blue;
}


// ***************************************
// ** FastLed/NeoPixel Common Functions **
// ***************************************

void setBrightness(byte b)
{
	FastLED.setBrightness(((float)b/100) * MAX_BRIGHTNESS);
	showStrip();
	brightness = b;
}


// Apply LED color changes
void showStrip() 
{
 #ifdef ADAFRUIT_NEOPIXEL_H 
   // NeoPixel
   strip.show();
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   FastLED.show();
 #endif
}

// Set a LED color (not yet visible)
void setPixel(int Pixel, Color c) 
{
 #ifdef ADAFRUIT_NEOPIXEL_H 
   // NeoPixel
   strip.setPixelColor(Pixel, strip.Color(c.red, c.green, c.blue));
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H 
   // FastLED
   leds[Pixel].r = c.red;
   leds[Pixel].g = c.green;
   leds[Pixel].b = c.blue;
 #endif
}

// Set all LEDs to a given color and apply it (visible)
void setAll(Color c) 
{
  for(int i = 0; i < NUM_LEDS; i++ ) 
  {
      setPixel(i, c); 
  }
  showStrip();
}
