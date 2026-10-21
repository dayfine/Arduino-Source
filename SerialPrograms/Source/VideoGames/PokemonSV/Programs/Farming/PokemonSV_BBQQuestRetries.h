// Retry state belongs to a running farmer, not to an individual batch.
#pragma once
#include <algorithm>
#include <cstddef>
#include <map>
#include <vector>
namespace PokemonAutomation::NintendoSwitch::PokemonSV{
enum class BBQuests;
class BBQQuestRetries{
public:
    bool can_attempt(BBQuests quest, unsigned retries) const{
        auto it = m_attempts.find(quest);
        return it == m_attempts.end() || it->second < (std::size_t)retries + 1;
    }
    void record_attempt(BBQuests quest){ m_attempts[quest]++; }
    void completed(BBQuests quest){ m_attempts.erase(quest); }
    // Only prune absent quests from a complete, confidently read board.
    void observe_board(const std::vector<BBQuests>& board, bool complete){
        if (!complete) return;
        for (auto it = m_attempts.begin(); it != m_attempts.end();){
            if (std::find(board.begin(), board.end(), it->first) == board.end()){
                it = m_attempts.erase(it);
            }else{
                ++it;
            }
        }
    }
private:
    std::map<BBQuests, std::size_t> m_attempts;
};
}
