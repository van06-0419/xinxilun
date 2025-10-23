#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <iomanip>

#pragma pack(push, 1)
struct Wenjiantou {
	uint16_t bfType; // wenjianleixing
	uint32_t bfSize; // wenjiandaxiao
	uint16_t bfReserved1; // baoliu
	uint16_t bfReserved2; // baoliu
	uint32_t bfOffBits; // xiangsushujupianyi = wenjaintou + xinxitou + tiaosebandaxiao
};

struct Xinxitou {
	uint32_t biSize; // xinxitoudaxiao
	int32_t biWidth; // tuxiangkuandu
	int32_t biHeight; // tuxianggaodu
	uint16_t biPlanes; // secaipingmian
	uint16_t biBitCount; // meigexiangsudeweishu
	uint32_t biCompression; // yasuoleixing
	uint32_t biSizeImage; // tuxiangshujudaxiao
	int32_t biXPelsPerMeter; // shuipingfenbianlv
	int32_t biYPelsPerMeter; // chuizhifenbianlv
	uint32_t biClrUsed; // tiaosebanyanseshu
	uint32_t biClrImportant; //zhongyaoyanseshu
};
#pragma pack(pop)
// wenjiantou 14 byte; xinxitou 40 byte; weitushuju

uint8_t getPixel(const std::vector<uint8_t>& shuju, int kuandu, int x, int y, int gaodu) {
	int meihangzijie = (kuandu + 1) / 2;
	int hangIndex = gaodu - 1 - y;
	int zijieIndex = hangIndex * meihangzijie + x / 2;
	uint8_t zijie = shuju[zijieIndex];

	uint8_t result;
	if (x % 2 == 0) result = zijie >> 4;
	else result = zijie & 0x0F; // Пиксели в байте хранятся слева направо: левый (чётный) пиксель находится в старших четырёх битах, а правый (нечётный) — в младших четырёх битах.

	return result;
}

std::vector<uint8_t> bianmaRLE4(const std::vector<uint8_t>& shuju, int kuandu, int gaodu) {
	std::vector<uint8_t> bianmahou;
	for (int y = gaodu - 1; y >= 0; y--) {
		int x = 0;
		while (x < kuandu) {
			uint8_t color = getPixel(shuju, kuandu, x, y, gaodu);
			int count = 1;
			while (x + count < kuandu && getPixel(shuju, kuandu, x + count, y, gaodu) == color && count < 255)
				count++;
			bianmahou.push_back(count);
			bianmahou.push_back((color << 4) | color);
			x += count;
		}
		bianmahou.push_back(0);
		bianmahou.push_back(0);
	}
	bianmahou.push_back(0);
	bianmahou.push_back(1);
	return bianmahou;
}

std::uintmax_t huoquwenjiandaxiao(const std::string& wenjianming) {
	std::ifstream wenjian(wenjianming.c_str(), std::ios::binary | std::ios::ate);
	if (!wenjian) return 0;
	return static_cast<std::uintmax_t>(wenjian.tellg());
}

int main() {
	std::string inputFile, outputFile;
	std::cout << "Введите имя выходного BMP файла: ";
	std::cin >> inputFile;

	std::ifstream fin(inputFile.c_str(), std::ios::binary); // Открыть файл в двоичном режиме
	if (!fin) {
		std::cerr << "Не удалось открыть входной файл.\n";
		return 1;
	}

	Wenjiantou fileHeader;
	fin.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
	if (fileHeader.bfType != 0x4D42) {
		std::cerr << "Это не корректный файл.\n";
		return 1;
	}

	Xinxitou infoHeader;
	fin.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
	if (infoHeader.biBitCount != 4) {
		std::cerr << "Поддерживаются только 4-битные (16 цветов) BMP изображения.\n";
		return 1;
	}

	int tiaosebandaxiao = 16 * 4;
	std::vector<uint8_t> tiaoseban(tiaosebandaxiao);
	fin.read(reinterpret_cast<char*>(&tiaoseban[0]), tiaosebandaxiao);

	// shujudaxiao = wenjainzongdaixao - wenjaintouhetiaosebandaixao
	int shujudaxiao = fileHeader.bfSize - fileHeader.bfOffBits;
	std::vector<uint8_t> tupianshuju(shujudaxiao);
	fin.read(reinterpret_cast<char*>(&tupianshuju[0]), shujudaxiao);
	fin.close();

	std::vector<uint8_t> bianmaResult = bianmaRLE4(tupianshuju, infoHeader.biWidth, infoHeader.biHeight);

	std::cout << "Введите имя выходного BMP файл: ";
	std::cin >> outputFile;

	std::ofstream fout(outputFile.c_str(), std::ios::binary);
	if (!fout) {
		std::cerr << "Не удалось создать выходной файл.\n";
		return 1;
	}
	// Создать бинарный выходной файл и проверить, успешно ли он открыт.


	// Обновление данных BMP-файла
	infoHeader.biCompression = 2;
	infoHeader.biSizeImage = bianmaResult.size();
	fileHeader.bfSize = fileHeader.bfOffBits + bianmaResult.size();
	fout.write(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
	fout.write(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
	fout.write(reinterpret_cast<char*>(&tiaoseban[0]), tiaosebandaxiao);
	fout.write(reinterpret_cast<char*>(&bianmaResult[0]), bianmaResult.size());
	fout.close();

	std::cout << "Сжатие завершено! Создан файл: " << outputFile << "\n\n";

	std::uintmax_t yuanlaidesize = huoquwenjiandaxiao(inputFile);
	std::uintmax_t yasuohoudesize = huoquwenjiandaxiao(outputFile);

	if (yuanlaidesize == 0 || yasuohoudesize == 0) {
		std::cerr << "Error\n";
	}
	else {
		double yasuobi = static_cast<double>(yuanlaidesize) / static_cast<double>(yasuohoudesize);
		std::cout << "Имя файла: " << inputFile << std::endl;
		std::cout << "Исходный размер: " << yuanlaidesize / 1024.0 << "КБ" << std::endl;
		std::cout << "Размер после сжатия: " << yasuohoudesize / 1024.0 << "КБ" << std::endl;
		std::cout << "Коэффициент сжатия: " << std::fixed << std::setprecision(1) << yasuobi << " : 1" << std::endl;
	}

	return 0;
}
