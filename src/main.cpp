#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

GxEPD2_3C<GxEPD2_750c_Z90, GxEPD2_750c_Z90::HEIGHT / 2> display(GxEPD2_750c_Z90(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH075Z90 880x528

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

const char* ssid     = "Velop";
const char* password = "turtleturtle";
const int httpsPort = 443;
const char* fp_rawcontent     = "cc aa 48 48 66 46 0e 91 53 2c 9c 7c 23 2a b1 74 4d 29 9d 33";
const char* host_rawcontent   = "raw.githubusercontent.com";
const char* path_rawcontent   = "/ZinggJM/GxEPD2/master/extras/bitmaps/";
const char* file_name   = "logo200x200.bmp";

//https://raw.githubusercontent.com/dvismans/calendar/master/bmp/test2.bmp
//const char* path_rawcontent   = "/dvismans/calendar/master/bmp/";
//const char* file_name   = "logo880x528-3.bmp";

void downloadingMessage();
void helloWorld();

uint16_t read16(WiFiClient& client);
uint32_t read32(WiFiClient& client);
uint32_t skip(WiFiClient& client, int32_t bytes);
void tryToWaitForAvailable(WiFiClient& client, int32_t amount);


void showBitmapFrom_HTTPS(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color = true);

const char HelloWorld[] = "Hello World!";



void setup()
{
  Serial.begin(115200);
  Serial.println("Connecting to Wifi...");

  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.begin(ssid, password);
  
  int ConnectTimeout = 30; // 15 seconds
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    Serial.print(WiFi.status());
    if (--ConnectTimeout <= 0)
    {
      Serial.println();
      Serial.println("WiFi connect timeout");
      return;
    }
  }
  Serial.println();
  Serial.println("WiFi connected");

  Serial.println("Setup display...");
  delay(100);
  display.init(115200);
  // first update should be full refresh

  Serial.println("Running showBitmapFrom_HTTPS...");
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, file_name, fp_rawcontent, 0, 0, false);

  //Serial.println("Running helloWorld...");
  //helloWorld();

  //Serial.println("Running downloadingMessage...");
  //downloadingMessage();
  //display.powerOff();
  //deepSleepTest();
  Serial.println("Done.");

  
}

void helloWorld()
{
  //Serial.println("helloWorld");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  }
  while (display.nextPage());
  //Serial.println("helloWorld done");
}

void downloadingMessage()
{
  display.setRotation(2);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 20);
    display.print("Downloading file "); display.println(file_name);
  }
  while (display.nextPage());
}


void loop(void)
{
}

static const uint16_t input_buffer_pixels = 880; // may affect performance
static const uint16_t max_row_width = 880; // for up to 7.5" display
static const uint16_t max_palette_pixels = 256; // for depth <= 8
uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

void showBitmapFrom_HTTPS(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color)
{
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display.width()) || (y >= display.height())) return;
  Serial.println(); Serial.print("downloading file \""); Serial.print(filename);  Serial.println("\"");
  Serial.print("connecting to "); Serial.println(host);
  if (!client.connect(host, httpsPort))
  {
    Serial.println("connection failed");
    return;
  }
  Serial.print("requesting URL: ");
  Serial.println(String("https://") + host + path + filename);
  client.print(String("GET ") + path + filename + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: GxEPD2_Spiffs_Loader\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println("request sent");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (!connection_ok)
    {
      connection_ok = line.startsWith("HTTP/1.1 200 OK");
      if (connection_ok) Serial.println(line);
      //if (!connection_ok) Serial.println(line);
    }
    if (!connection_ok) Serial.println(line);
    //Serial.println(line);
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }
  }
  if (!connection_ok) return;
  // Parse BMP header
  uint32_t bmp_sig = read16(client);
  Serial.println("Read header:");
  Serial.println(bmp_sig);

  if (bmp_sig == 0x4D42) // BMP signature
  {
    Serial.println("bmp signature in header");

    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client);
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width  = read32(client);
    uint32_t height = read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print("File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Bit Depth: "); Serial.println(depth);
      Serial.print("Image size: ");
      Serial.print(width);
      Serial.print('x');
      Serial.println(height);
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= display.width())  w = display.width()  - x;
      if ((y + h - 1) >= display.height()) h = display.height() - y;
      if (w <= max_row_width) // handle with direct drawing
      {
        valid = true;
        uint8_t bitmask = 0xFF;
        uint8_t bitshift = 8 - depth;
        uint16_t red, green, blue;
        bool whitish, colored;
        if (depth == 1) with_color = false;
        if (depth <= 8)
        {
          if (depth < 8) bitmask >>= depth;
          bytes_read += skip(client, 54 - bytes_read); //palette is always @ 54
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
          {
            blue  = client.read();
            green = client.read();
            red   = client.read();
            client.read();
            bytes_read += 4;
            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
            color_palette_buffer[pn / 8] |= colored << pn % 8;
            Serial.print("0x00"); Serial.print(red, HEX); Serial.print(green, HEX); Serial.print(blue, HEX);
            Serial.print(" : "); Serial.print(whitish); Serial.print(", "); Serial.println(colored);
          }
        }
        display.writeScreenBuffer();
        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
        Serial.print("skip "); Serial.println(rowPosition - bytes_read);
        bytes_read += skip(client, rowPosition - bytes_read);
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          if (!connection_ok || !client.connected()) break;
          delay(1); // yield() to avoid WDT
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0; // for depth <= 8
          uint8_t in_bits = 0; // for depth <= 8
          uint8_t out_byte = 0;
          uint8_t out_color_byte = 0;
          uint32_t out_idx = 0;
          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            yield();
            if (!connection_ok || !client.connected()) break;
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
              //if (get > client.available()) delay(200); // does improve? yes, if >= 200
              // there seems an issue with long downloads on ESP8266
              tryToWaitForAvailable(client, get);
              uint32_t got = client.read(input_buffer, get);
              while ((got < get) && connection_ok)
              {
                //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
                //if ((get - got) > client.available()) delay(200); // does improve? yes, if >= 200
                // there seems an issue with long downloads on ESP8266
                tryToWaitForAvailable(client, get - got);
                uint32_t gotmore = client.read(input_buffer + got, get - got);
                got += gotmore;
                connection_ok = gotmore > 0;
              }
              in_bytes = got;
              in_remain -= got;
              bytes_read += got;
            }
            if (!connection_ok)
            {
              Serial.print("Error: got no more after "); Serial.print(bytes_read); Serial.println(" bytes read!");
              break;
            }
            switch (depth)
            {
              case 24:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                break;
              case 16:
                {
                  uint8_t lsb = input_buffer[in_idx++];
                  uint8_t msb = input_buffer[in_idx++];
                  if (format == 0) // 555
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                    red   = (msb & 0x7C) << 1;
                  }
                  else // 565
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                    red   = (msb & 0xF8);
                  }
                  whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                  colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                }
                break;
              case 1:
              case 4:
              case 8:
                {
                  if (0 == in_bits)
                  {
                    in_byte = input_buffer[in_idx++];
                    in_bits = 8;
                  }
                  uint16_t pn = (in_byte >> bitshift) & bitmask;
                  whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  in_byte <<= depth;
                  in_bits -= depth;
                }
                break;
            }
            if (whitish)
            {
              out_byte |= 0x80 >> col % 8; // not black
              out_color_byte |= 0x80 >> col % 8; // not colored
            }
            else if (colored && with_color)
            {
              out_byte |= 0x80 >> col % 8; // not black
            }
            else
            {
              out_color_byte |= 0x80 >> col % 8; // not colored
            }
            if (7 == col % 8)
            {
              output_row_color_buffer[out_idx] = out_color_byte;
              output_row_mono_buffer[out_idx++] = out_byte;
              out_byte = 0;
              out_color_byte = 0;
            }
          } // end pixel
          int16_t yrow = y + (flip ? h - row - 1 : row);
          Serial.print(".");
          display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
        } // end line
        Serial.print("downloaded in "); Serial.print(millis() - startTime); Serial.println(" ms");
        Serial.print("bytes read "); Serial.println(bytes_read);
        display.refresh();
      } else {
        Serial.println("w NOT < than max_row_width");
        Serial.println(w);
        Serial.println(max_row_width);
      }
    }
  } else {
        Serial.println("bmp signature NOT in header");
  }
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}

uint16_t read16(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  return result;
}

uint32_t read32(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}

uint32_t skip(WiFiClient& client, int32_t bytes)
{
  int32_t remain = bytes;
  int16_t retries = 3;
  uint32_t chunk = 1024;
  if (chunk > sizeof(input_buffer)) chunk = sizeof(input_buffer);
  while (client.connected() && (remain > chunk))
  {
    // there seems an issue with long downloads on ESP8266
    tryToWaitForAvailable(client, chunk);
    uint32_t got = client.read(input_buffer, chunk);
    //Serial.print("skipped "); Serial.println(got);
    remain -= got;
    if ((0 == got) && (0 == --retries))
    {
      Serial.print("Error on skip, got 0, skipped "); Serial.print(bytes - remain); Serial.print(" of "); Serial.println(bytes);
      break; // don't hang forever (retries don't help)
    }
  }
  if (client.connected() && (remain > 0) && (remain <= chunk))
  {
    remain -= client.read(input_buffer, remain);
  }
  return bytes - remain; // bytes read and skipped
}

void tryToWaitForAvailable(WiFiClient& client, int32_t amount)
{
  // this doesn't work as expected, but it helps for long downloads
  int32_t start = millis();
  for (int16_t t = 0, dly = 50; t < 20; t++, dly += 50)
  {
    if (client.available()) break; // read would not recover after having returned 0
    delay(dly);
  }
  for (int16_t t = 0, dly = 50; t < 3; t++, dly += 25)
  {
    if (amount <= client.available()) break;
    delay(dly);
    //Serial.print("available "); Serial.println(client.available()); // stays constant
  }
  int32_t elapsed = millis() - start;
  if (elapsed > 250)
  {
    Serial.print("waited for available "); Serial.print(millis() - start); Serial.println(" ms"); // usually 0 or 225ms
  }
}
