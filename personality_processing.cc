#include <iostream>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>

using std::map;
using std::set;
using std::unordered_map;
using std::size_t;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::ifstream;
using std::to_string;


namespace {

inline void replace_all_instances(string& str, const string& old,
                                  const string& blah) {
using namespace std;
        size_t pos;
        if (blah == old) {
                cerr << "infinite cycle lol" << endl;
                exit(-1);
        }
        while ((pos = str.find(old)) != std::string::npos) {
                  str.erase(pos, old.size());
                  str.insert(pos, blah);
        }
}

std::unordered_map<std::string, std::string> wikia_question_mapping;

// Replace [option1/option2] with option1
inline void wikia_gender_male(std::string& question) {
  size_t idx = question.find('[');
  while (idx != string::npos) {
    size_t eidx = question.find(']', idx+1);
    size_t sidx = question.find('/', idx+1);
    if (sidx < eidx) {
      // Erase the /option2]
      question.erase(sidx, (eidx-sidx)+1);
      // Erase the [
      question.erase(idx, 1);
    }
    idx = question.find('[', idx+1);
  }
}

// Replace [option1/option2] with option2
inline void wikia_gender_female(std::string& question) {
  size_t idx = question.find('[');
  while (idx != string::npos) {
    size_t eidx = question.find(']', idx+1);
    size_t sidx = question.find('/', idx+1);
    if (sidx < eidx) {
      // Erase the ]
      question.erase(eidx, 1);
      // Erase the [option1/
      question.erase(idx, (sidx-idx)+1);
    }
    idx = question.find('[', idx+1);
  }
}

void init_wikia_question_mapping() {
  ifstream infile("wikia_questions.txt");
  string line;
  while (getline(infile, line)) {
    size_t tpos = line.find('\t');
    wikia_question_mapping[line.substr(0,tpos)] = line.substr(tpos+1);

  }
}

int wikia_question_map(
    const string& player,
    const string& gender,
    const string& question) {
  string q_copy = question;
  // Search the map for the question; there's not really an efficient way to do
  // this, sadly
  for (const auto& it : wikia_question_mapping) {
    string m_copy = it.second;
    if (gender == "Male") {
      wikia_gender_male(m_copy);
    } else {
      wikia_gender_female(m_copy);
    }
    replace_all_instances(m_copy, "[Player]", player);
    if (m_copy == q_copy) {
      // yay
      return stoi(it.first);
    }
  }
  // cerr << "Wikia: question not found: " << question << endl;
  return -1;
}

}

int main() {
  init_wikia_question_mapping();

  map<string, string> player_gender;
  {
    ifstream infile("genders.txt");
    string player, gender;
    while (getline(infile, player)) {
      // TODO: implement properly
      // remove blood code kappa
      /*
      if (isupper(player[1]) || isdigit(player[1])) {
            size_t space = player.find(' ');
            if (space != string::npos) {
              player = player.substr(space+1);
            }
          }
      */
      getline(infile, gender);
      player_gender[player] = gender;
    }
  }

  // Read from CG's dump of which players can get which questions
  map<string, set<int>> questions_for_players;

  ifstream infile("questions_for_players.tsv");
  while (true) {
    string blah;
    if (!getline(infile, blah)) {
      break;
    }
    const string name = blah.substr(0, blah.find('\t'));
    if (name.empty()) continue;
    if (!player_gender.count(name)) {
      cerr << "Player not found: "<< name << endl;
      exit(-1);
    }
    const string& gender = player_gender.at(name);
    string line;
    while (true) {
      if (!getline(infile, line)) break;
      line = line.substr(0, line.find('\t'));
      if (line.empty()) break;

      int wikia_question = wikia_question_map(name, gender, line);
      if (wikia_question == -1) {
        cerr << "Question not found: " << line << endl;
        exit(-1);
      }
      if (blah.substr(blah.find('\t')+1).empty()) {
        questions_for_players[name].insert(wikia_question);
      } else {
        questions_for_players[blah.substr(blah.find('\t')+1)].insert(wikia_question);
      }
    }
  }

  map<string, set<string>> players_by_question_set;
  for (const auto it : questions_for_players) {
    // cout << it.first << '\t';
    string question_set;
    // It appears all question sets are two contiguous ranges
    if (it.second.size() == 40) {
      auto itt = it.second.begin();
      for (int i = 0; i < 40; ++i) {
        if (i == 0 || i == 20) {
          question_set += to_string(*itt) + "-";
        }
        if (i == 19) {
          question_set += to_string(*itt) + ", ";
        }
        if (i == 39) {
          question_set += to_string(*itt);
        }
        itt++;
      }
    } else {
      for (const auto& q : it.second) {
        question_set += to_string(q) + ",";
      }
    }
    players_by_question_set[question_set].insert(it.first);
  }

  for (const auto it : players_by_question_set) {
    cout << it.first << '\t';
    for (const auto& p : it.second) {
      cout << p << ",";
    }
    cout << endl;
  }
}
