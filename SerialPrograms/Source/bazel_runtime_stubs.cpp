#include <string>
#include <vector>
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonTools/OCR/OCR_PaddleOCR.h"

namespace PokemonAutomation {

GlobalSettings& GlobalSettings::instance() {
    static GlobalSettings settings;
    return settings;
}

std::vector<std::string> parse_words(const std::string&) { return {}; }

namespace ML { class PaddleOCRPipeline; }
namespace OCR {
bool paddle_ocr_language_available(Language) { return false; }
ML::PaddleOCRPipeline& ensure_paddle_ocr_instance(Language) {
    throw "paddle_ocr stub";
}
void clear_paddle_ocr_cache() {}
std::string paddle_ocr_read(Language, const ImageViewRGB32&, PageSegMode) { return {}; }
std::string normalize_utf32(const std::string& s) { return s; }
}  // namespace OCR

}  // namespace PokemonAutomation
