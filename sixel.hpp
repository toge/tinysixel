#ifndef TINYSIXEL_SIXEL_HPP
#define TINYSIXEL_SIXEL_HPP

#include <climits>

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

class SixelImage {
private:
    std::vector<std::string> escaped_;

private:
    static void print_times(std::ostream &os, int val, int cnt)
    {
        os << "#0";
        char ch = static_cast<char>(val);
        switch (cnt) {
        case 3:
            os << ch;
        case 2:
            os << ch;
        case 1:
            os << ch;
            break;

        // 4回以上の場合はRLE圧縮ができる
        default:
            os << "!" << cnt;
            os << ch;
            break;
        }
    }

    static std::vector<std::string> escape(int width, int height,
                                           const uint8_t *pixels)
    {
        // Thanks to:
        // ftp://ftp.fu-berlin.de/unix/www/lynx/pub/shuford/terminal/all_about_sixels.txt

        // ? ... ~
        // - : LF (beginning of the next line)
        // $ : CR (beginning of the current line)
        // #10;2;r;g;b : color

        std::vector<std::string> escaped_lines;

        // 画像の大きさを指定する
        escaped_lines.emplace_back(";" + std::to_string(width) + ";" + std::to_string(height));

        // 縦6pixelごとに分割して処理する
        for (int ny = 0; ny < (height + 5) / 6; ny++) {
            int y = ny * 6;

            std::stringstream ss;
            std::vector<std::tuple<int, int, int, int, int>> colorpos;

            // Construct color2pos map.
            // 画像幅 x 6 の範囲について「その場所の色」を記録していく。
            // 色情報はα値分だけ薄めることとし、0-100に収める
            for (int x = 0; x < width; x++) {
                for (int i = 0; i < 6; i++) {
                    int pos = ((y + i) * width + x) * 4;
                    if (y + i >= height) break;
                    int red = pixels[pos + 0], green = pixels[pos + 1],
                        blue = pixels[pos + 2], alpha = pixels[pos + 3];

                    if (alpha == 0) continue;
                    red *= (alpha / 255.f) * 100 / 255;
                    green *= (alpha / 255.f) * 100 / 255;
                    blue *= (alpha / 255.f) * 100 / 255;

                    colorpos.push_back({red, green, blue, x, i});
                }
            }

            //  rgb順にソートして、同一色を一度に表示できるように
            std::sort(colorpos.begin(), colorpos.end());

            // Do actual printing.
            auto cpit = colorpos.begin();
            while (cpit != colorpos.end()) {
                int red = std::get<0>(*cpit), green = std::get<1>(*cpit),
                    blue = std::get<2>(*cpit);
                // 同一色の終端を探す
                auto end_cpit =
                    std::upper_bound(colorpos.begin(), colorpos.end(),
                                     std::tuple<int, int, int, int, int>(
                                         red, green, blue, INT_MAX, INT_MAX));
                // [cpit, end_cpit) は同一色

                // 同一行の先頭に戻る
                if (cpit != colorpos.begin())
                    ss << "$";
                // 10番目のパレットに色を設定する
                ss << "#0;2;" << red << ";" << green << ";" << blue;

                // 同一色であれば x 座標でソートされていることを利用する
                int val0 = 0, cnt = 0;
                for (int x = 0; x < width; x++) {
                    // x座標が同一の間、y座標の分だけフラグをたてる
                    int val = 0;
                    for (; cpit != end_cpit && std::get<3>(*cpit) == x; ++cpit)
                        val |= (1 << std::get<4>(*cpit));
                    val += '?';

                    // 同一の1x6パターンが続く場合はまとめて表示する
                    if (cnt == 0 || val == val0) {
                        val0 = val;
                        ++cnt;
                        continue;
                    }

                    // 異なる1x6パターンが来た場合には前のパターンを表示する
                    print_times(ss, val0, cnt);

                    // パターンのリセット
                    cnt = 1;
                    val0 = val;
                }
                // 最後の1x6パターンを表示する
                print_times(ss, val0, cnt);
            }

            // Go to the next line.
            ss << "-";

            escaped_lines.push_back(ss.str());
        }

        return escaped_lines;
    }

public:
    SixelImage(int width, int height, const uint8_t *pixels)
        : escaped_(escape(width, height, pixels))
    {
    }

    const std::vector<std::string> &getEscaped() const
    {
        return escaped_;
    }
};

class Sixel {
private:
    std::ostream &os_;

public:
    Sixel(std::ostream &os) : os_(os)
    {
    }

    void print(const SixelImage &image)
    {
        os_ << enter();

        const std::vector<std::string> &src = image.getEscaped();
        for (auto &&s : src) os_ << s;

        os_ << exit() << std::flush;
    }

private:
    static const char *enter()
    {
        // \33P : sixelの開始
        // 0; : p1 aspect ratio
        // 0; : p2 color of zero
        // 8  : p3 grid size parameter
        // q  : parameter end
        // "1;1 : 何だか不明
        // return "\033P0;0;8q\"1;1";
        return "\033Pq\"1;1";
    }

    static const char *exit()
    {
        // \033\ : sixelの終了
        return "\033\\";
    }
};

#endif
