// Studio 87 Gear Updater link: the product id and the exact link the About button opens (no JUCE needed).
#include "test_util.h"
#include "../plugin/Source/Editor/Studio87Link.h"
#include <cctype>
#include <string>

TEST(updater_link_product_and_url) {
    const std::string product = "JPurrr-8000";
    bool charsOk = ! product.empty() && product.size() <= 100;
    for (char ch : product) charsOk = charsOk && (std::isalnum((unsigned char) ch) || ch == '.' || ch == '-' || ch == '_');
    CHECK(charsOk);
    CHECK(studio87link::updaterUrl(product) == "studio87://open?product=JPurrr-8000");
    CHECK(studio87link::updaterUrl("") == "studio87://open");
    CHECK(studio87link::updaterUrl(product).find("http") == std::string::npos);
    CHECK(studio87link::githubUrl(product) == "https://github.com/SpeakingColors/JPurrr-8000");
    CHECK(studio87link::githubUrl(product).find("/releases") == std::string::npos);
}

TEST(updater_link_texts) {
    CHECK(std::string(studio87link::fallbackMessage()) == "Install Studio 87 Gear Updater from github.com/SpeakingColors/gearupdater/releases");
    CHECK(std::string(studio87link::tipText()).find("Nothing is sent anywhere.") != std::string::npos);
    CHECK(std::string(studio87link::tipText()).find("\xE2\x80\x94") == std::string::npos);
    CHECK(std::string(studio87link::githubButtonText()) == "Open on GitHub");
    CHECK(std::string(studio87link::githubTipText()) == "Opens this plugin's home page on GitHub in your browser. Nothing is sent by the plugin.");
    CHECK(std::string(studio87link::fallbackNote()).find("Gear Updater not found, opened the GitHub page instead.") == 0);
    CHECK(std::string(studio87link::fallbackNote()).find("github.com/SpeakingColors/gearupdater/releases") != std::string::npos);
    CHECK(std::string(studio87link::githubTipText()).find("\xE2\x80\x94") == std::string::npos);
    CHECK(std::string(studio87link::fallbackNote()).find("\xE2\x80\x94") == std::string::npos);
}
