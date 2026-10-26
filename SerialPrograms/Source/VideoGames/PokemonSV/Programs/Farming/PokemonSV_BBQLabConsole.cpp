/*  BBQ Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <QDir>
#include <QFileInfo>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_HomeToDateTime.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_RollDateForward1.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonSV/PokemonSV_Settings.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "VideoGames/PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryQuests.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryCatchPhoto.h"
#include "PokemonSV_BBQLab.h"
#include "PokemonSV_BBQLabConsole.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;


std::vector<std::string> bbq_console_words(const std::string& line){
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string word;
    while (in >> word){
        if (word[0] == '#'){
            break;
        }
        words.push_back(word);
    }
    return words;
}


BBQLabConsole_Descriptor::BBQLabConsole_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:BBQLabConsole",
        STRING_POKEMON + " SV", "BBQ Lab Console",
        "",
        "Lab: run command files from ~/BBQLabConsole/inbox on the Switch.",
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PanelDeprecation::NOT_DEPRECATED,
        {}
    )
{}

BBQLabConsole::BBQLabConsole(){
    PA_ADD_OPTION(BBQ_OPTIONS);
}

Button bbq_console_buttons(const std::string& text){
    static const std::map<std::string, Button> NAMES{
        {"A", BUTTON_A}, {"B", BUTTON_B}, {"X", BUTTON_X}, {"Y", BUTTON_Y},
        {"L", BUTTON_L}, {"R", BUTTON_R}, {"ZL", BUTTON_ZL}, {"ZR", BUTTON_ZR},
        {"PLUS", BUTTON_PLUS}, {"MINUS", BUTTON_MINUS},
        {"LCLICK", BUTTON_LCLICK}, {"RCLICK", BUTTON_RCLICK},
        {"HOME", BUTTON_HOME}, {"CAPTURE", BUTTON_CAPTURE},
        {"UP", BUTTON_UP}, {"DOWN", BUTTON_DOWN}, {"LEFT", BUTTON_LEFT}, {"RIGHT", BUTTON_RIGHT},
    };
    Button buttons = BUTTON_NONE;
    std::istringstream in(text);
    std::string name;
    while (std::getline(in, name, '+')){
        auto it = NAMES.find(name);
        if (it == NAMES.end()){
            throw ParseException("Unknown button: " + name);
        }
        buttons = (Button)(buttons | it->second);
    }
    return buttons;
}

namespace{

class CommandRunner{
public:
    CommandRunner(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context, const BBQOption& options,
        std::string out_prefix, bool& position_known
    )
        : m_env(env), m_context(context), m_options(options), m_out_prefix(std::move(out_prefix))
        , m_position_known(position_known)
    {}

    //  Returns false for "stop".
    bool run(const std::vector<std::string>& w, std::ostream& out){
        const std::string& cmd = w[0];
        auto num = [&](size_t i, double fallback = 0){ return i < w.size() ? std::stod(w[i]) : fallback; };
        auto ms = [&](size_t i, double fallback = 0){ return Milliseconds((int64_t)num(i, fallback)); };
        auto info = m_env.program_info();
        ConsoleHandle& console = m_env.console;
        ProControllerContext& context = m_context;

        //  The routes from Central steer the map cursor blind: fail fast until
        //  "central" or "away" says where the player is (a "coastal" from the
        //  Polar area pressed A on empty map for minutes).
        static const std::set<std::string> FROM_CENTRAL{"coastal", "savanna", "class1", "tera-self", "sneak-up"};
        if (FROM_CENTRAL.contains(cmd)){
            if (!m_position_known){
                throw ParseException(cmd + ": start position unknown. Run \"central\" (or \"away\") first.");
            }
            //  It leaves Central Plaza without telling the plaza tracker.
            m_position_known = false;
        }

        if (cmd == "stop"){
            return false;
        }else if (cmd == "press"){                          //  press BTN[+BTN] hold [release]
            pbf_press_button(context, bbq_console_buttons(w.at(1)), ms(2, 160), ms(3, 160));
        }else if (cmd == "stick"){                          //  stick L|R x y hold [release]
            JoystickPosition position{num(2), num(3)};
            if (w.at(1) == "R"){
                pbf_move_right_joystick(context, position, ms(4), ms(5));
            }else{
                pbf_move_left_joystick(context, position, ms(4), ms(5));
            }
        }else if (cmd == "ssf_press"){                      //  ssf_press BTN delay hold
            ssf_press_button(context, bbq_console_buttons(w.at(1)), ms(2), ms(3, 160));
        }else if (cmd == "ssf_stick"){                      //  ssf_stick L|R x y delay hold
            JoystickPosition position{num(2), num(3)};
            if (w.at(1) == "R"){
                ssf_press_right_joystick(context, position, ms(4), ms(5));
            }else{
                ssf_press_left_joystick(context, position, ms(4), ms(5));
            }
        }else if (cmd == "wait"){                           //  wait ms
            pbf_wait(context, ms(1));
        }else if (cmd == "shot"){                           //  shot label
            context.wait_for_all_requests();
            VideoSnapshot screen = console.video().snapshot();
            const std::string path = m_out_prefix + "_" + w.at(1) + ".png";
            if (screen.frame){
                screen.frame->save(path);
            }
            out << "shot " << path << "\n";
        }else if (cmd == "shots"){                          //  shots label count interval_ms, while queued moves run
            const int count = (int)num(2, 10);
            for (int i = 0; i < count; i++){
                if (i > 0){
                    context.wait_for(ms(3, 300));
                }
                VideoSnapshot screen = console.video().snapshot();
                char suffix[16];
                snprintf(suffix, sizeof(suffix), "_%02d.png", i);
                if (screen.frame){
                    screen.frame->save(m_out_prefix + "_" + w.at(1) + suffix);
                }
            }
            out << "shots " << m_out_prefix << "_" << w.at(1) << "_NN.png x" << count << "\n";
        }else if (cmd == "map"){
            open_map_from_overworld(info, console, context);
        }else if (cmd == "fly_here"){
            out << "flew " << fly_to_overworld_from_map(info, console, context, true) << "\n";
        }else if (cmd == "fly_closest"){
            open_map_from_overworld(info, console, context);
            fly_to_closest_pokecenter_on_map(info, console, context, 2);
        }else if (cmd == "fly_fast"){                       //  fly to the closest visible fast-travel (Terarium) fly point
            open_map_from_overworld(info, console, context);
            const bool flew = fly_to_visible_closest_flypoint_cur_zoom_level(info, console, context, FlyPoint::FAST_TRAVEL);
            out << "fly_fast " << (flew ? 1 : 0) << "\n";
            if (!flew){
                press_Bs_to_back_to_overworld(info, console, context);
            }
        }else if (cmd == "jgf"){                            //  jgf hold_up flight_wait drop_time: jump_glide_fly()
            jump_glide_fly(console, context, m_options.INVERTED_FLIGHT, ms(1, 1600), ms(2, 2000), ms(3, 3200));
        }else if (cmd == "central"){
            return_to_plaza(info, console, context, m_options.LANGUAGE);
            m_position_known = true;
        }else if (cmd == "at_central"){                     //  standing on the Central Plaza fly point: no flight
            reset_plaza_position();
            m_position_known = true;
        }else if (cmd == "away"){                           //  next central_to_*() flies from here
            stay_away_from_plaza(m_options.LANGUAGE);
            m_position_known = true;
        }else if (cmd == "class1"){                         //  Polar Classroom 1; directly after "away"
            central_to_polar_class1(info, console, context);
        }else if (cmd == "coastal"){
            central_to_coastal_plaza(info, console, context);
        }else if (cmd == "savanna"){
            central_to_savanna_plaza(info, console, context);
        }else if (cmd == "back"){
            press_Bs_to_back_to_overworld(info, console, context);
        }else if (cmd == "battle?"){                        //  battle? [seconds]
            context.wait_for_all_requests();
            NormalBattleMenuWatcher battle(COLOR_YELLOW);
            const int ret = wait_until(console, context, std::chrono::seconds((int)num(1, 2)), {battle});
            out << "battle " << (ret == 0 ? 1 : 0) << "\n";
        }else if (cmd == "opponent"){
            std::string names;
            for (const std::string& slug : read_singles_opponent(info, console, context, m_options.LANGUAGE)){
                names += (names.empty() ? "" : "|") + slug;
            }
            out << "opponent " << names << "\n";
        }else if (cmd == "escape"){                         //  escape: the farmer's wild-battle exit, run 3 times then fight
            out << "escape " << (int)escape_wild_battle(info, console, context) << "\n";
        }else if (cmd == "flee"){
            out << "flee_attempts " << run_from_battle(console, context) << "\n";
        }else if (cmd == "lookaround"){                     //  lookaround [first_wait_s]
            out << "battle " << send_out_and_look_around(console, context, std::chrono::seconds((int)num(1, 6)), "a wild Pokemon") << "\n";
        }else if (cmd == "dayskip"){
            pbf_press_button(context, BUTTON_HOME, 160ms, GameSettings::instance().GAME_TO_HOME_DELAY1);
            home_to_date_time(console, context, true);
            roll_date_forward_1(console, context, true);
            pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
            resume_game_from_home(console, context);
        }else if (cmd == "hill"){                           //  Coastal Plaza fly point -> middle of the hill
            coastal_plaza_to_grassland(console, context, m_options.INVERTED_FLIGHT);
        }else if (cmd == "quest"){                          //  quest slug[,slug...] [max_rerolls]: reroll blue slots until one is on the board, run it, check the board
            std::set<BBQuests> targets;
            std::istringstream slugs(w.at(1));
            for (std::string slug; std::getline(slugs, slug, ',');){
                targets.insert(BBQuests_string_to_enum(slug));
            }
            const int max_rerolls = (int)num(2, 15);
            std::vector<BBQuests> quests = read_quests(info, console, context, m_options);
            auto drawn = [&]{
                for (BBQuests quest : quests){
                    if (targets.contains(quest)){
                        return quest;
                    }
                }
                return BBQuests::UnableToDetect;
            };
            int rerolls = 0;
            while (drawn() == BBQuests::UnableToDetect){
                if (rerolls >= max_rerolls){
                    throw ParseException("quest: " + w.at(1) + " not drawn after " + std::to_string(rerolls) + " rerolls.");
                }
                //A blue slot (10 BP); leave red ones (50 BP).
                size_t slot = quests.size();
                for (size_t i = 0; i < quests.size(); i++){
                    if (reroll_cost(quests[i]) == 10){
                        slot = i;
                        break;
                    }
                }
                if (slot == quests.size()){
                    throw ParseException("quest: no blue slot to reroll.");
                }
                reroll_quest_slot(info, console, context, m_options, quests, slot);
                rerolls++;
                quests = read_quests(info, console, context, m_options);
            }
            const BBQuests target = drawn();
            out << "rerolls " << rerolls << "\n";
            uint8_t eggs = 0;
            const bool done = process_and_do_quest(m_env, console, context, const_cast<BBQOption&>(m_options), target, eggs);
            out << "quest " << BBQuests_enum_to_string(target) << " " << (done ? "completed" : "not completed") << "\n";
        }else if (cmd == "catchbattle"){                    //  catchbattle slug: in a battle, run the catch quest's battle handler
            bbq_lab_flags().quest_label = w.at(1);
            quest_catch_handle_battle(info, console, context, m_options, BBQuests_string_to_enum(w.at(1)));
        }else if (cmd == "flydirect"){                      //  flydirect 0|1|-1: override FLY_DIRECTLY (-1: use the option)
            bbq_lab_flags().fly_directly_override = (int)num(1, -1);
        }else if (cmd == "run"){                            //  run slug: a photo or catch quest's real action from Central, without the board
            if (!m_position_known){
                throw ParseException("run: start position unknown. Run \"central\" (or \"at_central\") first.");
            }
            const BBQuests quest = BBQuests_string_to_enum(w.at(1));
            bbq_lab_flags().quest_label = w.at(1);
            m_position_known = false;
            if (w.at(1).starts_with("photo-")){
                quest_photo(info, console, context, m_options, quest);
            }else if (w.at(1).starts_with("catch-")){
                quest_catch(info, console, context, m_options, quest);
            }else{
                throw ParseException("run: only photo-* and catch-* quests.");
            }
            //Both end at Central Plaza, or with flydirect 1 at the closest fly point,
            //where the plaza tracker knows to fly from anywhere.
            m_position_known = true;
        }else if (cmd == "tera-self"){
            quest_tera_self_defeat(info, console, context, m_options);
        }else if (cmd == "sneak-up"){
            quest_sneak_up(info, console, context, m_options);
        }else{
            throw ParseException("Unknown command: " + cmd);
        }
        return true;
    }

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    const BBQOption& m_options;
    std::string m_out_prefix;
    bool& m_position_known;
};

}


void BBQLabConsole::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const QString root = QDir::homePath() + "/BBQLabConsole";
    QDir().mkpath(root + "/inbox");
    QDir().mkpath(root + "/outbox");
    QDir().mkpath(root + "/done");
    env.log("Lab console: waiting for command files in " + root.toStdString() + "/inbox");

    bool position_known = false;

    //Every quest step the console runs saves its lab events, for replaying failed quests.
    struct HookGuard{
        ~HookGuard(){ set_bbq_lab_event_hook(nullptr); }
    } hook_guard;
    const std::string shot_dir = SCREENSHOTS_PATH() + "BBQReference/console-" + now_to_filestring() + "/";
    QDir().mkpath(QString::fromStdString(shot_dir));
    env.log("Saving reference screenshots to " + shot_dir);
    set_bbq_lab_event_hook([shot_dir](VideoStream& stream, ProControllerContext& context, const std::string& event){
        context.wait_for_all_requests();
        VideoSnapshot screen = stream.video().snapshot();
        if (screen.frame){
            const std::string& quest = bbq_lab_flags().quest_label;
            screen.frame->save(shot_dir + now_to_filestring() + "-" + (quest.empty() ? "none" : quest) + "-" + event + ".png");
        }
    });

    while (true){
        QFileInfoList files = QDir(root + "/inbox").entryInfoList({"*.txt"}, QDir::Files, QDir::Name);
        if (files.isEmpty()){
            context.wait_for(std::chrono::milliseconds(500));
            continue;
        }
        const QFileInfo file = files.front();
        const std::string name = file.completeBaseName().toStdString();
        const std::string out_prefix = (root + "/outbox/").toStdString() + name;
        std::ifstream in(file.absoluteFilePath().toStdString());
        std::ofstream out(out_prefix + ".tmp");
        env.log("Lab console: running " + name);

        CommandRunner runner(env, context, BBQ_OPTIONS, out_prefix, position_known);
        bool keep_going = true;
        std::string line;
        size_t line_number = 0;
        const WallClock file_start = current_time();
        while (keep_going && std::getline(in, line)){
            line_number++;
            const std::vector<std::string> words = bbq_console_words(line);
            if (words.empty()){
                continue;
            }
            const WallClock start = current_time();
            try{
                keep_going = runner.run(words, out);
                //ssf_* lines overlap the lines after them, and shots samples while they run.
                if (words[0].rfind("ssf_", 0) != 0 && words[0] != "shots"){
                    context.wait_for_all_requests();
                }
            }catch (OperationFailedException& e){
                out << "error line " << line_number << ": " << e.message() << "\n";
                break;
            }catch (ParseException& e){
                out << "error line " << line_number << ": " << e.message() << "\n";
                break;
            }catch (std::exception& e){
                out << "error line " << line_number << ": " << e.what() << "\n";
                break;
            }
            const double t0 = std::chrono::duration<double>(start - file_start).count();
            const double t1 = std::chrono::duration<double>(current_time() - file_start).count();
            out << "ok " << line_number << " " << line << " @" << (int)(t0 * 1000) << "-" << (int)(t1 * 1000) << "ms\n";
            out.flush();
        }
        out << "end\n";
        out.close();
        in.close();
        //QFile::rename() does not overwrite: a rerun under the same name kept the old results.
        QFile::remove(QString::fromStdString(out_prefix + ".out"));
        QFile::rename(QString::fromStdString(out_prefix + ".tmp"), QString::fromStdString(out_prefix + ".out"));
        QFile::remove(root + "/done/" + file.fileName());
        QFile::remove(root + "/done/" + file.fileName());
        QFile::rename(file.absoluteFilePath(), root + "/done/" + file.fileName());
        if (!keep_going){
            env.log("Lab console: stop.");
            return;
        }
    }
}


}
}
}
