/*
    MIT License

    See https://github.com/unobtec/arduino-clock
    for build instructions.

*/

#define ENCODER_USE_INTERRUPTS

#include "ssd1306.h"
#include "intf/i2c/ssd1306_i2c.h"
#include <Encoder.h>
#include <TimeLib.h>

#undef ENABLE_SPEED

#include "montserrat-light-57x64.h"

#define UNKNOWN -1;

#include "config.h"
#include "hardware.h"

#ifndef FLIP_DISPLAYS
    const int leftDisplayI2CPort = I2C_PORT_A;
    const int rightDisplayI2CPort = I2C_PORT_B;
#else
    const int leftDisplayI2CPort = I2C_PORT_B;
    const int rightDisplayI2CPort = I2C_PORT_A;
#endif

long encoderPos = 0;

#ifndef FLIP_ENCODER_DIRECTION
Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);
#else
Encoder encoder(ENCODER_PIN_B, ENCODER_PIN_A);
#endif

#ifdef HAS_POWER_SWITCH
int buttonState = UNKNOWN;
int lastButtonState = UNKNOWN;
#endif

int encoderButtonState = UNKNOWN;
int lastEncoderButtonState = UNKNOWN;

const unsigned long debounceDelay = 50;
const unsigned long updateInterval = 100;
const unsigned long longPressDelay = 5000; // 5 sec

unsigned long lastDebounceTime = 0;
unsigned long prevUpdateTime = 0;

bool wasLeftScreen = true;
#ifdef DEFAULT_TO_12H_FORMAT
bool is12HourFormat = true;
#else
bool is12HourFormat = false;
#endif
bool timeWasSet = false;
bool isEncoderPressed = false;
bool canResetSeconds = false;
unsigned long lasEncoderPressTime = 0;

String prevAP = "";
int prevAPPos = 0;
String prevHour = "";
String prevMinute = "";
String prevSecond = "";

String inputBuffer = "";

#ifdef HAS_POWER_SWITCH
bool isTurnedOn()
{
    return buttonState == LOW;
}
#endif

void leftScreen()
{
    if (wasLeftScreen)
    {
        return;
    }
    wasLeftScreen = true;
    ssd1306_i2cInitEx(-1, -1, leftDisplayI2CPort);
}

void rightScreen()
{
    if (!wasLeftScreen)
    {
        return;
    }
    wasLeftScreen = false;
    ssd1306_i2cInitEx(-1, -1, rightDisplayI2CPort);
}

void fillBothScreens(byte value)
{
    leftScreen();
    ssd1306_fillScreen(value);
    rightScreen();
    ssd1306_fillScreen(value);
}

void clearBothScreens()
{
    fillBothScreens(0x00);
}

void turnOff()
{
#ifdef DEBUG
    Serial.println("Status: turned OFF");
#endif
    clearBothScreens();
}

void turnOn()
{
#ifdef DEBUG
    Serial.println("Status: turned ON");
#endif
    clearBothScreens();
    delay(500);

    prevAP = "";
    prevAPPos = 0;
    prevHour = "";
    prevMinute = "";
    prevSecond = "";
}

void setup()
{
#ifdef HAS_POWER_SWITCH
    pinMode(ON_OFF_SWITCH_PIN, INPUT_PULLUP);
#endif
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);

    Serial.begin(115200);

    // initialize displays with size
    ssd1306_128x64_i2c_initEx(-1, -1, leftDisplayI2CPort);
    ssd1306_setContrast(50);
    ssd1306_128x64_i2c_initEx(-1, -1, rightDisplayI2CPort);
    ssd1306_setContrast(50);
    wasLeftScreen = false; // because we initialized right screen last

    fillBothScreens(0xFF);
    delay(500);

    // when Arduino is turned on, default time is 0 (in seconds since epoch),
    // so to allow going back from 00:00 to 23:59, move the time to 00:00
    // of some real date
    adjustTime(247276800);

    // setTime(1, 2, 3, 4, 5, 2018); // DEBUG
#ifndef HAS_POWER_SWITCH
    turnOn();
#endif
}

#ifdef HAS_POWER_SWITCH
void pollPowerSwitch()
{
    int reading = digitalRead(ON_OFF_SWITCH_PIN);
    if (reading != lastButtonState)
    {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay)
    {
        if (reading != buttonState)
        {
            buttonState = reading;

            if (buttonState == HIGH)
            {
                turnOff();
            }
            else
            {
                turnOn();
            }
        }
    }
    lastButtonState = reading;
}
#endif

void pollEncoderButton()
{
    unsigned long t = millis();

    if (canResetSeconds && isEncoderPressed && (t - lasEncoderPressTime) > longPressDelay)
    {
        isEncoderPressed = false;
        canResetSeconds = false;
        is12HourFormat = !is12HourFormat;
        turnOn(); // reset screen
    }

    int reading = digitalRead(ENCODER_BTN_PIN);
    if (reading != lastEncoderButtonState)
    {
        lastDebounceTime = t;
    }

    if ((t - lastDebounceTime) > debounceDelay)
    {
        if (reading != encoderButtonState)
        {
            encoderButtonState = reading;

            if (encoderButtonState == HIGH)
            {
#ifdef DEBUG
                Serial.println("Encoder button released");
#endif
                isEncoderPressed = false;
                if (canResetSeconds)
                {
                    // reset seconds
                    adjustTime(-second());
                }
            }
            else
            {
#ifdef DEBUG
                Serial.println("Encoder button pressed");
#endif
                isEncoderPressed = true;
                canResetSeconds = true;
                lasEncoderPressTime = t;
            }
        }
    }
    lastEncoderButtonState = reading;
}

void pollEncoder()
{
    long pos = encoder.read();
    long delta = (pos - encoderPos) / 4;
    if (delta == 0)
    {
        return;
    }
    encoderPos = pos;

#ifdef DEBUG
    Serial.print(">> ");
    Serial.print(pos);
    Serial.print(", delta: ");
    Serial.println(delta);
#endif

    long sign = delta < 0 ? -1 : 1;

    if (isEncoderPressed)
    {
        canResetSeconds = false;
        adjustTime(delta * 60 * 60); // hours
    }
    else
    {
        adjustTime(delta * 60); // minutes
    }

    timeWasSet = true;
}

String formatTimePart(int n)
{
    String s = String(n);
    if (n < 10)
    {
        s = "0" + s;
    }
    return s;
}

void processFormatCommand(String arg)
{
    if (arg == "12")
    {
        is12HourFormat = true;
        Serial.println("OK: 12-hour format set");
        return;
    }

    if (arg == "24")
    {
        is12HourFormat = false;
        Serial.println("OK: 24-hour format set");
        return;
    }

    Serial.println("ERR: allowed formats: F12 or F24");
}

void processTimeCommand(String arg)
{
    // Example command:
    // T2018-05-13 14:32:12
    // part offsets:
    //  0    5  8  11 14 17
    int y = arg.substring(0, 4).toInt();
    int m = arg.substring(5, 7).toInt();
    int d = arg.substring(8, 10).toInt();
    int h = arg.substring(11, 13).toInt();
    int n = arg.substring(14, 16).toInt();
    int s = arg.substring(17, 19).toInt();
    setTime(h, n, s, d, m, y);
    timeWasSet = true;
    Serial.println("OK: Time set");
}

void processCommand(String arg)
{
    if (arg == "")
    {
        return;
    }

    Serial.print("ACK: '");
    Serial.print(arg);
    Serial.println("'");

    char cmd = arg[0];
    arg.remove(0, 1);

    switch (cmd)
    {
    case 'F':
        processFormatCommand(arg);
        break;
    case 'T':
        processTimeCommand(arg);
        break;
    default:
        Serial.print("ERR: Unknown command: ");
        Serial.println(String(cmd));
        return;
    }
}

void processSerialInput()
{
    while (Serial.available() > 0)
    {
        String arg = Serial.readString();

#ifdef DEBUG
        Serial.print("ECHO: ");
        Serial.println(arg);
#endif

        int pos = 0;
        int len = arg.length();
        for (int i = 0; i < len; i++)
        {
            if (arg.charAt(i) == '\r' || arg.charAt(i) == '\n')
            {
                if (i > pos)
                {
                    inputBuffer += arg.substring(pos, i);
                }
                processCommand(inputBuffer);
                inputBuffer = "";
                pos = i + 1;
            }
        }
        if (pos < len)
        {
            inputBuffer += arg.substring(pos, len);
        }

        if (inputBuffer.length() > 256)
        {
            inputBuffer = "";
            Serial.println("ERR: Command too long");
        }
    }
}

void drawClock()
{
    if (!timeWasSet && second() % 2 == 1)
    {
        clearBothScreens();
        prevAP = "";
        prevAPPos = 0;
        prevHour = "";
        prevMinute = "";
        prevSecond = "";
        return;
    }

    int h = hour();
    String ap = "  ";
    int apPos = 0;
    if (is12HourFormat)
    {
        ap = "AM";
        if (h >= 12)
        {
            h -= 12;
            ap = "PM";
            apPos = 63;
        }
        if (h == 0)
        {
            h = 12;
        }
    }

    String hh = formatTimePart(h);
    String mm = formatTimePart(minute());
    String ss = formatTimePart(second());

    if (apPos != prevAPPos)
    {
        leftScreen();
        ssd1306_setFixedFont(ssd1306xled_font6x8);
        ssd1306_printFixed(0, prevAPPos, "  ", STYLE_NORMAL);
        prevAPPos = apPos;
    }

    if (ap != prevAP)
    {
        prevAP = ap;
        leftScreen();
        ssd1306_setFixedFont(ssd1306xled_font6x8);
        ssd1306_printFixed(0, apPos, ap.c_str(), STYLE_NORMAL);
    }

    if (hh != prevHour)
    {
        prevHour = hh;
        leftScreen();
        ssd1306_setFixedFont(Montserrat_Light57x64);
        ssd1306_printFixed(14, 0, hh.c_str(), STYLE_NORMAL);
    }

    if (mm != prevMinute)
    {
        prevMinute = mm;
        rightScreen();
        ssd1306_setFixedFont(Montserrat_Light57x64);
        ssd1306_printFixed(0, 0, mm.c_str(), STYLE_NORMAL);
    }

    if (ss != prevSecond || mm != prevMinute)
    {
        prevSecond = ss;
        rightScreen();
        ssd1306_setFixedFont(ssd1306xled_font6x8);
        ssd1306_printFixed(112, 0, ss.c_str(), STYLE_NORMAL);
    }
}

void loop()
{
#ifdef HAS_POWER_SWITCH
    pollPowerSwitch();
#endif
    pollEncoderButton();
    pollEncoder();
    processSerialInput();

#ifdef HAS_POWER_SWITCH
    if (!isTurnedOn())
    {
        delay(100);
        return;
    }
#endif

    unsigned long t = millis();
    if (t - prevUpdateTime > updateInterval)
    {
        prevUpdateTime = t;
        drawClock();
    }
}
