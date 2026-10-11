#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>
using namespace std;

enum class SubmissionStatus {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

SubmissionStatus parseStatus(const string& status) {
    if (status == "Accepted") return SubmissionStatus::Accepted;
    if (status == "Wrong_Answer") return SubmissionStatus::Wrong_Answer;
    if (status == "Runtime_Error") return SubmissionStatus::Runtime_Error;
    if (status == "Time_Limit_Exceed") return SubmissionStatus::Time_Limit_Exceed;
    return SubmissionStatus::Wrong_Answer; // default
}

string statusToString(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Accepted: return "Accepted";
        case SubmissionStatus::Wrong_Answer: return "Wrong_Answer";
        case SubmissionStatus::Runtime_Error: return "Runtime_Error";
        case SubmissionStatus::Time_Limit_Exceed: return "Time_Limit_Exceed";
    }
    return "Unknown";
}

struct Submission {
    string problem;
    SubmissionStatus status;
    int time;
    
    Submission(const string& p, SubmissionStatus s, int t) : problem(p), status(s), time(t) {}
};

struct ProblemStatus {
    int wrong_attempts = 0;
    int solve_time = -1;  // -1 means not solved
    bool is_frozen = false;
    int frozen_submissions = 0;
    int wrong_before_freeze = 0;
    
    ProblemStatus() = default;
};

struct Team {
    string name;
    map<char, ProblemStatus> problems;
    vector<Submission> submissions;
    
    Team(const string& n) : name(n) {}
    
    int getSolvedCount() const {
        int count = 0;
        for (const auto& [ch, status] : problems) {
            if (status.solve_time != -1 && !status.is_frozen) {
                count++;
            }
        }
        return count;
    }
    
    int getPenaltyTime() const {
        int penalty = 0;
        for (const auto& [ch, status] : problems) {
            if (status.solve_time != -1 && !status.is_frozen) {
                penalty += 20 * status.wrong_attempts + status.solve_time;
            }
        }
        return penalty;
    }
    
    vector<int> getSolveTimes() const {
        vector<int> times;
        for (const auto& [ch, status] : problems) {
            if (status.solve_time != -1 && !status.is_frozen) {
                times.push_back(status.solve_time);
            }
        }
        sort(times.rbegin(), times.rend());
        return times;
    }
};

struct RankingComparator {
    bool operator()(const pair<string, Team*>& a, const pair<string, Team*>& b) const {
        Team* teamA = a.second;
        Team* teamB = b.second;
        
        int solvedA = teamA->getSolvedCount();
        int solvedB = teamB->getSolvedCount();
        
        if (solvedA != solvedB) return solvedA > solvedB;
        
        int penaltyA = teamA->getPenaltyTime();
        int penaltyB = teamB->getPenaltyTime();
        
        if (penaltyA != penaltyB) return penaltyA < penaltyB;
        
        vector<int> timesA = teamA->getSolveTimes();
        vector<int> timesB = teamB->getSolveTimes();
        
        size_t minSize = min(timesA.size(), timesB.size());
        for (size_t i = 0; i < minSize; i++) {
            if (timesA[i] != timesB[i]) return timesA[i] < timesB[i];
        }
        
        if (timesA.size() != timesB.size()) return timesA.size() < timesB.size();
        
        return a.first < b.first;
    }
};

class ICPCManagementSystem {
private:
    map<string, Team> teams;
    bool competition_started = false;
    bool competition_ended = false;
    int duration_time = 0;
    int problem_count = 0;
    bool is_frozen = false;
    int freeze_time = -1;
    int current_time = 0;
    vector<pair<string, Team*>> current_ranking;
    bool scoreboard_flushed = false;
    
public:
    void addTeam(const string& team_name) {
        if (competition_started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        
        if (teams.find(team_name) != teams.end()) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        
        teams.emplace(team_name, Team(team_name));
        cout << "[Info]Add successfully.\n";
    }
    
    void startCompetition(int duration, int problems) {
        if (competition_started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        
        competition_started = true;
        duration_time = duration;
        problem_count = problems;
        
        // Initialize problem status for all teams
        for (auto& [name, team] : teams) {
            team.problems.clear();
            for (char c = 'A'; c < 'A' + problems; c++) {
                team.problems[c] = ProblemStatus();
            }
        }
        
        updateRanking();
        cout << "[Info]Competition starts.\n";
    }
    
    void submit(const string& problem, const string& team_name, const string& status_str, int time) {
        if (!competition_started || competition_ended) return;
        
        current_time = max(current_time, time);
        
        auto it = teams.find(team_name);
        if (it == teams.end()) return;
        
        Team& team = it->second;
        SubmissionStatus status = parseStatus(status_str);
        char problem_char = problem[0];
        
        // Record submission
        team.submissions.emplace_back(problem, status, time);
        
        // Update problem status
        if (team.problems.find(problem_char) == team.problems.end()) {
            team.problems[problem_char] = ProblemStatus();
        }
        
        ProblemStatus& prob_status = team.problems[problem_char];
        
        if (is_frozen && prob_status.solve_time == -1) {
            // Problem is unsolved before freeze and submitted during freeze
            if (!prob_status.is_frozen) {
                prob_status.is_frozen = true;
                prob_status.wrong_before_freeze = prob_status.wrong_attempts;
            }
            prob_status.frozen_submissions++;
        } else if (!prob_status.is_frozen) {
            // Normal (non-frozen) handling
            if (status == SubmissionStatus::Accepted && prob_status.solve_time == -1) {
                prob_status.solve_time = time;
            } else if (status != SubmissionStatus::Accepted && prob_status.solve_time == -1) {
                prob_status.wrong_attempts++;
            }
        } else {
            // Already frozen problem
            prob_status.frozen_submissions++;
        }
    }
    
    void flushScoreboard() {
        if (!competition_started || competition_ended) return;
        
        updateRanking();
        scoreboard_flushed = true;
        cout << "[Info]Flush scoreboard.\n";
    }
    
    void freezeScoreboard() {
        if (!competition_started || competition_ended) return;
        
        if (is_frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        
        is_frozen = true;
        freeze_time = current_time;
        cout << "[Info]Freeze scoreboard.\n";
    }
    
    void scrollScoreboard() {
        if (!competition_started || competition_ended) return;
        
        if (!is_frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }
        
        cout << "[Info]Scroll scoreboard.\n";
        
        // First flush the scoreboard
        updateRanking();
        
        // Show scoreboard before scrolling
        displayScoreboard();
        
        // Perform scrolling and show ranking changes
        vector<string> ranking_changes;
        performScrolling(ranking_changes);
        
        // Show ranking changes
        for (const string& change : ranking_changes) {
            cout << change << "\n";
        }
        
        // Show final scoreboard
        displayScoreboard();
        
        // Unfreeze
        is_frozen = false;
        freeze_time = -1;
    }
    
    void queryRanking(const string& team_name) {
        auto it = teams.find(team_name);
        if (it == teams.end()) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }
        
        cout << "[Info]Complete query ranking.\n";
        
        if (is_frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }
        
        // Find team ranking
        int ranking = 1;
        for (const auto& [name, team] : current_ranking) {
            if (name == team_name) break;
            ranking++;
        }
        
        cout << "[" << team_name << "] NOW AT RANKING [" << ranking << "]\n";
    }
    
    void querySubmission(const string& team_name, const string& problem, const string& status) {
        auto it = teams.find(team_name);
        if (it == teams.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }
        
        cout << "[Info]Complete query submission.\n";
        
        const Team& team = it->second;
        const Submission* result = nullptr;
        
        // Find last matching submission
        for (auto rit = team.submissions.rbegin(); rit != team.submissions.rend(); ++rit) {
            bool problem_match = (problem == "ALL" || rit->problem == problem);
            bool status_match = (status == "ALL" || statusToString(rit->status) == status);
            
            if (problem_match && status_match) {
                result = &(*rit);
                break;
            }
        }
        
        if (result == nullptr) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << "[" << team_name << "] [" << result->problem << "] [" 
                 << statusToString(result->status) << "] [" << result->time << "]\n";
        }
    }
    
    void endCompetition() {
        if (!competition_started || competition_ended) return;
        
        competition_ended = true;
        cout << "[Info]Competition ends.\n";
    }
    

    
    void updateRanking() {
        current_ranking.clear();
        for (auto& [name, team] : teams) {
            current_ranking.emplace_back(name, &team);
        }
        
        sort(current_ranking.begin(), current_ranking.end(), RankingComparator());
    }
    
    void displayScoreboard() {
        for (size_t i = 0; i < current_ranking.size(); i++) {
            const auto& [name, team] = current_ranking[i];
            cout << name << " " << (i + 1) << " " 
                 << team->getSolvedCount() << " " << team->getPenaltyTime();
            
            // Display problem status
            for (char c = 'A'; c < 'A' + problem_count; c++) {
                auto it = team->problems.find(c);
                if (it != team->problems.end()) {
                    cout << " " << getProblemStatusString(it->second);
                } else {
                    cout << " .";
                }
            }
            cout << "\n";
        }
    }
    
    int getTeamRanking(Team* team) {
        int ranking = 1;
        for (const auto& [name, t] : current_ranking) {
            if (t == team) break;
            ranking++;
        }
        return ranking;
    }
    
    string getProblemStatusString(const ProblemStatus& status) {
        if (status.is_frozen) {
            if (status.wrong_before_freeze == 0) {
                return "0/" + to_string(status.frozen_submissions);
            } else {
                return to_string(status.wrong_before_freeze) + "/" + to_string(status.frozen_submissions);
            }
        } else if (status.solve_time != -1) {
            if (status.wrong_attempts == 0) {
                return "+";
            } else {
                return "+" + to_string(status.wrong_attempts);
            }
        } else {
            if (status.wrong_attempts == 0) {
                return ".";
            } else {
                return "-" + to_string(status.wrong_attempts);
            }
        }
    }
    
    void performScrolling(vector<string>& ranking_changes) {
        // Store original rankings for comparison
        vector<pair<string, Team*>> original_ranking = current_ranking;
        
        // Continue unfreezing until no frozen problems remain
        while (true) {
            // Find lowest-ranked team with frozen problems
            pair<string, Team*> target_team("", nullptr);
            char target_problem = 0;
            
            for (auto rit = current_ranking.rbegin(); rit != current_ranking.rend(); ++rit) {
                const auto& [name, team] = *rit;
                
                // Find smallest frozen problem for this team
                char smallest_frozen = 0;
                for (char c = 'A'; c < 'A' + problem_count; c++) {
                    if (team->problems.find(c) != team->problems.end() && 
                        team->problems[c].is_frozen) {
                        smallest_frozen = c;
                        break;
                    }
                }
                
                if (smallest_frozen != 0) {
                    target_team = *rit;
                    target_problem = smallest_frozen;
                    break;
                }
            }
            
            // No more frozen problems
            if (target_team.second == nullptr) break;
            
            // Unfreeze the target problem
            ProblemStatus& status = target_team.second->problems[target_problem];
            processFrozenSubmissions(*target_team.second, target_problem, status);
            status.is_frozen = false;
            
            // Recalculate rankings
            updateRanking();
            
            // Check if ranking changed
            bool ranking_changed = false;
            for (size_t i = 0; i < original_ranking.size(); i++) {
                if (i >= current_ranking.size() || original_ranking[i].first != current_ranking[i].first) {
                    ranking_changed = true;
                    break;
                }
            }
            
            if (ranking_changed) {
                // Find new position of the target team
                int new_ranking = 1;
                for (const auto& [name, team] : current_ranking) {
                    if (name == target_team.first) break;
                    new_ranking++;
                }
                
                // Find old position
                int old_ranking = 1;
                for (const auto& [name, team] : original_ranking) {
                    if (name == target_team.first) break;
                    old_ranking++;
                }
                
                // Only report if team actually moved up
                if (new_ranking < old_ranking) {
                    // Find the team that was replaced (the one at the old position of target team)
                    string replaced_team = "";
                    if (old_ranking <= current_ranking.size()) {
                        replaced_team = current_ranking[old_ranking - 1].first;
                    }
                    
                    if (!replaced_team.empty() && replaced_team != target_team.first) {
                        ranking_changes.push_back(target_team.first + " " + replaced_team + " " + 
                                                to_string(target_team.second->getSolvedCount()) + " " + 
                                                to_string(target_team.second->getPenaltyTime()));
                    }
                }
            }
            
            original_ranking = current_ranking;
        }
    }
    
    void processFrozenSubmissions(Team& team, char problem_char, ProblemStatus& status) {
        // Find submissions for this problem after freeze time
        bool found_accepted = false;
        int accepted_time = -1;
        int wrong_after_freeze = 0;
        
        for (const auto& sub : team.submissions) {
            if (sub.problem[0] == problem_char && sub.time > freeze_time) {
                if (sub.status == SubmissionStatus::Accepted && !found_accepted) {
                    found_accepted = true;
                    accepted_time = sub.time;
                } else if (sub.status != SubmissionStatus::Accepted && !found_accepted) {
                    wrong_after_freeze++;
                }
            }
        }
        
        if (found_accepted) {
            status.solve_time = accepted_time;
            status.wrong_attempts = status.wrong_before_freeze + wrong_after_freeze;
        } else {
            status.wrong_attempts = status.wrong_before_freeze + wrong_after_freeze;
        }
        
        status.frozen_submissions = 0;
        status.wrong_before_freeze = 0;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    ICPCManagementSystem system;
    string line;
    
    while (getline(cin, line)) {
        if (line.empty()) continue;
        
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (command == "ADDTEAM") {
            string team_name;
            iss >> team_name;
            system.addTeam(team_name);
        } else if (command == "START") {
            string duration_str, problem_str;
            int duration, problems;
            iss >> duration_str >> duration >> problem_str >> problems;
            system.startCompetition(duration, problems);
        } else if (command == "SUBMIT") {
            string problem, by, team_name, with, status_str, at;
            int time;
            iss >> problem >> by >> team_name >> with >> status_str >> at >> time;
            system.submit(problem, team_name, status_str, time);
        } else if (command == "FLUSH") {
            system.flushScoreboard();
        } else if (command == "FREEZE") {
            system.freezeScoreboard();
        } else if (command == "SCROLL") {
            system.scrollScoreboard();
        } else if (command == "QUERY_RANKING") {
            string team_name;
            iss >> team_name;
            system.queryRanking(team_name);
        } else if (command == "QUERY_SUBMISSION") {
            string team_name, where, problem_str, and_str, status_str;
            iss >> team_name >> where >> problem_str >> and_str >> status_str;
            
            // Parse problem and status
            string problem = problem_str.substr(8); // Remove "PROBLEM="
            string status = status_str.substr(7);   // Remove "STATUS="
            
            system.querySubmission(team_name, problem, status);
        } else if (command == "END") {
            system.endCompetition();
            break;
        }
    }
    
    return 0;
}