#include <iostream>
#include <cstdlib>
#include <fstream>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <unordered_map>

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

int main() {
  using namespace std;

  init_wikia_question_mapping();

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
      if (isupper(player[1]) || isdigit(player[1])) {
            size_t space = player.find(' ');
            if (space != string::npos) {
              player = player.substr(space+1);
            }
          }

      getline(infile, gender);
      player_gender[player] = gender;
    }
  }

  unordered_set<string> u_questions;
  {
    ifstream infile("questions.txt");
    string line;
    while (getline(infile, line)) {
      size_t tab = line.find('\t');
      questions_male[line.substr(0,tab)] = line.substr(tab+1);
      u_questions.insert(line.substr(0,tab));
      getline(infile, line);
      tab = line.find('\t');
      questions_female[line.substr(0,tab)] = line.substr(tab+1);
      u_questions.insert(line.substr(0,tab));
    }
  }

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
          // cerr << name << endl;
          // cout << line.substr(0, tab);

          // Deal with name inconsistencies in the spreadsheet
          if (name == "Jin") name = "King Jin";
          // Remove blood codes
          // fucking gcc and clang not supporting ECMA mode
          /*
          static const regex blood_code("[A-Z0-9][A-Z0-9]* (.*)", regex_constants::extended);
          smatch match;
          if (std::regex_match(name, match, blood_code)) {
            name = match[1].str();
            }*/

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

  
  for (const auto it : wikia_qas) {
    cout << endl << it.first << endl;
    for (const auto itt : it.second) {
      cout << itt.second << endl;
    }
  }
  cout << endl << endl;

  //for (int i = 1; i <= 260; ++i) {
  //  cout << "\t" << i << ".1"
  //       << "\t" << i << ".2"
  //       << "\t" << i << ".3";
  //}
  
  for (int i = 0; i < 260; i += 20) {
	  //cout << i+1 << "-" << i+20;
	  for (int j = 0; j < 20; ++j) {
		  cout << "\t" << i+j+1 << ".1"
		   << "\t" << i+j+1 << ".2"
		    << "\t" << i+j+1 << ".3";
	  }
	  cout << endl;
  for (auto it : player_answers_by_id) {
    //cout << it.first << "\t";
	string bleh = it.first + "\t";
	bool any_nonzero = false;
    for (int j = 1; j <= 20; ++j) {
      //cout << it.second[i] << "\t";
      if (it.second.count(i+j)) {
        bleh += it.second[i+j];
		any_nonzero = true;
      } else {
        bleh += "\t\t\t";
      }
    }
	if (any_nonzero) cout << bleh << endl;
  }
  cout << endl << endl;
}
  //  cout << "Hello, world!" << endl;
  return 0;
}
