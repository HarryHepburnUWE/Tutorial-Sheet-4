#include "mbed.h"

// Define the PWM pin (D15 = PB_8 on NUCLEO-F439ZI, CN9 Pin 15)
PwmOut buzzer(D9);

// Gas sensor input pin (A3)
AnalogIn gasSensor(A3);

// LM35 temperature sensor input pin (A1)
AnalogIn lm35(A1);  // 

// Potentiometer input pin (A0)
AnalogIn potentiometer(A0);

// Define the LED for debugging
DigitalOut led(LED1);  // Onboard LED (LD2)

// Serial communication setup (for UART)
UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

// Global variables
float lm35Reading = 0.0;
float lm35TempC = 0.0;
float lm35TempF = 0.0;
float potentiometerReading = 0.0;
float potentiometerScaledToC = 0.0;
float potentiometerScaledToF = 0.0;
bool gasDetected = false;
bool tempExceeded = false;

// Function prototypes
void pcSerialComStringWrite(const char* str);
char pcSerialComCharRead();
float analogReadingScaledWithTheLM35Formula(float analogReading);
float celsiusToFahrenheit(float tempInCelsiusDegrees);
float potentiometerScaledToCelsius(float analogValue);
float potentiometerScaledToFahrenheit(float analogValue);
void availableCommands();
void uartTask();
void checkSensors();
float readStableAnalog(AnalogIn &sensor);

int main() {
    buzzer.period_ms(2);  // 500 Hz (period = 1/500 = 0.002 seconds = 2 ms)
    buzzer.write(0.0f);   // Start with the buzzer off

    availableCommands();  // Display available commands in the serial terminal
    while (true) {
        checkSensors();  // Continuously check gas, temperature, and potentiometer
        uartTask();      // Handle UART commands
        ThisThread::sleep_for(200ms);  // Control loop frequency
    }
}

// Sends a string to the serial terminal
void pcSerialComStringWrite(const char* str) {
    uartUsb.write(str, strlen(str));
}


// Reads a character from the serial terminal
char pcSerialComCharRead() {
    char receivedChar = '\0';
    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);
    }
    return receivedChar;
}

// Shows available commands in the serial terminal
void availableCommands() {
    pcSerialComStringWrite("\r\nPress the following keys to continuously ");
    pcSerialComStringWrite("print the readings until 'q' is pressed:\r\n");
    pcSerialComStringWrite(" - 'a' the reading at the analog pin A0 (potentiometer)\r\n");
    pcSerialComStringWrite(" - 'b' the reading at the analog pin A1 (LM35)\r\n");
    pcSerialComStringWrite(" - 'c' the temperature in Celsius from LM35\r\n");
    pcSerialComStringWrite(" - 'd' the temperature in Fahrenheit from LM35\r\n");
    pcSerialComStringWrite(" - 'e' both LM35 in Celsius and potentiometer value in Celsius\r\n");
    pcSerialComStringWrite(" - 'f' both LM35 in Fahrenheit and potentiometer value in Fahrenheit\r\n");
    pcSerialComStringWrite("\r\nWARNING: Press 'q' or 'Q' to stop.\r\n");
}

// Check gas sensor, temperature, and potentiometer continuously, and print readings
void checkSensors() {
    static bool lastGasDetected = false;
    static bool lastTempExceeded = false;
    static uint32_t lastPrintTime = 0;
    char str[100] = "";

    // Read all sensors with stable readings
    float gasReading = readStableAnalog(gasSensor);
    lm35Reading = readStableAnalog(lm35);
    lm35TempC = analogReadingScaledWithTheLM35Formula(lm35Reading);
    lm35TempF = celsiusToFahrenheit(lm35TempC);
    potentiometerReading = readStableAnalog(potentiometer);

    // Check gas sensor
    if (gasReading > 0.5f) {  // Gas detected (adjust threshold as needed)
        gasDetected = true;
        if (!lastGasDetected) {  // State change: gas newly detected
            pcSerialComStringWrite("Gas detected!\r\n");
            lastGasDetected = true;
        }
    } else {
        gasDetected = false;
        if (lastGasDetected) {  // State change: gas no longer detected
            pcSerialComStringWrite("Gas no longer detected.\r\n");
            lastGasDetected = false;
        }
    }

    // Check temperature
    if (lm35TempC > 24.0f) {
        tempExceeded = true;
        if (!lastTempExceeded) {  // State change: temp newly exceeded
            pcSerialComStringWrite("ALERT: LM35 temperature exceeds 24°C!\r\n");
            lastTempExceeded = true;
        }
    } else {
        tempExceeded = false;
        if (lastTempExceeded) {  // State change: temp no longer exceeded
            pcSerialComStringWrite("LM35 temperature below 24°C.\r\n");
            lastTempExceeded = false;
        }
    }

    // Print all sensor readings every 1 second
    uint32_t currentTime = us_ticker_read() / 1000;  // Get current time in milliseconds
    if (currentTime - lastPrintTime >= 1000) {       // Print every 1000 ms (1 second)
        sprintf(str, "Gas: %.2f, LM35: %.2f C, Potentiometer: %.2f\r\n",
                gasReading, lm35TempC, potentiometerReading);
        pcSerialComStringWrite(str);
        lastPrintTime = currentTime;
    }

    // Control buzzer and LED, and print alarm source
    if (gasDetected || tempExceeded) {
        buzzer.write(0.5f);  // Turn buzzer on
        led = !led;          // Toggle LED to indicate alert
        // Print alarm source(s)
        if (gasDetected) {
            pcSerialComStringWrite("Gas Alarm\r\n");
        }
        if (tempExceeded) {
            pcSerialComStringWrite("Temperature Alarm\r\n");
        }
    } else {
        buzzer.write(0.0f);  // Turn buzzer off
        led = 0;             // Turn LED off
    }
}

// Main loop for UART handling based on user input
void uartTask() {
    char receivedChar = '\0';
    char str[100] = "";
    receivedChar = pcSerialComCharRead();

    if (receivedChar != '\0') {
        switch (receivedChar) {
        case 'a':
        case 'A':
            while (!(receivedChar == 'q' || receivedChar == 'Q')) {
                potentiometerReading = potentiometer.read();
                sprintf(str, "Potentiometer reading: %.2f\r\n", potentiometerReading);
                pcSerialComStringWrite(str);
                ThisThread::sleep_for(200ms);
                receivedChar = pcSerialComCharRead();
            }
            break;

        case 'b':
        case 'B':
            while (!(receivedChar == 'q' || receivedChar == 'Q')) {
                lm35Reading = lm35.read();
                sprintf(str, "LM35 reading: %.2f\r\n", lm35Reading);
                pcSerialComStringWrite(str);
                ThisThread::sleep_for(200ms);
                receivedChar = pcSerialComCharRead();
            }
            break;

        case 'c':
        case 'C':
            while (!(receivedChar == 'q' || receivedChar == 'Q')) {
                lm35Reading = lm35.read();
                lm35TempC = analogReadingScaledWithTheLM35Formula(lm35Reading);
                sprintf(str, "LM35: %.2f °C\r\n", lm35TempC);
                pcSerialComStringWrite(str);
                ThisThread::sleep_for(200ms);
                receivedChar = pcSerialComCharRead();
            }
            break;

        case 'd':
        case 'D':
            while (!(receivedChar == 'q' || receivedChar == 'Q')) {
                lm35Reading = lm35.read();
                lm35TempC = analogReadingScaledWithTheLM35Formula(lm35Reading);
                lm35TempF = celsiusToFahrenheit(lm35TempC);
                sprintf(str, "LM35: %.2f °F\r\n", lm35TempF);
                pcSerialComStringWrite(str);
                ThisThread::sleep_for(200ms);
                receivedChar = pcSerialComCharRead();
            }
            break;

        case 'e':
        case 'E':
            while (!(receivedChar == 'q' || receivedChar == 'Q')) {
                potentiometerReading = potentiometer.read();
                potentiometerScaledToC = potentiometerScaledToCelsius(potentiometerReading);
                lm35Reading = lm35.read();
                lm35TempC = analogReadingScaledWithTheLM35Formula(lm35Reading);
                sprintf(str, "LM35: %.2f °C, Potentiometer scaled to °C: %.2f\r\n",
                        lm35TempC, potentiometerScaledToC);
                pcSerialComStringWrite(str);
                ThisThread::sleep_for(200ms);
                receivedChar = pcSerialComCharRead();
            }
            break;

        case 'f':
        case 'F':
            while (!(receivedChar == 'q' || receivedChar == 'Q')) {
                potentiometerReading = potentiometer.read();
                potentiometerScaledToF = potentiometerScaledToFahrenheit(potentiometerReading);

                lm35Reading = lm35.read();
                lm35TempC = analogReadingScaledWithTheLM35Formula(lm35Reading);
                lm35TempF = celsiusToFahrenheit(lm35TempC);

                sprintf(str, "LM35: %.2f °F, Potentiometer scaled to °F: %.2f\r\n",
                        lm35TempF, potentiometerScaledToF);
                pcSerialComStringWrite(str);
                ThisThread::sleep_for(200ms);
                receivedChar = pcSerialComCharRead();
            }
            break;

        default:
            break;
        }
    }
}

// Stable analog reading function
float readStableAnalog(AnalogIn &sensor) {
    // Read multiple times to stabilize the value
    float reading = 0;
    for (int i = 0; i < 10; ++i) {
        reading += sensor.read();
        ThisThread::sleep_for(10ms);  // Add small delay between readings
    }
    return reading / 10.0f;  // Average to stabilize
}

// LM35 formula conversion to Celsius
float analogReadingScaledWithTheLM35Formula(float analogReading) {
    return analogReading * 330.0f;  // LM35 gives 10mV/°C and the ADC maps it to 0-3.3V
}

// Celsius to Fahrenheit conversion
float celsiusToFahrenheit(float tempInCelsiusDegrees) {
    return tempInCelsiusDegrees * 9.0f / 5.0f + 32.0f;
}

// Potentiometer scaling to Celsius (example: 0V to 30°C)
float potentiometerScaledToCelsius(float analogValue) {
    return analogValue * 330.0f;  // Assuming the potentiometer range is 0-3V and maps to 0-30°C
}

// Potentiometer scaling to Fahrenheit
float potentiometerScaledToFahrenheit(float analogValue) {
    return potentiometerScaledToCelsius(analogValue) * 9.0f / 5.0f + 32.0f;
}