#include <SoftwareSerial.h>
#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
#include <ArduinoJson.h>

SoftwareSerial softSerial(2, 3); //RX, TX
SoftwareSerial ESPSerial(5, 6); // RX, TX

RFID nano; //Create instance
String epcTagString = "";
int ctr = 0;

void array_to_string(byte arr[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (arr[i] >> 4) & 0x0F;
        byte nib2 = (arr[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}



boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while (!softSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (softSerial.available()) softSerial.read();

  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    softSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}





int getFreeRam()
{
  extern int __heap_start, *__brkval; 
  int v;

  v = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);

  Serial.print("Free RAM = ");
  Serial.println(v, DEC);

  return v;
}

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // while (!Serial) {
  //    ; // wait for serial port to connect. Needed for Leonardo only
  //  }
//  pinMode(A0, INPUT);
  ESPSerial.begin(9600);


  while (!Serial); //Wait for the serial port to come online

  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  nano.setRegion(REGION_EUROPE); //Set to Europe

  nano.setReadPower(500); //5.00 dBm. Higher values may caues USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

//  Serial.println(F("Press a key to begin scanning for tags."));
//  while (!Serial.available()); //Wait for user to send a character
//  Serial.read(); //Throw away the user's character

  nano.startReading(); //Begin scanning for tags
  delay(2000);
}

void loop() // run over and over
{ 

  if (nano.check() == true) //Check to see if any new data has come in from module
  {
    //Tell readUserData to read up to 64 bytes


    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp
    ctr = ctr+1;
    Serial.println(ctr);

    signed int rssi = 0;

    if (ctr == 10) {
      
      if (responseType == RESPONSE_IS_KEEPALIVE)
      {
        Serial.println(F("Scanning"));
      }
      else if (responseType == RESPONSE_IS_TAGFOUND)
      {
        //If we have a full record we can pull out the fun bits
        rssi = nano.getTagRSSI(); //Get the RSSI for this tag read
//      long freq = nano.getTagFreq(); //Get the frequency this tag was detected at
        long timeStamp = nano.getTagTimestamp(); //Get the time this was read, (ms) since last keep-alive message

        byte tagEPCBytes = nano.getTagEPCBytes(); //Get the number of bytes of EPC from response

        char tmp[tagEPCBytes];
        for (byte x = 0 ; x < tagEPCBytes ; x++)
        {
          tmp[x] = nano.msg[31 + x];
        }

        char str[tagEPCBytes] = "";
        array_to_string(tmp, tagEPCBytes, str);
        Serial.println(str);
        

        Serial.println();
        ESPSerial.print(F("{\"EPC\":\""));

        ESPSerial.print(str);
        ESPSerial.print(F("\", \"rssi\" : \""));
        ESPSerial.print(rssi);
        ESPSerial.print(F("\", \"timeStamp\" : \""));
        ESPSerial.print(timeStamp);
        ESPSerial.println(F("\"}"));
        Serial.print(F("{\"EPC\":\""));
        Serial.print(str);
      
        Serial.print(F("\", \"rssi\" : \""));
        Serial.print(rssi);
        Serial.print(F("\", \"timeStamp\" : \""));
        Serial.print(timeStamp);
        Serial.println(F("\"}"));
        ESPSerial.flush();
        Serial.println();
      }
      else if (responseType == ERROR_CORRUPT_RESPONSE)
      {
        Serial.println("Bad CRC");
      }
      else
      {
        //Unknown response
        Serial.print("Unknown error");
      }
      Serial.println();

      ctr = 0;
      getFreeRam();
    }
  }

}

