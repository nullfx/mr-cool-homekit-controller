#include "Pairing.h"

#include <HomeSpan.h>
#include <Preferences.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <esp_rom_crc.h>
#include <iterator>
#include <nvs.h>

namespace mrcool {
namespace pairing {

namespace {

enum class Source { Fixed, Saved, External };

constexpr const char *kNamespace = "mrcool";
constexpr uint8_t kCategoryAirConditioner = 21;  // HAP accessory category

Source source_ = Source::Saved;
bool fixedConfigured_ = false;
char code_[9] = "";  // plain code, empty when unknown
char setupId_[5] = "HSPN";

// Fingerprint of the SRP verifier HomeSpan derives from the setup code.
// It changes whenever the code is changed by anyone. 0 = none stored.
uint32_t verifierFingerprint() {
  nvs_handle_t h;
  if (nvs_open("SRP", NVS_READONLY, &h) != ESP_OK)
    return 0;
  uint32_t crc = 0;
  size_t len = 0;
  if (nvs_get_blob(h, "VERIFYDATA", nullptr, &len) == ESP_OK && len > 0) {
    uint8_t *buf = static_cast<uint8_t *>(malloc(len));
    if (buf && nvs_get_blob(h, "VERIFYDATA", buf, &len) == ESP_OK)
      crc = esp_rom_crc32_le(0, buf, len) | 1;  // never 0 when present
    free(buf);
  }
  nvs_close(h);
  return crc;
}

// Copies the digits of `in` to `out`; true if there were exactly 8.
bool normalize(const char *in, char out[9]) {
  int n = 0;
  for (const char *p = in; *p; p++) {
    if (*p >= '0' && *p <= '9') {
      if (n == 8)
        return false;
      out[n++] = *p;
    } else if (*p != '-' && *p != ' ') {
      return false;
    }
  }
  out[n] = '\0';
  return n == 8;
}

void randomCode(char out[9]) {
  do {
    snprintf(out, 9, "%08lu", (unsigned long)(esp_random() % 100000000UL));
  } while (!isAllowed(out));
}

void remember(const char *code) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("code", code);
  prefs.putUInt("vcrc", verifierFingerprint());
  prefs.end();
}

void apply(const char *digits) {
  homeSpan.setPairingCode(digits);  // already validated, so this cannot halt
  strlcpy(code_, digits, sizeof(code_));
  remember(digits);
}

}  // namespace

bool isAllowed(const char *d) {
  if (strlen(d) != 8)
    return false;
  bool allSame = true;
  for (int i = 1; i < 8; i++)
    allSame &= d[i] == d[0];
  return !allSame && strcmp(d, "12345678") != 0 && strcmp(d, "87654321") != 0;
}

void begin(const char *fixedCode, const char *setupId) {
  strlcpy(setupId_, setupId, sizeof(setupId_));

  Preferences prefs;
  prefs.begin(kNamespace, false);
  const String stored = prefs.getString("code", "");
  const uint32_t storedFp = prefs.getUInt("vcrc", 0);
  prefs.end();
  const uint32_t fp = verifierFingerprint();

  char digits[9];
  if (fixedCode && *fixedCode) {
    if (normalize(fixedCode, digits) && isAllowed(digits)) {
      fixedConfigured_ = true;
      source_ = Source::Fixed;
      if (stored != digits || fp != storedFp)
        apply(digits);
      else
        strlcpy(code_, digits, sizeof(code_));
      return;
    }
    Serial.printf("\n[mrcool] HOMEKIT_SETUP_CODE \"%s\" is not usable (needs 8 digits and "
                  "must not be trivial like 12345678). Falling back to a random code.\n",
                  fixedCode);
  }

  if (storedFp != 0 && fp == storedFp) {
    // Nothing changed since we last looked.
    source_ = stored.length() == 8 ? Source::Saved : Source::External;
    strlcpy(code_, stored.length() == 8 ? stored.c_str() : "", sizeof(code_));
    return;
  }

  if (storedFp != 0 && fp != 0) {
    // Changed via HomeSpan's `S` command or the setup access point. Keep
    // the user's choice; we just can't display it.
    source_ = Source::External;
    code_[0] = '\0';
    remember("");
    return;
  }

  // First boot with this firmware: create a code. Wi-Fi is not running
  // yet, so turn on the hardware entropy source for esp_random().
  bootloader_random_enable();
  randomCode(digits);
  bootloader_random_disable();
  source_ = Source::Saved;
  apply(digits);
}

bool setCode(const char *code, Print &out) {
  char digits[9];
  if (!normalize(code, digits) || !isAllowed(digits)) {
    out.printf("\n*** '%s' is not a valid setup code. Use 8 digits, e.g. @p 31415926 "
               "(trivial codes like 12345678 are rejected).\n\n", code);
    return false;
  }
  apply(digits);
  source_ = Source::Saved;
  printBanner(out);
  if (fixedConfigured_)
    out.printf("    Note: HOMEKIT_SETUP_CODE is set at build time and will replace "
               "this code at the next boot.\n\n");
  return true;
}

void regenerate(Print &out) {
  char digits[9];
  randomCode(digits);  // Wi-Fi is up by now, so esp_random() is hardware-seeded
  setCode(digits, out);
}

void printBanner(Print &out) {
  const int controllers = std::distance(homeSpan.controllerListBegin(), homeSpan.controllerListEnd());

  out.printf("\n------------------------------------------------------------\n");
  if (code_[0]) {
    const char *how = source_ == Source::Fixed ? "fixed in firmware" : "saved in flash";
    out.printf("  HomeKit setup code : %.3s-%.2s-%.3s   (%s)\n", code_, code_ + 3, code_ + 5, how);
    HapQR qr;
    out.printf("  QR setup payload   : %s\n", qr.get(atoi(code_), setupId_, kCategoryAirConditioner));
    out.printf("                       (make qr CODE=%s SETUP_ID=%s draws it)\n", code_, setupId_);
  } else {
    out.printf("  HomeKit setup code : set via HomeSpan 'S' command or setup AP (not stored here)\n");
    out.printf("                       Type @n for a new random code, or @p <8 digits>.\n");
  }
  if (controllers > 0)
    out.printf("  Pairing            : paired with %d controller(s)\n", controllers);
  else
    out.printf("  Pairing            : NOT PAIRED - in the Home app tap + > Add Accessory\n"
               "                       > More options..., pick this device, enter the code\n");
  out.printf("------------------------------------------------------------\n\n");
}

}  // namespace pairing
}  // namespace mrcool
