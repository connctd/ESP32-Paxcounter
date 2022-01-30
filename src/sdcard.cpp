// routines for writing data to an SD-card, if present
// use FAT32 formatted card
// check whether your card reader supports SPI oder SDMMC and select appropriate
// SD low level driver in board hal file

// Local logging tag
static const char TAG[] = __FILE__;

#include "sdcard.h"

#ifdef HAS_SDCARD

static bool useSDCard = false;
static void openFile(void);

File fileSDCard;

bool sdcard_init(bool create) {

  uint8_t cardType;
  uint64_t cardSize;

  ESP_LOGI(TAG, "looking for SD-card...");

  // for usage of SD drivers on ESP32 platform see
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html

#if HAS_SDCARD == 1 // use SD SPI host driver
  useSDCard = MYSD.begin(SDCARD_CS, SDCARD_MOSI, SDCARD_MISO, SDCARD_SCLK);
#elif HAS_SDCARD == 2 // use SD MMC host driver
  // enable internal pullups of sd-data lines
  gpio_set_pull_mode(gpio_num_t(SDCARD_DATA0), GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(gpio_num_t(SDCARD_DATA1), GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(gpio_num_t(SDCARD_DATA2), GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(gpio_num_t(SDCARD_DATA3), GPIO_PULLUP_ONLY);
  useSDCard = MYSD.begin();
#endif

  if (useSDCard) {
    ESP_LOGI(TAG, "SD-card found");
    cardType = MYSD.cardType();
    cardSize = MYSD.cardSize() / (1024 * 1024);
  } else {
    ESP_LOGI(TAG, "SD-card not found");
    return false;
  }

  if (cardType == CARD_NONE) {
    ESP_LOGI(TAG, "No SD card attached");
    return false;
  }

  if (cardType == CARD_MMC) {
    ESP_LOGI(TAG, "SD Card type: MMC");
  } else if (cardType == CARD_SD) {
    ESP_LOGI(TAG, "SD Card type: SDSC");
  } else if (cardType == CARD_SDHC) {
    ESP_LOGI(TAG, "SD Card type: SDHC");
  } else {
    ESP_LOGI(TAG, "SD Card type: UNKNOWN");
  }

  ESP_LOGI(TAG, "SD Card Size: %lluMB\n", cardSize);

  openFile();

  return true;
}

void sdcard_close(void) {
  ESP_LOGI(TAG, "closing SD-card");
  fileSDCard.flush();
  fileSDCard.close();
}

void sdcardWriteData(uint16_t noWifi, uint16_t noBle,
                     __attribute__((unused)) uint16_t voltage) {
  static int counterWrites = 0;
  char tempBuffer[20 + 1];
  time_t t = time(NULL);
  struct tm tt;
  gmtime_r(&t, &tt); // make UTC timestamp

#if (HAS_SDS011)
  sdsStatus_t sds;
#endif

  if (!useSDCard)
    return;

  ESP_LOGI(TAG, "SD: writing data");
  strftime(tempBuffer, sizeof(tempBuffer), "%FT%TZ", &tt);
  fileSDCard.print(tempBuffer);
  snprintf(tempBuffer, sizeof(tempBuffer), ",%d,%d", noWifi, noBle);
  fileSDCard.print(tempBuffer);
#if (defined BAT_MEASURE_ADC || defined HAS_PMU)
  snprintf(tempBuffer, sizeof(tempBuffer), ",%d", voltage);
  fileSDCard.print(tempBuffer);
#endif
#if (HAS_SDS011)
  sds011_store(&sds);
  snprintf(tempBuffer, sizeof(tempBuffer), ",%5.1f,%4.1f", sds.pm10, sds.pm25);
  fileSDCard.print(tempBuffer);
#endif
  fileSDCard.println();

  if (++counterWrites > 2) {
    // force writing to SD-card
    ESP_LOGI(TAG, "SD: flushing data");
    fileSDCard.flush();
    counterWrites = 0;
  }
}

void openFile(void) {
  char bufferFilename[30];

  useSDCard = false;

  snprintf(bufferFilename, sizeof(bufferFilename), "/%s.csv", SDCARD_FILE_NAME);
  ESP_LOGI(TAG, "SD: looking for file <%s>", bufferFilename);

  /*
    if (MYSD.exists(bufferFilename)) {
      if (MYSD.open(bufferFilename, FILE_APPEND))
        useSDCard = true;
    } else {
      ESP_LOGI(TAG, "SD: file does not exist, creating it");
      if (MYSD.open(bufferFilename, FILE_WRITE))
        useSDCard = true;
    }
    */

  if (!MYSD.exists(bufferFilename))
    ESP_LOGI(TAG, "SD: file does not exist, creating it");

  if (MYSD.open(bufferFilename, FILE_WRITE))
    useSDCard = true;

  if (useSDCard) {
    ESP_LOGI(TAG, "SD: file opened: <%s>", bufferFilename);
    fileSDCard.print(SDCARD_FILE_HEADER);
#if (defined BAT_MEASURE_ADC || defined HAS_PMU)
    fileSDCard.print(SDCARD_FILE_HEADER_VOLTAGE); // for battery level data
#endif
#if (HAS_SDS011)
    fileSDCard.print(SDCARD_FILE_HEADER_SDS011);
#endif
    fileSDCard.println();
  } else {
    ESP_LOGE(TAG, "SD: file not opened error");
  }

  return;
}

#endif // (HAS_SDCARD)
