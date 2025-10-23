#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip>  // setprecision

#pragma pack(push, 1)
struct BITMAPFILEHEADER {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};

struct BITMAPINFOHEADER {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

uint8_t getPixel(const std::vector<uint8_t>& shuju, int kuandu, int x, int y, int gaodu) {
    int meihangZijie = (kuandu + 1) / 2;
    int hangIndex = gaodu - 1 - y;
    int zijieIndex = hangIndex * meihangZijie + x / 2;
    uint8_t zijie = shuju[zijieIndex];

    uint8_t result;
    if (x % 2 == 0) result = zijie >> 4;
    else result = zijie & 0x0F;

    return result;
}

// RLE4 
std::vector<uint8_t> encodeRLE4(const std::vector<uint8_t>& data, int width, int height) {
    std::vector<uint8_t> encoded;
    for (int y = height - 1; y > 0; y--) {
        int x = 0;
        while (x < width) {
            uint8_t color = getPixel(data, width, x, y, height);
            int count = 1;
            while (x + count < width && getPixel(data, width, x + count, y, height) == color && count < 255)
                count++;
            encoded.push_back(count);
            encoded.push_back((color << 4) | color);
            x += count;
        }
        encoded.push_back(0);
        encoded.push_back(0);
    }
    encoded.push_back(0);
    encoded.push_back(1);
    return encoded;
}

std::uintmax_t getFileSize(const std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    if (!file) return 0;
    return static_cast<std::uintmax_t>(file.tellg());
}

int main() {
    std::string inputFile, outputFile;
    std::cout << "Введите имя входного BMP файла: ";
    std::cin >> inputFile;

    std::ifstream fin(inputFile.c_str(), std::ios::binary);
    if (!fin) {
        std::cerr << "Не удалось открыть входной файл.\n";
        return 1;
    }

    BITMAPFILEHEADER fileHeader;
    fin.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (fileHeader.bfType != 0x4D42) {
        std::cerr << "Это не корректный BMP файл.\n";
        return 1;
    }

    BITMAPINFOHEADER infoHeader;
    fin.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    if (infoHeader.biBitCount != 4) {
        std::cerr << "Поддерживаются только 4-битные (16 цветов) BMP изображения.\n";
        return 1;
    }

    int paletteSize = 16 * 4;
    std::vector<uint8_t> palette(paletteSize);
    fin.read(reinterpret_cast<char*>(&palette[0]), paletteSize);

    int dataSize = fileHeader.bfSize - fileHeader.bfOffBits;
    std::vector<uint8_t> imageData(dataSize);
    fin.read(reinterpret_cast<char*>(&imageData[0]), dataSize);
    fin.close();

    std::vector<uint8_t> encoded = encodeRLE4(imageData, infoHeader.biWidth, infoHeader.biHeight);

    std::cout << "Введите имя выходного BMP файла: ";
    std::cin >> outputFile;

    std::ofstream fout(outputFile.c_str(), std::ios::binary);
    if (!fout) {
        std::cerr << "Не удалось создать выходной файл.\n";
        return 1;
    }

    infoHeader.biCompression = 2; // BI_RLE4
    infoHeader.biSizeImage = encoded.size();
    fileHeader.bfSize = fileHeader.bfOffBits + encoded.size();

    fout.write(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    fout.write(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    fout.write(reinterpret_cast<char*>(&palette[0]), paletteSize);
    fout.write(reinterpret_cast<char*>(&encoded[0]), encoded.size());
    fout.close();

    std::cout << "Сжатие завершено! Создан файл: " << outputFile << "\n\n";

    std::uintmax_t originalSize = getFileSize(inputFile);
    std::uintmax_t compressedSize = getFileSize(outputFile);

    if (originalSize == 0 || compressedSize == 0) {
        std::cerr << "Не удалось получить информацию о размере файла или файл пуст.\n";
    }
    else {
        double ratio = static_cast<double>(originalSize) / static_cast<double>(compressedSize);
        std::cout << "Имя файла: " << inputFile << std::endl;
        std::cout << "Исходный размер: " << originalSize / 1024.0 << " КБ" << std::endl;
        std::cout << "Размер после сжатия: " << compressedSize / 1024.0 << " КБ" << std::endl;
        std::cout << "Коэффициент сжатия: " << std::fixed << std::setprecision(1) << ratio << " : 1" << std::endl;
    }

    return 0;
}
