/*
  Connect the SD card to the following pins:

*/
#include "Flash.h"

void setup()
{
  Flash mem;
  
  Serial.begin(115200);
  Serial.println("\n\nSTART");

  int ret = mem.init();
  if (ret != SUCCESS) {
    Serial.printf("Card Mount Failed return code: %d\n", ret);
    vTaskDelete( NULL );
  }

  char buff[20];
  mem.openFile("test.txt", "r");
  if( mem.readFile(buff, sizeof(buff)) == SUCCESS )
    Serial.printf("file read: %s\n", buff);
  else
    Serial.println("couldn't read from file");
  mem.closeFile();

  uint32_t ws = 0, rs = 0;
  ret = mem.testFileIO("speed.txt", &ws, &rs);
  if (ret == SUCCESS)
    Serial.printf("write speed = %dB/s\nreed speed = %dB/s\n", 1024*1024/ws, 1024*1024/rs);
  else
    Serial.printf("Test IO speed failed ret = %d\n", ret);

  Serial.println("FINISH\n");
  vTaskDelete( NULL );
}

void loop()
{

}
