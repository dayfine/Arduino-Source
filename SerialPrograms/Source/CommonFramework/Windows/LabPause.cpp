/*  Lab Pause
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <cstdlib>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/FilePath.h"
#include "Common/Cpp/ScopeExit.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "LabPause.h"

namespace PokemonAutomation{

using namespace std::chrono_literals;


namespace{

//  How often a paused program looks for the continue file.
constexpr std::chrono::milliseconds LOOK_INTERVAL = 100ms;

struct PauseState{
    Mutex lock;
    std::string label;
    size_t skips = 0;
};

PauseState& pause_state(){
    static PauseState state;
    return state;
}

}


size_t parse_lab_continue(const std::string& text){
    size_t end = text.size();
    while (end > 0 && std::isspace((unsigned char)text[end - 1])){
        end--;
    }
    size_t begin = end;
    while (begin > 0 && std::isdigit((unsigned char)text[begin - 1])){
        begin--;
    }
    if (begin == end){
        return 0;
    }
    return (size_t)std::strtoull(text.substr(begin, end - begin).c_str(), nullptr, 10);
}

void lab_pause_set_skips(size_t skips){
    PauseState& state = pause_state();
    std::lock_guard<Mutex> lg(state.lock);
    state.skips = skips;
}

std::string lab_paused_label(){
    PauseState& state = pause_state();
    std::lock_guard<Mutex> lg(state.lock);
    return state.label;
}

void lab_pause(std::string_view label, const LabPauseIO& io){
    PauseState& state = pause_state();
    {
        std::lock_guard<Mutex> lg(state.lock);
        if (state.skips > 0){
            state.skips--;
            return;
        }
        state.label = std::string(label);
    }
    ScopeExit clear([&]{
        std::lock_guard<Mutex> lg(state.lock);
        state.label.clear();
    });
    while (true){
        const std::optional<std::string> text = io.take_continue();
        if (text){
            lab_pause_set_skips(parse_lab_continue(*text));
            return;
        }
        io.sleep();
    }
}

void lab_pause_wait(std::string_view label, CancellableScope& scope){
    const std::string path = USER_FILE_PATH() + "Lab/continue";
    LabPauseIO io;
    io.take_continue = [&]() -> std::optional<std::string>{
        const Filesystem::Path file(path);
        if (!Filesystem::exists(file)){
            return std::nullopt;
        }
        std::string text;
        file_to_string(path, text);
        Filesystem::remove(file);
        return text;
    };
    io.sleep = [&]{
        scope.wait_for(LOOK_INTERVAL);
    };
    lab_pause(label, io);
}



}
