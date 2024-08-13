#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <curl/curl.h>
#include <thread>
#include <future>

const std::string url = "https://hexhunt.hackmit.org/solve_hex_hunt";

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string ping_api(const std::string& random_string) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        // replace the "userID" with your userID
        std::string payload = "{\"letters\":\"" + random_string + "\",\"userId\":\"replace_it_with_user_id_\"}";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);  // Added to free headers
    }
    return readBuffer;
}

std::unordered_set<std::string> load_valid_words(const std::string& json_path) {
    std::unordered_set<std::string> valid_words;
    std::ifstream file(json_path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        std::istringstream ss(content);
        std::string pair;
        while (std::getline(ss, pair, ',')) {
            pair.erase(std::remove(pair.begin(), pair.end(), '"'), pair.end());
            size_t colon_pos = pair.find(':');
            if (colon_pos != std::string::npos) {
                std::string word = pair.substr(0, colon_pos);
                valid_words.insert(word);
            }
        }
    } else {
        std::cerr << "Error: Could not find file at " << json_path << std::endl;
    }
    return valid_words;
}

class TrieNode {
public:
    std::unordered_map<char, TrieNode*> children;
    bool is_word;

    TrieNode() : is_word(false) {}
    ~TrieNode() {
        for (auto& child : children) {
            delete child.second;
        }
    }
};

class Trie {
public:
    TrieNode* root;

    Trie() {
        root = new TrieNode();
    }

    ~Trie() {
        delete root;
    }

    void insert(const std::string& word) {
        TrieNode* node = root;
        for (char c : word) {
            if (node->children.find(c) == node->children.end()) {
                node->children[c] = new TrieNode();
            }
            node = node->children[c];
        }
        node->is_word = true;
    }

    std::unordered_set<std::string> find_words(const std::vector<std::vector<char>>& board) {
        std::unordered_set<std::string> words;
        std::vector<std::vector<std::pair<int, int>>> directions = {
            {{0, -1}, {0, 1}, {1, 0}, {1, 1}},
            {{0, -1}, {0, 1}, {1, 0}, {1, 1}, {-1, 0}, {-1, -1}},
            {{0, -1}, {0, 1}, {1, 0}, {1, -1}, {-1, 0}, {-1, -1}},
            {{0, -1}, {0, 1}, {1, 0}, {1, -1}, {-1, 0}, {-1, 1}},
            {{0, -1}, {0, 1}, {-1, 0}, {-1, 1}}
        };

        std::function<void(TrieNode*, int, int, std::string, int)> dfs = [&](TrieNode* node, int i, int j, std::string path, int visited) {
            if (node->is_word && path.length() >= 3) {
                words.insert(path);
            }

            if (i < 0 || i >= 5 || j < 0 || j >= board[i].size() || (visited & (1 << (i * 5 + j)))) {
                return;
            }

            char c = tolower(board[i][j]);
            if (node->children.find(c) == node->children.end()) {
                return;
            }

            visited |= 1 << (i * 5 + j);
            node = node->children[c];
            path += c;

            for (const auto& dir : directions[i]) {
                dfs(node, i + dir.first, j + dir.second, path, visited);
            }

            visited &= ~(1 << (i * 5 + j));
        };

        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < board[i].size(); ++j) {
                dfs(root, i, j, "", 0);
            }
        }

        return words;
    }
};

Trie create_trie(const std::unordered_set<std::string>& words) {
    Trie trie;
    for (const auto& word : words) {
        if (word.length() >= 3) {
            trie.insert(word);
        }
    }
    return trie;
}

std::string generate_random_string(size_t length) {
    static const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string result;
    result.reserve(length);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    for (size_t i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }

    return result;
}

int score_words(const std::unordered_set<std::string>& words) {
    int score = 0;
    for (const auto& word : words) {
        int length = word.length();
        score += length;
        if (std::unordered_set<char>(word.begin(), word.end()).size() == 19) {
            score += 14;
        }
    }
    return score;
}

std::pair<int, std::unordered_set<std::string>> score_string(const std::string& str, Trie& trie) {
    if (str.length() != 19) {
        throw std::invalid_argument("Input string must be exactly 19 characters long");
    }

    std::vector<std::vector<char>> board = {
        {str[0], str[1], str[2]},
        {str[3], str[4], str[5], str[6]},
        {str[7], str[8], str[9], str[10], str[11]},
        {str[12], str[13], str[14], str[15]},
        {str[16], str[17], str[18]}
    };

    std::unordered_set<std::string> words = trie.find_words(board);
    int score = score_words(words);

    return {score, words};
}

std::pair<std::string, int> optimize_string(const std::string& initial_string, int iterations, Trie& trie) {
    std::string best_string = initial_string;
    auto [best_score, _] = score_string(best_string, trie);

    for (int iter = 0; iter < iterations; ++iter) {
        bool improved = false;
        for (size_t i = 0; i < best_string.length(); ++i) {
            for (char letter = 'A'; letter <= 'Z'; ++letter) {
                if (letter != best_string[i]) {
                    std::string new_string = best_string;
                    new_string[i] = letter;
                    auto [new_score, _] = score_string(new_string, trie);
                    if (new_score > best_score) {
                        best_string = new_string;
                        best_score = new_score;
                        improved = true;
                        std::cout << "Improved: " << best_string << " (Score: " << best_score << ")" << std::endl;
                        break;
                    }
                }
            }
            if (improved) {
                break;
            }
        }
        if (!improved) {
            break;
        }
    }

    return {best_string, best_score};
}

int main() {
    std::string script_dir = ".";
    std::string json_path = script_dir + "/sowpods.json";

    std::unordered_set<std::string> valid_words = load_valid_words(json_path);
    Trie trie = create_trie(valid_words);

    int overall_best_score = 0;
    std::string overall_best_string;
    int iteration = 0;

    std::string best_strings_log = "best_strings_log.txt";

    try {
        while (true) {
            ++iteration;
            std::string initial_string = generate_random_string(19);
            std::cout << "Iteration " << iteration << std::endl;
            std::cout << "Initial string: " << initial_string << std::endl;

            auto [best_string, best_score] = optimize_string(initial_string, 1000, trie);

            std::cout << "Best string after optimization: " << best_string << std::endl;
            std::cout << "Score for best string: " << best_score << std::endl;

            auto [_, found_words] = score_string(best_string, trie);

            std::string api_response = ping_api(best_string);
            std::cout << "API Response: " << api_response << std::endl;

            if (best_score > overall_best_score) {
                overall_best_score = best_score;
                overall_best_string = best_string;
                std::cout << "New overall best score: " << overall_best_score << std::endl;
                std::cout << "New overall best string: " << overall_best_string << std::endl;

                std::ofstream log_file(best_strings_log, std::ios_base::app);
                log_file << "Iteration: " << iteration << std::endl;
                log_file << "String: " << best_string << std::endl;
                log_file << "Local Score: " << best_score << std::endl;
                log_file << "API Score: " << api_response << std::endl;
                log_file << "Timestamp: " << std::time(nullptr) << std::endl;
                log_file << std::string(50, '=') << std::endl;
            }

            std::cout << std::string(50, '=') << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "\nScript terminated by user." << std::endl;
        std::cerr << "Final best score: " << overall_best_score << std::endl;
        std::cerr << "Final best string: " << overall_best_string << std::endl;
    }

    return 0;
}

