#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <algorithm>
using namespace std;

// ===================== 工具函数 =====================
string readFile(const string& filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Ошибка открытия файла: " << filename << endl;
        return "";
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

void writeFile(const string& filename, const string& data) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Ошибка записи файла: " << filename << endl;
        return;
    }
    file.write(data.c_str(), data.size());
}

int findOptimalBlockSize(const string& text) {
    int n = text.size();
    if (n < 1024) return 1;
    if (n < 4096) return 2;
    if (n < 16384) return 4;
    return 8;
}

// ===================== Huffman =====================
struct Node {
    char ch;
    int freq;
    Node* left;
    Node* right;
    Node(char c, int f) : ch(c), freq(f), left(nullptr), right(nullptr) {}
};
struct Compare {
    bool operator()(Node* a, Node* b) { return a->freq > b->freq; }
};

void buildHuffmanCodes(Node* root, const string& str, unordered_map<char, string>& codes) {
    if (!root) return;
    if (!root->left && !root->right)
        codes[root->ch] = str;
    buildHuffmanCodes(root->left, str + "0", codes);
    buildHuffmanCodes(root->right, str + "1", codes);
}

void deleteTree(Node* root) {
    if (!root) return;
    deleteTree(root->left);
    deleteTree(root->right);
    delete root;
}

void compressHuffman(const string& inputFile, const string& outputFile) {
    string text = readFile(inputFile);
    if (text.empty()) {
        cerr << "Ошибка: пустой файл!" << endl;
        return;
    }

    unordered_map<char, int> freq;
    for (char c : text) freq[c]++;

    priority_queue<Node*, vector<Node*>, Compare> pq;
    for (auto& p : freq)
        pq.push(new Node(p.first, p.second));

    while (pq.size() > 1) {
        Node* left = pq.top(); pq.pop();
        Node* right = pq.top(); pq.pop();
        Node* node = new Node('\0', left->freq + right->freq);
        node->left = left;
        node->right = right;
        pq.push(node);
    }

    Node* root = pq.top();
    unordered_map<char, string> codes;
    buildHuffmanCodes(root, "", codes);

    string encoded;
    for (char c : text) encoded += codes[c];

    string byteData;
    for (size_t i = 0; i < encoded.size(); i += 8) {
        string byteStr = encoded.substr(i, 8);
        while (byteStr.size() < 8) byteStr += '0';
        bitset<8> bits(byteStr);
        byteData.push_back(static_cast<char>(bits.to_ulong()));
    }

    ofstream out(outputFile, ios::binary);
    string header = "HUFF";
    out.write(header.c_str(), 4);

    size_t dictSize = codes.size();
    out.write(reinterpret_cast<char*>(&dictSize), sizeof(size_t));
    for (auto& p : codes) {
        out.put(p.first);
        size_t len = p.second.size();
        out.write(reinterpret_cast<char*>(&len), sizeof(size_t));
        out.write(p.second.c_str(), len);
    }
    out.write(byteData.c_str(), byteData.size());
    out.close();

    deleteTree(root);
    cout << "Файл успешно заархивирован (Хаффман): " << outputFile << endl;

    ifstream orig(inputFile, ios::binary | ios::ate);
    ifstream comp(outputFile, ios::binary | ios::ate);
    double origSize = orig.tellg();
    double compSize = comp.tellg();
    orig.close(); comp.close();
    cout << "Размер исходного файла: " << origSize << " байт\n";
    cout << "Размер сжатого файла: " << compSize << " байт\n";
    cout << "Коэффициент сжатия: " << (compSize / origSize * 100) << "%\n";
}

void decompressHuffman(const string& inputFile, const string& outputFile) {
    ifstream in(inputFile, ios::binary);
    if (!in.is_open()) {
        cerr << "Ошибка открытия файла!" << endl;
        return;
    }

    char header[5] = {};
    in.read(header, 4);
    if (string(header) != "HUFF") {
        cerr << "Неверный формат файла!" << endl;
        return;
    }

    size_t dictSize;
    in.read(reinterpret_cast<char*>(&dictSize), sizeof(size_t));
    unordered_map<string, char> reverseCodes;
    for (size_t i = 0; i < dictSize; ++i) {
        char c = in.get();
        size_t len;
        in.read(reinterpret_cast<char*>(&len), sizeof(size_t));
        string code(len, '\0');
        in.read(&code[0], len);
        reverseCodes[code] = c;
    }

    string encodedBytes((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    in.close();

    string bitStream;
    for (unsigned char c : encodedBytes) {
        bitset<8> bits(c);
        bitStream += bits.to_string();
    }

    string decoded, current;
    for (char bit : bitStream) {
        current += bit;
        if (reverseCodes.count(current)) {
            decoded += reverseCodes[current];
            current.clear();
        }
    }

    writeFile(outputFile, decoded);
    cout << "Файл успешно разархивирован (Хаффман): " << outputFile << endl;
}

// ===================== Shannon–Fano =====================
struct SFNode {
    char ch;
    int freq;
    string code;
};

bool compareSF(const SFNode& a, const SFNode& b) {
    return a.freq > b.freq;
}

void buildShannonFano(vector<SFNode>& symbols, int left, int right) {
    if (left >= right) return;
    int total = 0;
    for (int i = left; i <= right; i++) total += symbols[i].freq;
    int half = total / 2;
    int sum = 0, split = left;
    for (int i = left; i <= right; i++) {
        sum += symbols[i].freq;
        if (sum >= half) { split = i; break; }
    }
    for (int i = left; i <= split; i++) symbols[i].code += "0";
    for (int i = split + 1; i <= right; i++) symbols[i].code += "1";
    buildShannonFano(symbols, left, split);
    buildShannonFano(symbols, split + 1, right);
}

void compressShannonFano(const string& inputFile, const string& outputFile) {
    string text = readFile(inputFile);
    if (text.empty()) {
        cerr << "Ошибка: пустой файл!" << endl;
        return;
    }

    unordered_map<char, int> freq;
    for (char c : text) freq[c]++;

    vector<SFNode> symbols;
    for (auto& p : freq) symbols.push_back({ p.first, p.second, "" });
    sort(symbols.begin(), symbols.end(), compareSF);
    buildShannonFano(symbols, 0, symbols.size() - 1);

    unordered_map<char, string> codes;
    for (auto& s : symbols) codes[s.ch] = s.code;

    string encoded;
    for (char c : text) encoded += codes[c];

    string byteData;
    for (size_t i = 0; i < encoded.size(); i += 8) {
        string byteStr = encoded.substr(i, 8);
        while (byteStr.size() < 8) byteStr += '0';
        bitset<8> bits(byteStr);
        byteData.push_back(static_cast<char>(bits.to_ulong()));
    }

    ofstream out(outputFile, ios::binary);
    string header = "SFAN";
    out.write(header.c_str(), 4);

    size_t dictSize = codes.size();
    out.write(reinterpret_cast<char*>(&dictSize), sizeof(size_t));
    for (auto& p : codes) {
        out.put(p.first);
        size_t len = p.second.size();
        out.write(reinterpret_cast<char*>(&len), sizeof(size_t));
        out.write(p.second.c_str(), len);
    }
    out.write(byteData.c_str(), byteData.size());
    out.close();

    cout << "Файл успешно заархивирован (Шеннон-Фано): " << outputFile << endl;

    ifstream orig(inputFile, ios::binary | ios::ate);
    ifstream comp(outputFile, ios::binary | ios::ate);
    double origSize = orig.tellg();
    double compSize = comp.tellg();
    orig.close(); comp.close();
    cout << "Размер исходного файла: " << origSize << " байт\n";
    cout << "Размер сжатого файла: " << compSize << " байт\n";
    cout << "Коэффициент сжатия: " << (compSize / origSize * 100) << "%\n";
}

void decompressShannonFano(const string& inputFile, const string& outputFile) {
    ifstream in(inputFile, ios::binary);
    if (!in.is_open()) {
        cerr << "Ошибка открытия файла!" << endl;
        return;
    }

    char header[5] = {};
    in.read(header, 4);
    if (string(header) != "SFAN") {
        cerr << "Неверный формат файла!" << endl;
        return;
    }

    size_t dictSize;
    in.read(reinterpret_cast<char*>(&dictSize), sizeof(size_t));
    unordered_map<string, char> reverseCodes;
    for (size_t i = 0; i < dictSize; ++i) {
        char c = in.get();
        size_t len;
        in.read(reinterpret_cast<char*>(&len), sizeof(size_t));
        string code(len, '\0');
        in.read(&code[0], len);
        reverseCodes[code] = c;
    }

    string encodedBytes((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    in.close();

    string bitStream;
    for (unsigned char c : encodedBytes) {
        bitset<8> bits(c);
        bitStream += bits.to_string();
    }

    string decoded, current;
    for (char bit : bitStream) {
        current += bit;
        if (reverseCodes.count(current)) {
            decoded += reverseCodes[current];
            current.clear();
        }
    }

    writeFile(outputFile, decoded);
    cout << "Файл успешно разархивирован (Шеннон-Фано): " << outputFile << endl;
}

// ===================== 主函数 =====================
int main() {
    cout << "Выберите действие:\n";
    cout << "1 - Заархивировать файл\n";
    cout << "2 - Разархивировать файл\n";
    int choice;
    cin >> choice;

    string inputFile, outputFile;
    cout << "Введите имя входного файла: ";
    cin >> inputFile;
    cout << "Введите имя выходного файла: ";
    cin >> outputFile;

    if (choice == 1) {
        cout << "Выберите алгоритм:\n1 - Хаффман\n2 - Шеннон-Фано\n";
        int alg;
        cin >> alg;

        int blockSize = findOptimalBlockSize(readFile(inputFile));
        cout << "Оптимальная длина блока: " << blockSize << endl;

        if (alg == 1)
            compressHuffman(inputFile, outputFile);
        else
            compressShannonFano(inputFile, outputFile);
    }
    else if (choice == 2) {
        cout << "Введите тип алгоритма (1 - Хаффман, 2 - Шеннон-Фано): ";
        int alg;
        cin >> alg;
        if (alg == 1)
            decompressHuffman(inputFile, outputFile);
        else
            decompressShannonFano(inputFile, outputFile);
    }
    else {
        cout << "Неверный выбор!" << endl;
    }

    return 0;
}
