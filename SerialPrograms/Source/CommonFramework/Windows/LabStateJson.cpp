/*  Lab State JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string_view>
#include <vector>
#include "LabStateJson.h"

namespace PokemonAutomation{

namespace{


//  JSON text in the contract's canonical form, which is Python's
//  json.dumps(indent=2, ensure_ascii=False): keys in the order written,
//  "key": value, one member per line. The project's JsonObject sorts its
//  keys, so it can't write this. Values come from the json_*() functions.
class OrderedJsonWriter{
public:
    //  Opens an object as the value of `key`; the outermost object has key "".
    void open(std::string_view key);
    void close();
    //  `value` is JSON text.
    void value(std::string_view key, std::string_view value);

    //  The whole document, after the outermost close().
    std::string text() const{
        return std::format("{}\n", m_text);
    }

private:
    void member(std::string_view key);
    void newline();

    std::string m_text;
    std::vector<size_t> m_counts;   //  Members written so far, per open object.
};

std::string json_string(std::string_view text){
    std::string out = "\"";
    for (char ch : text){
        switch (ch){
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        default:
            if ((unsigned char)ch < 0x20){
                out += std::format("\\u{:04x}", (unsigned)(unsigned char)ch);
            }else{
                out += ch;
            }
        }
    }
    out += '"';
    return out;
}

void OrderedJsonWriter::open(std::string_view key){
    member(key);
    m_text += "{";
    m_counts.push_back(0);
}
void OrderedJsonWriter::close(){
    const bool empty = m_counts.back() == 0;
    m_counts.pop_back();
    if (!empty){
        newline();
    }
    m_text += "}";
}
void OrderedJsonWriter::value(std::string_view key, std::string_view value){
    member(key);
    m_text += value;
}
void OrderedJsonWriter::member(std::string_view key){
    if (m_counts.empty()){
        return;
    }
    if (m_counts.back()++ > 0){
        m_text += ",";
    }
    newline();
    m_text += json_string(key);
    m_text += ": ";
}
void OrderedJsonWriter::newline(){
    m_text += "\n";
    m_text.append(2 * m_counts.size(), ' ');
}


std::string json_nullable(const std::optional<std::string>& text){
    return text ? json_string(*text) : "null";
}
std::string json_timestamp(const std::optional<WallClock>& time){
    return time ? json_string(lab_timestamp_text(*time)) : "null";
}

//  The shortest text that reads back as `value`, as Python writes it.
std::string json_number(double value){
    if (!std::isfinite(value)){
        return "null";
    }
    std::array<char, 32> buffer{};
    for (int precision = 1; precision <= 17; precision++){
        std::snprintf(buffer.data(), buffer.size(), "%.*g", precision, value);
        if (std::strtod(buffer.data(), nullptr) == value){
            break;
        }
    }
    std::string text(buffer.data());
    if (text.find_first_of(".e") == std::string::npos){
        text += ".0";
    }
    return text;
}


std::string lower_case(std::string_view text){
    std::string out;
    for (char ch : text){
        out += ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
    }
    return out;
}

std::string_view controller_name(std::string_view legacy){
    if (legacy == "ready"){
        return "ready";
    }
    if (legacy == "none"){
        return "not_connected";
    }
    return "no_console";
}

std::string_view video_state_name(LabVideoState state){
    switch (state){
    case LabVideoState::LIVE:
        return "live";
    case LabVideoState::STALE:
        return "stale";
    case LabVideoState::MISSING:
        return "missing";
    }
    return "missing";
}

std::string_view source_name(LabTransitionSource source){
    switch (source){
    case LabTransitionSource::UI:
        return "ui";
    case LabTransitionSource::LAB:
        return "lab";
    case LabTransitionSource::PROGRAM:
        return "program";
    case LabTransitionSource::PROGRAM_ERROR:
        return "error";
    }
    return "ui";
}

std::optional<std::string> unless_empty(const std::string& text){
    return text.empty() ? std::nullopt : std::optional<std::string>(text);
}


void write_transition(const std::optional<LabTransition>& transition, OrderedJsonWriter& json){
    if (!transition){
        json.value("last_transition", "null");
        return;
    }
    json.open("last_transition");
    json.value("action", json_string(transition->action == LabTransitionAction::START ? "start" : "stop"));
    json.value("source", json_string(source_name(transition->source)));
    json.value("request_id", json_nullable(transition->request_id));
    json.value("at", json_timestamp(transition->at));
    json.close();
}

void write_program(const LabAppState& state, OrderedJsonWriter& json){
    json.open("program");
    json.value("state", json_string(lower_case(state.legacy.state)));
    json.value("run_id", "null");
    write_transition(state.last_transition, json);
    if (state.paused){
        json.value("paused", json_string(*state.paused));
    }
    json.close();
}

void write_video(const LabAppState& state, OrderedJsonWriter& json){
    json.open("video");
    json.value("state", json_string(video_state_name(state.video)));
    json.value("frame_seq", state.frame ? std::to_string(state.frame->seq) : "null");
    json.value("captured_at", json_timestamp(state.frame ? std::optional<WallClock>(state.frame->captured_at) : std::nullopt));
    json.value("signal", json_string("unknown"));
    json.close();
}

void write_observation(std::string_view key, const LabObservation& observation, OrderedJsonWriter& json){
    json.open(key);
    json.value("value", json_string(observation.value));
    json.value("observed_at", json_timestamp(observation.observed_at));
    if (observation.detector){
        json.value("detector", json_string(*observation.detector));
    }
    if (observation.confidence){
        json.value("confidence", json_number(*observation.confidence));
    }
    json.close();
}

//  Unlike the legacy file, which names any selected panel, "panel.id" names
//  only a program.
void write_panel(const LabState& legacy, OrderedJsonWriter& json){
    const bool is_program = legacy.state != "NO_PROGRAM";
    json.open("panel");
    json.value("id", json_nullable(is_program ? unless_empty(legacy.program) : std::nullopt));
    json.value("kind", json_string(is_program ? "program" : "other"));
    json.close();
}


}  // namespace



std::string lab_timestamp_text(WallClock time){
    using namespace std::chrono;
    const sys_days day = floor<days>(time);
    const year_month_day date(day);
    const hh_mm_ss<milliseconds> clock(floor<milliseconds>(time - day));
    return std::format(
        "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
        (int)date.year(), (unsigned)date.month(), (unsigned)date.day(),
        clock.hours().count(), clock.minutes().count(), clock.seconds().count(), clock.subseconds().count()
    );
}


LabVideoState lab_video_state(bool has_video, const std::optional<LabFrameToken>& frame, WallClock now, Milliseconds max_age){
    if (!has_video || !frame){
        return LabVideoState::MISSING;
    }
    return now - frame->captured_at <= max_age ? LabVideoState::LIVE : LabVideoState::STALE;
}


std::string lab_state_json_text(const LabAppState& state){
    OrderedJsonWriter json;
    json.open("");
    json.value("schema", "1");
    json.value("written_at", json_timestamp(state.written_at));

    json.open("instance");
    json.value("id", json_string(state.instance_id));
    json.value("pid", std::to_string(state.pid));
    if (state.profile){
        json.value("profile", json_string(*state.profile));
    }
    json.close();

    write_panel(state.legacy, json);
    write_program(state, json);
    json.open("controller");
    json.value("state", json_string(controller_name(state.legacy.controller)));
    json.close();
    write_video(state, json);

    json.open("observation");
    write_observation("game", state.game, json);
    write_observation("screen", state.screen, json);
    json.close();

    json.open("channel");
    json.value("last_request", json_nullable(unless_empty(state.legacy.seq)));
    json.value("last_error", json_nullable(unless_empty(state.legacy.error)));
    json.value("owner_claimed", state.legacy.owner.empty() ? "false" : "true");
    json.close();

    json.close();
    return json.text();
}



}  // namespace PokemonAutomation
