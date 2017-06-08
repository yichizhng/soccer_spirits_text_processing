#include <iostream>
#include <cstdlib>
#include <fstream>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <unordered_map>

using std::string;

static inline
// Copied from wikibooks
int levenshtein_distance(const std::string &s1, const std::string &s2)
{
        // To change the type this function manipulates and returns, change
        // the return type and the types of the two variables below.
        int s1len = s1.size();
        int s2len = s2.size();

        auto column_start = (decltype(s1len))1;

        auto column = new decltype(s1len)[s1len + 1];
        std::iota(column + column_start, column + s1len + 1, column_start);

        for (auto x = column_start; x <= s2len; x++) {
                column[0] = x;
                auto last_diagonal = x - column_start;
                for (auto y = column_start; y <= s1len; y++) {
                        auto old_diagonal = column[y];
                        auto possibilities = {
                                column[y] + 1,
                                column[y - 1] + 1,
                                last_diagonal + (s1[y - 1] == s2[x - 1]? 0 : 1)
                        };
                        column[y] = std::min(possibilities);
                        last_diagonal = old_diagonal;
                }
        }
        auto result = column[s1len];
        delete[] column;
        return result;
}

static inline void replace_all_instances(std::string& str, const std::string& old, const std::string& blah) {
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

static std::unordered_map<std::string, std::string> wikia_question_mapping;

// Replace [option1/option2] with option1
inline void wikia_gender_male(std::string& question) {
  using namespace std;
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
  using namespace std;
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
  using namespace std;
  ifstream infile("wikia_questions.txt");
  string line;
  while (getline(infile, line)) {
    size_t tpos = line.find('\t');
    wikia_question_mapping[line.substr(0,tpos)] = line.substr(tpos+1);

  }
}

// question should be given in player unspecified style (i.e. {0})
std::string wikia_question_map(const std::string& gender,
                               const std::string& question) {
  using namespace std;
  string q_copy = question;
  replace_all_instances(q_copy, "{0}", "[Player]");
  // Search the map for the question; there's not really an efficient way to do
  // this, sadly
  for (const auto& it : wikia_question_mapping) {
    string m_copy = it.second;
    if (gender == "Male") {
      wikia_gender_male(m_copy);
    } else {
      wikia_gender_female(m_copy);
    }
    if (m_copy == q_copy) {
      // yay
      return it.first;
    }
  }
  // cerr << "Wikia: question not found: " << question << endl;
  return "";
}

std::string reverse_wikia_map_male(int question_id, const std::string& name) {
  string m_copy = wikia_question_mapping.at(std::to_string(question_id));
  wikia_gender_male(m_copy);
  replace_all_instances(m_copy, "[Player]", name);
  return m_copy;
}

std::string reverse_wikia_map_female(int question_id, const std::string& name) {
  string m_copy = wikia_question_mapping.at(std::to_string(question_id));
  wikia_gender_female(m_copy);
  replace_all_instances(m_copy, "[Player]", name);
  return m_copy;
}

// Questions are divided into 13 groups (1-20, 21-40, etc). Each group has
// 3 distinct "personalities" which determine the results of answers to
// those questions; personalities 1, 2, and 3, for example, correspond to
// questions 1-20.

// Map storing the personality values of each player.
static std::map<std::string, std::vector<int>> player_personalities;

static void init_personality_map() {
  using namespace std;
  ifstream infile("personalities.tsv");
  string line;
  while (getline(infile, line)) {
    size_t tpos = line.find('\t');
    string player = line.substr(0, tpos);
    // Parse out the rest of the personalities
    do {
      // Check if this section is empty
      size_t n_tpos = line.find('\t', tpos + 1);
      string section = line.substr(tpos+1, n_tpos == string::npos ? string::npos : (n_tpos) - (tpos+1));
      if (!section.empty()) {
        player_personalities[player].push_back(stoi(section));
      }
      tpos = n_tpos;
    } while (tpos != string::npos);
  }
}

// Map storing all this nonsense. First key is the personality, second
// is the question number (yes, this is somewhat redundant). Third key is the
// answer in question. Fourth key is the teamwork value of the answer. Value
// is the number of votes for that key (used to detect/resolve conflicts)
static std::map<int, std::map<int, std::array<std::map<int, int>, 3>>>
teamwork_personality_map;

// Utility function to insert an answer into the map.
static inline void insert_answer(const std::string& player_name,
                                 int question_id,
                                 int question_answer,
                                 int question_value) {
  if (!player_personalities.count(player_name)) return;
  // Look for relevant personality for id
  int group = 3 * ((question_id - 1) / 20);
  for (const int& p : player_personalities.at(player_name)) {
    if (p == group + 1 || p == group + 2 || p == group + 3) {
      // Found it
      teamwork_personality_map[p][question_id][question_answer-1][question_value]++;
      return;
    }
  }
  std::cerr << "impossible question " << group << ' ' << question_id << std::endl;
}

string remove_blood_code(const string& player) {
  string copy = player;
  if (isupper(copy[1]) || isdigit(copy[1])) {
    size_t space = copy.find(' ');
    if (space != string::npos) {
      copy = copy.substr(space+1);
    }
  }
  if (copy.substr(0,8) == "Metatron") copy = "Metatron";
  return copy;
}

int main() {
  using namespace std;

  // init_wikia_question_mapping();
  init_personality_map();

  map<string, string> questions_male;
  map<string, string> questions_female;

  map<string, map<int, string>> wikia_qas;
  map<string, map<string, string>> player_qas;

  map<string, map<int, string>> player_answers_by_id;  // squishy format

  map<string, string> player_gender;
  {
    ifstream infile("genders.txt");
    string player, gender;
    while (getline(infile, player)) {
      // TODO: implement properly
      // remove blood code kappa
      getline(infile, gender);
      player_gender[player] = gender;
    }
  }

  unordered_set<string> u_questions;
  vector<string> male_questions;
  vector<string> female_questions;
  male_questions.push_back("");
  female_questions.push_back("");
  {
    ifstream infile("questions.txt");
    string line;
    while (getline(infile, line)) {
      size_t tab = line.find('\t');
      questions_male[line.substr(0,tab)] = line.substr(tab+1);
	  male_questions.push_back(line.substr(0, tab));
      u_questions.insert(line.substr(0,tab));
      getline(infile, line);
      tab = line.find('\t');
      questions_female[line.substr(0,tab)] = line.substr(tab+1);
	  female_questions.push_back(line.substr(0, tab));
      u_questions.insert(line.substr(0,tab));
    }
  }

  for (const auto it : player_personalities) {
    string player = it.first;
    string mangled_player = remove_blood_code(player);
    const string& gender = player_gender[player];
    for (const int& personality : it.second) {
      const int starting_point = 20 * ((personality-1)/3);
      for (int j = 1; j <= 20; ++j) {
        int question_idx = starting_point + j;
        int answer_idx = ((personality + j + 1) % 3);
        //cout << starting_point + j << ": " << 1 + ((i + j + 2) % 3) << endl;
        if (gender == "Male") {
          string original_question = male_questions[question_idx];
          string answers = questions_male.at(original_question);
          replace_all_instances(answers, "{0}", mangled_player);
          replace_all_instances(original_question, "{0}", mangled_player);
          cout << '[' << player << ']' << original_question << endl;
          for (int ii = 0; ii < answer_idx; ++ii) {
            answers = answers.substr(answers.find('\t')+1);
          }
          cout << '\t' << answers.substr(0, answers.find('\t')) << endl;
        } else {
          string original_question = female_questions[question_idx];
          string answers = questions_female.at(original_question);
          replace_all_instances(answers, "{0}", mangled_player);
          replace_all_instances(original_question, "{0}", mangled_player);
          cout << '[' << player << ']' << original_question << endl;
          for (int ii = 0; ii < answer_idx; ++ii) {
            answers = answers.substr(answers.find('\t')+1);
          }
          cout << '\t' << answers.substr(0, answers.find('\t')) << endl;
        }
      }
      cout << endl;
    }
  }
  return 0;

  ifstream infile("all_teamwork.tsv");
  string line;
  while (getline(infile, line)) {
        size_t tab = line.find_first_not_of(" \t");
        if (tab == string::npos) {
          // line is empty
          continue;
        } else {
          tab = line.find('\t');
          // This is the character's name, hopefully?
          string name = line.substr(0, tab);
          string original_name = name;
          // cerr << name << endl;
          // cout << line.substr(0, tab);

          // Deal with name inconsistencies in the spreadsheet
          if (name == "Jin") name = "King Jin";
          // Remove blood codes

          if (isupper(name[1]) || isdigit(name[1])) {
            size_t space = name.find(' ');
            if (space != string::npos) {
              name = name.substr(space+1);
            }
          }
          string this_gender = player_gender[name];

          getline(infile, line);
          // the question
          string question = line.substr(0, line.find('\t'));
          // validate question

          int closest_distance = 9999999;
          string closest_question;

          // For performance first look for an exact match in a dumb way
          // Some player names exist in the questions, so this will not always work
          // But close enough :)
          string gender = "neuter";
          string q_copy = question;
          string question_idx;
          replace_all_instances(q_copy, name, "{0}");
          if (u_questions.count(q_copy)) {
            closest_distance = 0;
            question_idx = q_copy;  // mkay
            if (questions_male.count(q_copy)) {
              if (questions_female.count(q_copy)) {
              } else {
                gender = "Male";
              }
            } else {
              if (questions_female.count(q_copy)) {
                gender = "Female";
              } else {
                cerr << "Unrecognized player: " << name << endl << endl;
                continue;
              }
            }
          }

          if (closest_distance) {
            if (this_gender == "Male") {
            for (auto it : questions_male) {
              string copy = it.first;
              replace_all_instances(copy, "{0}", name);
              int distance = levenshtein_distance(copy, question);
              if (distance < closest_distance) {
                closest_distance = distance;
                closest_question = copy;
              }
              if (distance == 0) {
                question_idx = it.first;
                break;
              }
            }
            } else {

            for (auto it : questions_female) {
              string copy = it.first;
              replace_all_instances(copy, "{0}", name);
              int distance = levenshtein_distance(copy, question);
              if (distance < closest_distance) {
                closest_distance = distance;
                closest_question = copy;
              }
              if (distance == 0) {
                question_idx = it.first;
                break;
              }
            }
            }
          }

          // Question verification
          string answers;
          if (closest_distance) {
            cerr << '\t' << "Question not found for " << name << ":" << endl;
            cerr << line.substr(0, line.find('\t')) << endl;
            cerr << '\t' << "Suggested replacement:" << endl;
            cerr << closest_question << endl << endl;
          } else {
            if (this_gender == "Female") {
              if (gender != "Male") {
                answers = questions_female[question_idx];
              } else {
                cerr << "Gender mismatch for " << name << " ("
                     << this_gender << ")" << endl;
                cerr << question << endl << endl;
                answers = questions_male[question_idx];
              }
            } else {
              if (gender != "Female") {
                answers = questions_male[question_idx];
              } else {
                cerr << "Gender mismatch for " << name << " ("
                     << this_gender << ")" << endl;
                cerr << question << endl << endl;
                answers = questions_female[question_idx];
              }
            }
          }
          string wikia_number = wikia_question_map(this_gender,
                                                   question_idx);
          if (!wikia_number.empty()) {
            // cout << name << "\t{{TeamworkQ" << wikia_number << "|";
          } else {
            cerr << "Wikia lookup failed" << endl
                 << name << "\t" << this_gender << "\t" << question_idx << endl;
          }
          string wikia_format = "{{TeamworkQ" + wikia_number + "|";
          string bleh;
          // string ans;
          // Answer 1
          // string tsv_answers;
          getline(infile, line);
          tab = line.find('\t');
          // ans += line.substr(tab);
          // if (!wikia_number.empty()) cout << line.substr(tab+1) << "|";
          string part = line.substr(tab+1);
          if (part != "1" && part != "3" && part != "+1"
              && part != "-1" && part != "+3" && part != "") {
            cerr << "Error in answer scores for " << question << endl;
            cerr << part << endl << endl;
            part = "";
          }
          wikia_format += (part.length() == 1 ? "+" + part : part) + "|";
          bleh += part + "\t";
          if (!wikia_number.empty() && !part.empty()) {
            insert_answer(name, stoi(wikia_number), 2, stoi(part));
          }

          // Answer 2
          getline(infile, line);
          tab = line.find('\t');
          // ans += line.substr(tab);
          // if (!wikia_number.empty()) cout << line.substr(tab+1) << "|";
          part = line.substr(tab+1);
          if (part != "1" && part != "3" && part != "+1"
              && part != "-1" && part != "+3" && part != "") {
            cerr << "Error in answer scores for " << question << endl;
            cerr << part << endl << endl;
            part = "";
          }
          wikia_format += (part.length() == 1 ? "+" + part : part) + "|";
          bleh += part + "\t";
          if (!wikia_number.empty() && !part.empty()) {
            insert_answer(name, stoi(wikia_number), 3, stoi(part));
          }

          // Answer 3
          getline(infile, line);
          tab = line.find('\t');
          // ans += line.substr(tab);
          // if (!wikia_number.empty()) cout << line.substr(tab+1) << "}}" << endl;
          part = line.substr(tab+1);
          if (part != "1" && part != "3" && part != "+1"
              && part != "-1" && part != "+3" && part != "") {
            cerr << "Error in answer scores for " << question << endl;
            cerr << part << endl << endl;
            part = "";
          }
          wikia_format += (part.length() == 1 ? "+" + part : part) + "}}";
          bleh += part + "\t";

          if (!wikia_number.empty()) {
            if (wikia_qas[name].count(stoi(wikia_number))) {
              cerr << "Repeated question for " << name << ": " << question << endl;
            }
            wikia_qas[name][stoi(wikia_number)] = wikia_format;
            player_answers_by_id[name][stoi(wikia_number)] = bleh;
          }
          if (!wikia_number.empty() && !part.empty()) {
            insert_answer(name, stoi(wikia_number), 1, stoi(part));
          }

          // player_qas[name][question] = ans;
          /*
          size_t answers_pos;
          while ((answers_pos = answers.find("{0}")) != string::npos) {
            answers.erase(answers_pos, 3);
            answers.insert(answers_pos, name);
          }
          while ((answers_pos = answers.find("\t")) != string::npos) {
            answers.erase(answers_pos, 1);
            answers.insert(answers_pos, "\n");
          }
          */
          // if (!answers.empty() && answers != tsv_answers) {
            // cerr << "Answers do not match for player " << name << ":" << endl;
            // cerr << question << endl;
            // cerr << answers << endl << endl;
            // cerr << tsv_answers << endl << endl;
          // }
        }
  }

  // output modules
  /*
  for (const auto it : player_qas) {
    for (const auto itt : it.second) {
      cout << it.first << "\t" << itt.first << itt.second << endl;
    }
    } */

  /*
// Wikia templates
  for (const auto it : wikia_qas) {
    cout << endl << it.first << endl;
    for (const auto itt : it.second) {
      cout << itt.second << endl;
    }
  }
  cout << endl << endl;
  */

  // Personality groups
  for (const auto it : teamwork_personality_map) {
    cout << "Personality " << it.first;
    /*
    for (const auto itt : it.second) {
      cout << "Question " << itt.first << endl;
      for (int i = 0; i < 3; ++i) {
        if (itt.second[i].size() > 1) {
          cout << '\t' << "CONFLICT" << endl;
        }
        for (const auto ittt : itt.second[i]) {
          cout << "Answer " << i + 1 << ": " << ittt.first << " (" << ittt.second << " votes)" << endl;
        }
      }
      } */
    for (int i = 1; i <= 20; ++i) {
      int question = 20 * ((it.first-1)/3) + i;
      // cout << "Question " << question << endl;
      for (int j = 0; j < 3; ++j) {
        // cout << "Answer " << i + 1 << ": ";
        cout << '\t';
        if (it.second.count(question)) {
          if (it.second.at(question)[j].size() > 1) {
            for (const auto itt : it.second.at(question)[j]) {
              cout << itt.first << " (" << itt.second << " votes)";
            }
          } else if (it.second.at(question)[j].size() == 1) {
            for (const auto itt : it.second.at(question)[j]) {
              cout << itt.first;
            }
          }
        }
      }
    }
    cout << endl;
  }
  //  cout << "Hello, world!" << endl;
  return 0;
}
