#include <fcitx-utils/key.h>
#include <fcitx/addonfactory.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kPageSize = 5;

std::string projectDir() {
    const char *env = std::getenv("UYKENLM_DIR");
    if (env && *env) {
        return env;
    }
    return "/";
}

bool isUyghurArabic(const std::string &text) {
    for (unsigned char ch : text) {
        if (ch >= 0xd8) {
            return true;
        }
    }
    return false;
}

bool isTrimByte(unsigned char ch) {
    return ch <= 0x20 || ch == '.' || ch == ',' || ch == ';' || ch == ':' ||
           ch == '!' || ch == '?' || ch == '"' || ch == '\'' || ch == '(' ||
           ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}

std::string cleanToken(std::string token) {
    while (!token.empty() && isTrimByte(static_cast<unsigned char>(token.front()))) {
        token.erase(token.begin());
    }
    while (!token.empty() && isTrimByte(static_cast<unsigned char>(token.back()))) {
        token.pop_back();
    }
    static const std::vector<std::string> punct = {"،", "؛", "؟", "»", "«", "›", "‹"};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &p : punct) {
            if (token.size() >= p.size() && token.rfind(p, 0) == 0) {
                token.erase(0, p.size());
                changed = true;
            }
            if (token.size() >= p.size() &&
                token.compare(token.size() - p.size(), p.size(), p) == 0) {
                token.erase(token.size() - p.size());
                changed = true;
            }
        }
    }
    return token;
}

class Lexicon {
public:
    void load() {
        if (loaded_) {
            return;
        }
        loaded_ = true;
        std::ifstream input(projectDir() + "/1.txt");
        if (!input) {
            return;
        }
        std::unordered_map<std::string, int> counts;
        std::vector<std::string> lines;
        std::string line;
        std::string token;
        while (std::getline(input, line)) {
            lines.push_back(line);
            std::istringstream stream(line);
            while (stream >> token) {
                token = cleanToken(token);
                if (!token.empty() && isUyghurArabic(token)) {
                    counts[token]++;
                }
            }
        }
        rawLines_ = std::move(lines);
        words_.reserve(counts.size());
        for (auto &entry : counts) {
            words_.push_back({std::move(entry.first), entry.second});
        }
        std::sort(words_.begin(), words_.end(), [](const Word &a, const Word &b) {
            if (a.count != b.count) {
                return a.count > b.count;
            }
            if (a.text.size() != b.text.size()) {
                return a.text.size() < b.text.size();
            }
            return a.text < b.text;
        });
    }

    std::vector<std::string> prefix(const std::string &text, int limit) {
        load();
        std::vector<std::string> result;
        auto add = [&](const std::string &candidate) {
            if (candidate.empty()) {
                return;
            }
            if (std::find(result.begin(), result.end(), candidate) == result.end()) {
                result.push_back(candidate);
            }
        };
        add(text);
        for (const auto &word : words_) {
            if (static_cast<int>(result.size()) >= limit) {
                break;
            }
            if (word.text == text) {
                continue;
            }
            if (word.text.rfind(text, 0) == 0) {
                add(word.text);
            }
        }
        if (static_cast<int>(result.size()) >= limit) {
            return result;
        }

        for (const auto &line : rawLines_) {
            if (static_cast<int>(result.size()) >= limit) {
                break;
            }
            if (line.find(text) == std::string::npos) {
                continue;
            }
            std::istringstream stream(line);
            std::string token;
            while (stream >> token) {
                if (static_cast<int>(result.size()) >= limit) {
                    break;
                }
                token = cleanToken(token);
                if (token.empty() || !isUyghurArabic(token)) {
                    continue;
                }
                if (token.rfind(text, 0) == 0 || token.find(text) != std::string::npos) {
                    add(token);
                }
            }
        }
        if (static_cast<int>(result.size()) >= limit) {
            return result;
        }

        for (const auto &word : words_) {
            if (static_cast<int>(result.size()) >= limit) {
                break;
            }
            if (word.text.find(text) != std::string::npos) {
                add(word.text);
            }
        }
        return result;
    }

private:
    struct Word {
        std::string text;
        int count;
    };
    bool loaded_ = false;
    std::vector<Word> words_;
    std::vector<std::string> rawLines_;
};

bool isAsciiInputKey(const fcitx::Key &key) {
    const auto sym = key.sym();
    const auto blocked = fcitx::KeyStates(fcitx::KeyState::Ctrl) |
                         fcitx::KeyState::Alt | fcitx::KeyState::Super |
                         fcitx::KeyState::Super2 | fcitx::KeyState::Hyper |
                         fcitx::KeyState::Meta;
    // return !key.states().testAny(blocked) &&
    //        ((sym >= FcitxKey_a && sym <= FcitxKey_z) ||
    //         (sym >= FcitxKey_A && sym <= FcitxKey_Z) ||
    //         sym == FcitxKey_apostrophe || sym == FcitxKey_minus ||
    //         sym == FcitxKey_bracketleft || sym == FcitxKey_bracketright ||
    //         sym == FcitxKey_semicolon || sym == FcitxKey_comma ||
    //         sym == FcitxKey_period || sym == FcitxKey_slash ||
    //         sym == FcitxKey_9 || sym == FcitxKey_0);
            // sym == FcitxKey_apostrophe || sym == FcitxKey_quotedbl ||
            // sym == FcitxKey_minus || sym == FcitxKey_underscore ||
            // sym == FcitxKey_bracketleft || sym == FcitxKey_braceleft ||
            // sym == FcitxKey_bracketright || sym == FcitxKey_braceright ||
            // sym == FcitxKey_semicolon || sym == FcitxKey_colon ||
            // sym == FcitxKey_comma || sym == FcitxKey_less ||
            // sym == FcitxKey_period || sym == FcitxKey_greater ||
            // sym == FcitxKey_slash || sym == FcitxKey_question ||
            // sym == FcitxKey_9 || sym == FcitxKey_parenleft ||
            // sym == FcitxKey_0 || sym == FcitxKey_parenright);
        return !key.states().testAny(blocked) &&
           ((sym >= FcitxKey_a && sym <= FcitxKey_z) ||
            (sym >= FcitxKey_A && sym <= FcitxKey_Z) ||
            sym == FcitxKey_apostrophe || sym == FcitxKey_quotedbl ||
            sym == FcitxKey_minus || sym == FcitxKey_underscore ||
            sym == FcitxKey_bracketleft || sym == FcitxKey_braceleft ||
            sym == FcitxKey_bracketright || sym == FcitxKey_braceright ||
            sym == FcitxKey_semicolon || sym == FcitxKey_colon ||
            sym == FcitxKey_comma || sym == FcitxKey_less ||
            sym == FcitxKey_period || sym == FcitxKey_greater ||
            sym == FcitxKey_slash || sym == FcitxKey_question ||
            sym == FcitxKey_9 || sym == FcitxKey_parenleft ||
            sym == FcitxKey_0 || sym == FcitxKey_parenright);
}

std::string standardUyghurKeyText(const fcitx::Key &key) {
    //const bool shifted = key.states().test(fcitx::KeyState::Shift);
    // auto sym = key.sym();
    // if (sym >= FcitxKey_A && sym <= FcitxKey_Z && !shifted) {
    //     sym = static_cast<fcitx::KeySym>(sym - FcitxKey_A + FcitxKey_a);
    // }

    auto sym = key.sym();
    bool shifted = false;
    // 如果 sym 是大写字母，说明 Shift 被按下了
    if (sym >= FcitxKey_A && sym <= FcitxKey_Z) {
        shifted = true;
        sym = static_cast<fcitx::KeySym>(sym - FcitxKey_A + FcitxKey_a);
    }


    switch (sym) {
    case FcitxKey_q:
    case FcitxKey_Q:
        return "چ";
    case FcitxKey_w:
    case FcitxKey_W:
        return "ۋ";
    case FcitxKey_e:
    case FcitxKey_E:
        return "ې";
    case FcitxKey_r:
    case FcitxKey_R:
        return "ر";
    case FcitxKey_t:
    case FcitxKey_T:
        return "ت";
    case FcitxKey_y:
    case FcitxKey_Y:
        return "ي";
    case FcitxKey_u:
    case FcitxKey_U:
        return "ۇ";
    case FcitxKey_i:
    case FcitxKey_I:
        return "ڭ";
    case FcitxKey_o:
    case FcitxKey_O:
        return "و";
    case FcitxKey_p:
    case FcitxKey_P:
        return "پ";
    case FcitxKey_a:
    case FcitxKey_A:
        return "ھ";
    case FcitxKey_s:
    case FcitxKey_S:
        return "س";
    case FcitxKey_d:
    case FcitxKey_D:
        return shifted ? "ژ" : "د";
    case FcitxKey_f:
    case FcitxKey_F:
        return shifted ? "ف" : "ا";
    case FcitxKey_g:
    case FcitxKey_G:
        return shifted ? "گ" : "ە";
    case FcitxKey_h:
    case FcitxKey_H:
        return shifted ? "خ" : "ى";
    case FcitxKey_j:
    case FcitxKey_J:
        return shifted ? "ج" : "ق";
    case FcitxKey_k:
    case FcitxKey_K:
        return shifted ? "ۆ" : "ك";
    case FcitxKey_l:
    case FcitxKey_L:
        return "ل";
    case FcitxKey_z:
    case FcitxKey_Z:
        return "ز";
    case FcitxKey_x:
    case FcitxKey_X:
        return "ش";
    case FcitxKey_c:
    case FcitxKey_C:
        return "غ";
    case FcitxKey_v:
    case FcitxKey_V:
        return "ۈ";
    case FcitxKey_b:
    case FcitxKey_B:
        return "ب";
    case FcitxKey_n:
    case FcitxKey_N:
        return "ن";
    case FcitxKey_m:
    case FcitxKey_M:
        return "م";
    case FcitxKey_slash:
        return shifted ? "؟" : "ئ";
    case FcitxKey_comma:
        return shifted ? "›" : "،";
    case FcitxKey_period:
        return shifted ? "‹" : ".";
    case FcitxKey_semicolon:
        return shifted ? ":" : "؛";
    case FcitxKey_bracketright:
        return shifted ? "»" : "]";
    case FcitxKey_bracketleft:
        return shifted ? "«" : "[";
    case FcitxKey_minus:
        return shifted ? "—" : "-";
    case FcitxKey_9:
        return shifted ? ")" : "9";
    case FcitxKey_0:
        return shifted ? "(" : "0";
    default:
        break;
    }

    const std::string utf8 = fcitx::Key::keySymToUTF8(key.sym());
    if (isUyghurArabic(utf8)) {
        return utf8;
    }
    return {};
}

void popUTF8(std::string &text) {
    if (text.empty()) {
        return;
    }
    size_t pos = text.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) {
        --pos;
    }
    text.erase(pos);
}

std::string isolateLTR(std::string text) {
    return "\xe2\x81\xa6" + std::move(text) + "\xe2\x81\xa9";
}

std::string isolateRTL(std::string text) {
    return "\xe2\x81\xa7" + std::move(text) + "\xe2\x81\xa9";
}

// std::string candidateDisplayText(size_t number, const std::string &text) {
//     return isolateLTR(std::to_string(number) + ". " + text);
// }
std::string candidateDisplayText(size_t number, const std::string &text) {
    static const char *eastern[] = {"٠","1","2","3","4","5","6","7","8","9"};
    std::string result;
    if (number < 10) {
        result = eastern[number];
    } else {
        result = std::to_string(number);
        for (auto &ch : result) {
            ch = "٠١٢٣٤٥٦٧٨٩"[ch - '0'];
        }
    }
    result += ". " + text;
    return result;
}

class UyghurCandidateWord : public fcitx::CandidateWord {
public:
    UyghurCandidateWord(std::string text, std::string displayText)
        : fcitx::CandidateWord(fcitx::Text(std::move(displayText))),
          text_(std::move(text)) {}

    void select(fcitx::InputContext *ic) const override {
        if (ic) {
            ic->commitString(text_);
            ic->inputPanel().reset();
            ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        }
    }

private:
    std::string text_;
};

class UyghurKenLMEngine final : public fcitx::InputMethodEngineV3 {
public:
    std::vector<fcitx::InputMethodEntry> listInputMethods() override {
        std::vector<fcitx::InputMethodEntry> entries;
        entries.emplace_back("uyghur-kenlm", "Uyghur KenLM", "ug", "uyghurkenlm");
        entries.back().setNativeName("ئۇيغۇرچە KenLM").setLabel("ئۇ").setIcon("fcitx");
        return entries;
    }

    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override {
        if (event.isRelease()) {
            return;
        }

        auto *ic = event.inputContext();
        const auto key = event.key();
        if (key.check(FcitxKey_Escape)) {
            clear(ic);
            event.filterAndAccept();
            return;
        }

        if (key.check(FcitxKey_BackSpace)) {
            if (!buffer_.empty()) {
                popUTF8(buffer_);
                updateUI(ic);
                event.filterAndAccept();
            }
            return;
        }

        // if (key.check(FcitxKey_space) || key.check(FcitxKey_Return)) {
        //     if (!candidates_.empty()) {
        //         commit(ic, 0);
        //         event.filterAndAccept();
        //     }
        //     return;
        // }
        if (key.check(FcitxKey_space) || key.check(FcitxKey_Return)) {
            if (!candidates_.empty()) {
                commit(ic, 0);
                if (key.check(FcitxKey_space)) {
                    ic->commitString(" ");
                }
                event.filterAndAccept();
            }
            return;
        }

        if (!buffer_.empty() && key.sym() >= FcitxKey_1 &&
            key.sym() <= FcitxKey_5) {
            int index = static_cast<int>(key.sym() - FcitxKey_1);
            if (index >= 0 && index < static_cast<int>(candidates_.size())) {
                commit(ic, index);
                event.filterAndAccept();
            }
            return;
        }

        if (isAsciiInputKey(key)) {
            const auto text = standardUyghurKeyText(key);
            if (!text.empty()) {
                buffer_ += text;
                updateUI(ic);
                event.filterAndAccept();
            }
            return;
        }
    }

    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        clear(event.inputContext());
    }

private:
    void updateUI(fcitx::InputContext *ic) {
        if (!ic) {
            return;
        }
        auto &panel = ic->inputPanel();
        if (buffer_.empty()) {
            panel.reset();
            candidates_.clear();
            ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            return;
        }

        candidates_ = lexicon_.prefix(buffer_, kPageSize);
        if (candidates_.empty()) {
            candidates_.push_back(buffer_);
        }
        auto list = std::make_unique<fcitx::CommonCandidateList>();
        list->setPageSize(kPageSize);
        list->setLayoutHint(fcitx::CandidateLayoutHint::Horizontal);
        std::vector<std::string> labels(candidates_.size(), "");
        for (size_t i = candidates_.size(); i > 0; --i) {
            const auto &candidate = candidates_[i - 1];
            list->append<UyghurCandidateWord>(
                candidate, candidateDisplayText(i, candidate));
        }
        list->setLabels(labels);
        list->setGlobalCursorIndex(static_cast<int>(candidates_.size() - 1));
      
        // fcitx::Text preedit(buffer_);
        // preedit.setCursor(static_cast<int>(buffer_.size()));
        // panel.setPreedit(preedit);
        // panel.setAuxUp(fcitx::Text());
        // panel.setAuxDown(fcitx::Text());
        // panel.setClientPreedit(preedit);
        std::string displayText = "\xE2\x80\x8F" + buffer_;
        fcitx::Text preedit(displayText);
        preedit.setCursor(static_cast<int>(displayText.size()));
        panel.setPreedit(preedit);
        panel.setAuxUp(fcitx::Text());
        panel.setAuxDown(fcitx::Text());
        panel.setClientPreedit(preedit);
        
        panel.setCandidateList(std::move(list));
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void commit(fcitx::InputContext *ic, int index) {
        if (!ic || index < 0 || index >= static_cast<int>(candidates_.size())) {
            return;
        }
        ic->commitString(candidates_[index]);
        buffer_.clear();
        candidates_.clear();
        ic->inputPanel().reset();
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void clear(fcitx::InputContext *ic) {
        buffer_.clear();
        candidates_.clear();
        if (ic) {
            ic->inputPanel().reset();
            ic->updatePreedit();
            ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        }
    }

    std::string buffer_;
    std::vector<std::string> candidates_;
    Lexicon lexicon_;
};

class UyghurKenLMFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *) override {
        return new UyghurKenLMEngine;
    }
};

} // namespace

FCITX_ADDON_FACTORY(UyghurKenLMFactory)
