#include "Inkplate.h"
#include "htmlCode.h"
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <uri/UriBraces.h>
#include "Network.h"

#define ssid ""
#define pass ""
#define REFRESH_DELAY 6000

// App state management
enum AppState {
    PICTURE_APP,
    QUOTE_APP,
    TODO_APP
};

Inkplate display(INKPLATE_3BIT);
WebServer server(80);
Network network;
SdFile folder, file;

// App state variables
AppState currentApp = PICTURE_APP;
RTC_DATA_ATTR uint16_t lastImageIndex = 0;
const char folderPath[] = "/images/";

// Timer variables
unsigned long lastQuoteRefresh = 0;
unsigned long lastPictureRefresh = 0;
unsigned long refreshTime = 0;
const unsigned long QUOTE_REFRESH_INTERVAL = 300000;  // 5 minutes
const unsigned long PICTURE_REFRESH_INTERVAL = 60000; // 1 minute

// Todo list variables
IPAddress serverIP;
std::vector<String> texts;
std::vector<bool> checkedStatus;
//char quote[256];
//char author[64];
//int len;
char date[64];
int n = 0;

// Quote structure
struct Quote {
    const char* text;
    const char* author;
};

// Array of quotes
const Quote quotes[] = {
    {"inser quote here", "author"},

};

const int numQuotes = sizeof(quotes) / sizeof(quotes[0]);
RTC_DATA_ATTR int currentQuoteIndex = 0;

void setup() {
    Serial.begin(115200);
    display.begin();
    display.setTextWrap(true);

    // Initialize touchscreen
    if (!display.tsInit(true)) {
        Serial.println("Touchscreen init failed!");
    }

    // Initialize APDS9960 sensor
    display.wakePeripheral(INKPLATE_APDS9960);
    if (!display.apds9960.init()) {
        Serial.println("APDS9960 initialization failed!");
    }
    display.apds9960.enableGestureSensor();
    display.apds9960.setGestureGain(0);

    // Initialize network and web server
    network.begin(ssid, pass);
    serverIP = WiFi.localIP();
    server.on("/", handleRoot);
    server.on(UriBraces("/string/{}"), handleString);
    server.begin();

    // Initialize SD card
    if (!display.sdCardInit()) {
        Serial.println("SD Card initialization failed!");
    }

    // Start with picture display
    displayNextImage();
}

void loop() {
    server.handleClient();

    // Check for gestures
    if (display.apds9960.isGestureAvailable()) {
        int gesture = display.apds9960.readGesture();
        if (gesture == DIR_LEFT || gesture == DIR_RIGHT || gesture == DIR_UP || gesture == DIR_DOWN) {
            switchToNextApp();
        }
    }

    // Handle auto-refresh for current app
    switch (currentApp) {
        case PICTURE_APP:
            if (millis() - lastPictureRefresh > PICTURE_REFRESH_INTERVAL) {
                lastPictureRefresh = millis();
                displayNextImage();
            }
            break;

        case QUOTE_APP:
            if (millis() - lastQuoteRefresh > QUOTE_REFRESH_INTERVAL) {
                lastQuoteRefresh = millis();
//                if (network.getData(quote, author, &len, &display)) {
//                    drawNetworkQuote();
//                }
                drawLocalQuote();
            }
            break;

        case TODO_APP:
            for (int i = 0; i < texts.size(); ++i) {
                if (display.touchInArea(0, 220 + 60 * i, display.width() - 100, 60)) {
                    toggleCheckbox(i);
                }
                if (display.touchInArea(display.width() - 100, 220 + 60 * i, 100, 60)) {
                    deleteTodoItem(i);
                    delay(200);
                }
            }
            break;
    }

    if(currentApp != PICTURE_APP)
    {
      // Update time display
      if ((unsigned long)(millis() - refreshTime) > REFRESH_DELAY) {
        displayTime();
        if (n > 9) {
            display.display(true);
            n = 0;
        } else {
            display.partialUpdate(false, true);
            n++;
        }
        refreshTime = millis();
      }
    }
    

    delay(15);
}

void displayNextImage() {
    if (!display.sdCardInit()) {
        return;
    }

    if (folder.open(folderPath)) {
        if (!file.openNext(&folder, O_RDONLY)) {
            lastImageIndex = 0;
            folder.rewind();
            file.openNext(&folder, O_RDONLY);
        }

        lastImageIndex = file.dirIndex();
        
        char pictureName[100];
        file.getName(pictureName, 100);
        
        char path[120];
        strcpy(path, folderPath);
        strcat(path, pictureName);
        
        display.clearDisplay();
        display.drawImage(path, 0, 0, 1, 0);
        display.display();
        
        file.close();
        folder.close();
    }
}

void switchToNextApp() {
    currentApp = static_cast<AppState>((currentApp + 1) % 3);
    
    switch (currentApp) {
        case PICTURE_APP:
            displayNextImage();
            break;
        case QUOTE_APP:
//            if (network.getData(quote, author, &len, &display)) {
//                drawNetworkQuote();
//            }
              drawLocalQuote();
            break;
        case TODO_APP:
            drawToDo();
            break;
    }
}


void drawToDo() {
    display.clearDisplay();
    display.setTextColor(0); 
    // Display current time
    displayTime();

    int startY = 110;

    display.setTextSize(2);
    display.setCursor(50, startY); // Adjusted position
    display.setTextSize(6);
    
    display.print("To do: ");
    
    display.fillRect(10, startY + 70, 580, 3, BLACK);

    int toDoYPosition = startY * 2 ; // Adjusted start position for displaying texts
    for (int i = 0; i < texts.size(); ++i) {
        if (toDoYPosition > 400) break;

        if (checkedStatus[i]) {
            display.fillCircle(60, toDoYPosition + 20, 20, BLACK);
        } else {
            display.drawCircle(60, toDoYPosition + 20, 20, BLACK);
        }

        display.setTextSize(4);
        display.setCursor(100, toDoYPosition);
        display.print(texts[i]);

         // Draw "X" button
        display.setTextSize(3);
        display.setCursor(display.width() - 60, toDoYPosition + 10 );
        display.print("X");
        
        toDoYPosition += 60;
    }
    
    display.display();
}


//void drawNetworkQuote() {
//    display.clearDisplay();
//    display.setTextColor(0); 
//
//    // Display current time
//    displayTime();
//
//    int startX = 50;
//    int startY = 150;
//    int lineHeight = 48; // Adjust based on your font size
//
//    display.setCursor(startX, startY);
//
//    std::vector<String> words;
//    char *word = strtok(quote, " ");
//    while (word != nullptr) {
//        words.push_back(String(word));
//        word = strtok(nullptr, " ");
//    }
//
//    int currentRow = 0;
//    String currentLine;
//
//    for (const auto &word : words) {
//        String tempLine = currentLine + (currentLine.isEmpty() ? "" : " ") + word;
//        int16_t x1, y1;
//        uint16_t w, h;
//        display.getTextBounds(tempLine.c_str(), startX, startY + (lineHeight * currentRow), &x1, &y1, &w, &h);
//
//        if ((x1 + w) > 560) { // If the word doesn't fit in the current line
//            // Print the current line and move to the next row
//            display.print(currentLine);
//            display.setCursor(startX, startY + (lineHeight * ++currentRow));
//            currentLine = word; // Start new line with the current word
//        } else {
//            currentLine = tempLine; // Add word to the current line
//        }
//    }
//
//    // Print the last line if there's any remaining text
//    if (!currentLine.isEmpty()) {
//        display.print(currentLine);
//    }
//
//    // Print author
//    display.setCursor(startX, startY + (lineHeight * (++currentRow)));
//    display.print("- ");
//    display.println(author);
//
//    display.display(); // Refresh screen with new content
//}

void drawLocalQuote() {
    currentQuoteIndex = (currentQuoteIndex + 1) % numQuotes;

    display.clearDisplay();
    display.setTextColor(0);
    displayTime();
    
    int startX = 50;
    int startY = 150;
    int lineHeight = 48;
    display.setCursor(startX, startY);
    
    // Split and draw the quote text
    String quoteText = quotes[currentQuoteIndex].text;
    std::vector<String> words;
    char* text = strdup(quoteText.c_str());
    char* word = strtok(text, " ");
    while (word != nullptr) {
        words.push_back(String(word));
        word = strtok(nullptr, " ");
    }
    free(text);
    
    int currentRow = 0;
    String currentLine;
    for (const auto &word : words) {
        String tempLine = currentLine + (currentLine.isEmpty() ? "" : " ") + word;
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(tempLine.c_str(), startX, startY + (lineHeight * currentRow), &x1, &y1, &w, &h);
        
        if ((x1 + w) > 560) {
            display.print(currentLine);
            display.setCursor(startX, startY + (lineHeight * ++currentRow));
            currentLine = word;
        } else {
            currentLine = tempLine;
        }
    }
    
    if (!currentLine.isEmpty()) {
        display.print(currentLine);
    }
    
    display.setCursor(startX, startY + (lineHeight * (++currentRow)));
    display.print("- ");
    display.println(quotes[currentQuoteIndex].author);
    
    display.display();
}

// Function to print the current time at the top of the screen
void displayTime() {
    // Clear the area where the time is displayed with WHITE
    display.fillRect(0, 0, display.width(), 100, 7);
    
    // Define timezone offset for EEST (UTC+3)
    int timezoneOffsetHours = 2;

    // Get the current time from the network
    network.getTime(date);

    // Convert the date string to a time_t structure
    struct tm timeinfo;
    strptime(date, "%a %b %d %H:%M:%S %Y", &timeinfo);

    // Adjust for timezone
    time_t localTime = mktime(&timeinfo) + timezoneOffsetHours * 3600;

    // Convert back to tm structure for display
    localtime_r(&localTime, &timeinfo);

    // Extract weekday and month abbreviations
    char weekday[4], month[4];
    strncpy(weekday, "SunMonTueWedThuFriSat" + (timeinfo.tm_wday * 3), 3);
    strncpy(month, "JanFebMarAprMayJunJulAugSepOctNovDec" + (timeinfo.tm_mon * 3), 3);
    weekday[3] = '\0';
    month[3] = '\0';

    // Format and display the local time and date
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s, %02d %s, %02d:%02d",
             weekday, timeinfo.tm_mday, month, timeinfo.tm_hour, timeinfo.tm_min);

    // Clear the area where the time is displayed
    display.fillRect(display.width() / 2 - 220, 20, 480, 84, 7); // Adjust size as needed

    display.setTextSize(4);
    display.setCursor(display.width() / 2 - 220, 35);
    display.println(buffer);
}


void updateHTML() {
    server.send(200, "text/html", s);
}

void handleRoot() {
    updateHTML();
}

void handleString() {
    texts.push_back(server.arg(0));
    checkedStatus.push_back(false);
    updateHTML();
    drawToDo();
}

void toggleCheckbox(int index) {
    if (index < checkedStatus.size()) {
        checkedStatus[index] = !checkedStatus[index];
        drawToDo();
    }
}

void clearTexts() {
    texts.clear();
    checkedStatus.clear();
    drawToDo();
}

void deleteTodoItem(int index) {
  if (index < texts.size()) {
      texts.erase(texts.begin() + index);
      checkedStatus.erase(checkedStatus.begin() + index);
      drawToDo();
  }
}
